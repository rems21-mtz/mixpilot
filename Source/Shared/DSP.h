#pragma once

// =============================================================================
//  MixPilot — briques DSP (sans allocation dans le thread audio)
// =============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>
#include "Profiles.h"

namespace mp
{
    inline float dbToGain (float db)  { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)   { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

    // -------------------------------------------------------------------------
    //  Biquad (RBJ) + calcul de réponse pour l'affichage
    // -------------------------------------------------------------------------
    struct BiquadCoeffs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

        enum Type { Peak, LowShelf, HighShelf, HighPass, LowPass };

        static BiquadCoeffs make (Type type, double sr, double f, double q, double gainDb)
        {
            BiquadCoeffs c;
            f = juce::jlimit (5.0, sr * 0.49, f);
            const double A  = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * f / sr;
            const double cw = std::cos (w0), sw = std::sin (w0);
            const double alpha = sw / (2.0 * q);
            double b0, b1, b2, a0, a1, a2;

            switch (type)
            {
                case Peak:
                    b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                    a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
                    break;
                case LowShelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    b0 =     A * ((A + 1) - (A - 1) * cw + s);
                    b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                    b2 =     A * ((A + 1) - (A - 1) * cw - s);
                    a0 =          (A + 1) + (A - 1) * cw + s;
                    a1 =    -2 * ((A - 1) + (A + 1) * cw);
                    a2 =          (A + 1) + (A - 1) * cw - s;
                    break;
                }
                case HighShelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    b0 =      A * ((A + 1) + (A - 1) * cw + s);
                    b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                    b2 =      A * ((A + 1) + (A - 1) * cw - s);
                    a0 =           (A + 1) - (A - 1) * cw + s;
                    a1 =      2 * ((A - 1) - (A + 1) * cw);
                    a2 =           (A + 1) - (A - 1) * cw - s;
                    break;
                }
                case HighPass:
                    b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
                    a0 = 1 + alpha;    a1 = -2 * cw;   a2 = 1 - alpha;
                    break;
                case LowPass:
                default:
                    b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
                    a0 = 1 + alpha;    a1 = -2 * cw; a2 = 1 - alpha;
                    break;
            }
            c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
            return c;
        }

        double magnitudeDb (double f, double sr) const
        {
            const double w = juce::MathConstants<double>::twoPi * f / sr;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
            const auto num = b0 + b1 * z1 + b2 * z2;
            const auto den = 1.0 + a1 * z1 + a2 * z2;
            return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
        }
    };

    struct BiquadState
    {
        double z1 = 0, z2 = 0;
        inline float process (const BiquadCoeffs& c, float xin) noexcept
        {
            const double x = xin;
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }
        void reset() { z1 = z2 = 0; }
    };

    // -------------------------------------------------------------------------
    //  EQ multi-bandes stéréo à gains lissés
    // -------------------------------------------------------------------------
    template <int N>
    class SmoothEQ
    {
    public:
        struct Band { BiquadCoeffs::Type type = BiquadCoeffs::Peak; float freq = 1000, q = 1, gainDb = 0; bool enabled = true; };

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& ch : state) for (auto& s : ch) s.reset();
            for (int i = 0; i < N; ++i) { current[i] = pending[i].load(); updateCoeffs (i); }
        }

        // Thread message : on prépare la cible, l'audio la rejoint en douceur
        void setBand (int i, BiquadCoeffs::Type type, float freq, float q, float gainDb, bool enabled = true)
        {
            pending[i].type.store ((int) type); pending[i].freq.store (freq); pending[i].q.store (q);
            pending[i].gainDb.store (gainDb);   pending[i].enabled.store (enabled);
        }

        void process (juce::AudioBuffer<float>& buf)
        {
            // Lissage par bloc (environ 50 ms)
            for (int i = 0; i < N; ++i)
            {
                const Band t = pending[i].load();
                Band& c = current[i];
                const bool changed = t.type != c.type || std::abs (t.freq - c.freq) > 0.01f || t.q != c.q
                                     || std::abs (t.gainDb - c.gainDb) > 0.001f || t.enabled != c.enabled;
                if (changed)
                {
                    const float k = 0.25f;
                    c.type = t.type; c.q = t.q; c.enabled = t.enabled;
                    c.freq   = std::abs (t.freq - c.freq) < 0.5f ? t.freq : c.freq + (t.freq - c.freq) * k;
                    c.gainDb = std::abs (t.gainDb - c.gainDb) < 0.01f ? t.gainDb : c.gainDb + (t.gainDb - c.gainDb) * k;
                    updateCoeffs (i);
                }
            }

            const int nCh = juce::jmin (buf.getNumChannels(), 2);
            for (int i = 0; i < N; ++i)
            {
                const Band& c = current[i];
                if (! c.enabled) continue;
                if ((c.type == BiquadCoeffs::Peak || c.type == BiquadCoeffs::LowShelf || c.type == BiquadCoeffs::HighShelf)
                    && std::abs (c.gainDb) < 0.01f) continue;

                for (int ch = 0; ch < nCh; ++ch)
                {
                    auto* d = buf.getWritePointer (ch);
                    auto& st = state[ch][i];
                    for (int n = 0; n < buf.getNumSamples(); ++n)
                        d[n] = st.process (coeffs[i], d[n]);
                }
            }
        }

        // Réponse en dB de la cible (pour l'affichage)
        float responseDb (float f) const
        {
            double sum = 0;
            for (int i = 0; i < N; ++i)
            {
                const Band b = pending[i].load();
                if (! b.enabled) continue;
                sum += BiquadCoeffs::make (b.type, sr, b.freq, b.q, b.gainDb).magnitudeDb (f, sr);
            }
            return (float) sum;
        }

        Band getTarget (int i) const { return pending[i].load(); }

    private:
        void updateCoeffs (int i)
        {
            coeffs[i] = BiquadCoeffs::make (current[i].type, sr, current[i].freq, current[i].q, current[i].gainDb);
        }

        struct AtomicBand
        {
            std::atomic<int> type { (int) BiquadCoeffs::Peak };
            std::atomic<float> freq { 1000.0f }, q { 1.0f }, gainDb { 0.0f };
            std::atomic<bool> enabled { true };
            Band load() const { return { (BiquadCoeffs::Type) type.load(), freq.load(), q.load(), gainDb.load(), enabled.load() }; }
        };

        double sr = 48000.0;
        AtomicBand pending[N];
        Band current[N];
        BiquadCoeffs coeffs[N];
        BiquadState state[2][N];
    };

    // Fréquence de base d'une bande du match EQ (les extrêmes sont des shelves)
    inline float octaveEQFreq (int b)
    {
        return b == 0 ? 50.0f : (b == kBands - 1 ? 11000.0f : kBandFreqs[(size_t) b]);
    }

    // Déplacement manuel maximal d'une bande (en octaves, de part et d'autre)
    constexpr float kMaxBandShiftOct = 0.75f;

    // Filtres de "match EQ" sur les 10 bandes d'octave (shiftOct : décalage manuel de chaque bande)
    template <int N>
    inline void configureOctaveEQ (SmoothEQ<N>& eq, int firstIndex, const Bands& gains, bool enabled = true, const Bands& shiftOct = {})
    {
        static_assert (N >= kBands, "EQ too small");
        for (int b = 0; b < kBands; ++b)
        {
            const auto type = b == 0 ? BiquadCoeffs::LowShelf : (b == kBands - 1 ? BiquadCoeffs::HighShelf : BiquadCoeffs::Peak);
            const float f   = juce::jlimit (20.0f, 20000.0f, octaveEQFreq (b) * std::exp2 (shiftOct[(size_t) b]));
            const float q   = b == 0 || b == kBands - 1 ? 0.7f : 1.1f;
            eq.setBand (firstIndex + b, type, f, q, gains[(size_t) b], enabled);
        }
    }

    // -------------------------------------------------------------------------
    //  Compresseur stéréo lié, genou doux
    // -------------------------------------------------------------------------
    class Compressor
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; env = 0.0f; ms = 0.0f; updateTimes(); }
        void setRms (bool useRms) { rms = useRms; }

        void setParams (float thresholdDb, float ratio_, float attackMs, float releaseMs, float kneeDb = 6.0f, float makeupDb = 0.0f)
        {
            threshold = thresholdDb; ratio = std::max (1.0f, ratio_); knee = kneeDb; makeup = dbToGain (makeupDb);
            if (attackMs != attMs || releaseMs != relMs) { attMs = attackMs; relMs = releaseMs; updateTimes(); }
        }

        // Retourne le gain (linéaire) à appliquer pour un niveau d'entrée donné (dB)
        inline float computeGain (float levelDb) noexcept
        {
            const float over = levelDb - threshold;
            float grDb = 0.0f;
            if (over > knee * 0.5f)            grDb = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f)      { const float x = over + knee * 0.5f; grDb = (1.0f - 1.0f / ratio) * x * x / (2.0f * knee); }

            const float coeff = grDb > env ? attCoeff : relCoeff;
            env = grDb + coeff * (env - grDb);
            return dbToGain (-env);
        }

        void process (juce::AudioBuffer<float>& buf, float amount = 1.0f)
        {
            const int nCh = juce::jmin (2, buf.getNumChannels());
            float maxGr = 0.0f;
            for (int n = 0; n < buf.getNumSamples(); ++n)
            {
                float peak = 0.0f;
                for (int ch = 0; ch < nCh; ++ch) peak = std::max (peak, std::abs (buf.getSample (ch, n)));
                float level = peak;
                if (rms) { ms = peak * peak + rmsCoeff * (ms - peak * peak); level = std::sqrt (ms) * 1.414f; }
                const float g  = computeGain (gainToDb (level));
                const float ga = (1.0f + amount * (g - 1.0f)) * (1.0f + amount * (makeup - 1.0f));
                for (int ch = 0; ch < nCh; ++ch) buf.getWritePointer (ch)[n] *= ga;
                maxGr = std::max (maxGr, env * amount);
            }
            gainReductionDb.store (maxGr);
        }

        float getEnvDb() const { return env; }
        std::atomic<float> gainReductionDb { 0.0f };

    private:
        void updateTimes()
        {
            attCoeff = std::exp (-1.0f / (float) (std::max (0.05f, attMs) * 0.001 * sr));
            relCoeff = std::exp (-1.0f / (float) (std::max (1.0f,  relMs) * 0.001 * sr));
            rmsCoeff = std::exp (-1.0f / (float) (0.010 * sr));
        }

        bool rms = false;
        float ms = 0.0f, rmsCoeff = 0.99f;
        double sr = 48000.0;
        float threshold = 0.0f, ratio = 1.0f, knee = 6.0f, makeup = 1.0f;
        float attMs = 10.0f, relMs = 100.0f, attCoeff = 0.0f, relCoeff = 0.0f;
        float env = 0.0f;
    };

    // -------------------------------------------------------------------------
    //  Limiteur "lookahead" sans dépassement (min-hold + moyenne glissante)
    // -------------------------------------------------------------------------
    class LookaheadLimiter
    {
    public:
        void prepare (double sampleRate, int numChannels, float lookaheadMs = 1.5f)
        {
            sr = sampleRate;
            L  = std::max (2, (int) std::round (lookaheadMs * 0.001 * sr));
            nCh = numChannels;
            delay.assign ((size_t) (nCh * L), 0.0f);
                        boxRing.assign ((size_t) L, 1.0f);
            dqIdx.assign ((size_t) L + 2, 0); dqVal.assign ((size_t) L + 2, 1.0f);
            dqHead = dqTail = 0; pos = 0; t = 0; boxSum = (double) L; rel = 1.0f;
            setRelease (relMs);
        }

        int getLatency() const { return L; }
        void setCeiling (float db) { ceiling = dbToGain (db); }
        void setRelease (float ms)  { relMs = ms; relCoeff = std::exp (-1.0f / (float) (std::max (1.0f, ms) * 0.001 * sr)); }

        // Traite des pointeurs de canaux (sur place). "level" (optionnel) = crête true peak par échantillon
        void process (float* const* ch, int numSamples, const float* level = nullptr)
        {
            float minGain = 1.0f;
            for (int n = 0; n < numSamples; ++n)
            {
                float peak = 0.0f;
                if (level != nullptr) peak = level[n];
                else for (int c = 0; c < nCh; ++c) peak = std::max (peak, std::abs (ch[c][n]));
                const float g = peak > ceiling ? ceiling / peak : 1.0f;

                // Minimum glissant sur L+1 échantillons (file monotone)
                while (dqTail != dqHead && dqVal[(size_t) ((dqTail + (int) dqVal.size() - 1) % (int) dqVal.size())] >= g)
                    dqTail = (dqTail + (int) dqVal.size() - 1) % (int) dqVal.size();
                dqVal[(size_t) dqTail] = g; dqIdx[(size_t) dqTail] = t;
                dqTail = (dqTail + 1) % (int) dqVal.size();
                while (dqIdx[(size_t) dqHead] <= t - L - 1) dqHead = (dqHead + 1) % (int) dqVal.size();
                const float held = dqVal[(size_t) dqHead];

                // Attaque instantanée, relâchement progressif
                rel = held < rel ? held : held + relCoeff * (rel - held);

                // Moyenne glissante sur L échantillons
                boxSum += rel - boxRing[(size_t) pos];
                boxRing[(size_t) pos] = rel;
                const float gain = (float) std::min (1.0, boxSum / L);

                for (int c = 0; c < nCh; ++c)
                {
                    float& slot = delay[(size_t) (c * L + pos)];
                    const float delayed = slot;
                    slot = ch[c][n];
                    ch[c][n] = delayed * gain;
                }
                minGain = std::min (minGain, gain);
                pos = (pos + 1) % L; ++t;
            }
            gainReductionDb.store (-gainToDb (minGain));
        }

        std::atomic<float> gainReductionDb { 0.0f };

    private:
        double sr = 48000.0;
        int L = 64, nCh = 2, pos = 0;
        int64_t t = 0;
        float ceiling = 1.0f, rel = 1.0f, relMs = 100.0f, relCoeff = 0.99f;
        double boxSum = 0;
        std::vector<float> delay, boxRing, dqVal;
        std::vector<int64_t> dqIdx;
        int dqHead = 0, dqTail = 0;
    };

    // -------------------------------------------------------------------------
    //  Détecteur true peak (interpolation x8, sinc fenêtré Kaiser de 257 points).
    //  Retarde le signal de 16 échantillons et fournit, pour chaque échantillon,
    //  la crête "entre les échantillons" correspondante.
    // -------------------------------------------------------------------------
    class TruePeakDetector
    {
    public:
        static constexpr int kPhases = 8, kTaps = 8 * 32 + 1, kDelay = 16, kHist = 33;

        TruePeakDetector()
        {
            auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
            const double beta = 8.0, mid = (kTaps - 1) / 2.0;
            for (int k = 0; k < kTaps; ++k)
            {
                const double x = (k - mid) / (double) kPhases;
                const double sinc = x == 0 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
                const double r = (k - mid) / mid;
                const double w = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
                h[k] = (float) (sinc * w);
            }
            // y_p = somme_j x[n-j] * h[kPhases*j + p]  ->  fenêtre ordonnée ancien->récent : m = kHist-1-j
            for (int p = 0; p < kPhases; ++p)
                for (int j = 0; j < kHist; ++j)
                {
                    const int k = kPhases * j + p;
                    taps[(size_t) p][(size_t) (kHist - 1 - j)] = k < kTaps ? h[k] : 0.0f;
                }
        }

        void reset() { for (auto& c : hist) c.fill (0.0f); pos = 0; }

        // ch : signal (modifié : retardé de kDelay), level : crête par échantillon (max des canaux)
        void process (float* const* ch, int numCh, int numSamples, float* level)
        {
            numCh = std::min (numCh, 2);
            for (int i = 0; i < numSamples; ++i)
            {
                float peak = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    auto& hb = hist[(size_t) c];
                    // tampon doublé : la fenêtre des kHist derniers échantillons est toujours contiguë
                    hb[(size_t) pos] = hb[(size_t) (pos + kHist)] = ch[c][i];
                    const float* win = hb.data() + pos + 1;     // du plus ancien au plus récent
                    for (int p = 0; p < kPhases; ++p)
                    {
                        const float* t = taps[p].data();
                        float y = 0.0f;
                        for (int m = 0; m < kHist; ++m) y += win[m] * t[m];
                        peak = std::max (peak, std::abs (y));
                    }
                    ch[c][i] = win[kHist - 1 - kDelay];          // signal aligné (retard kDelay)
                }
                level[i] = peak;
                pos = (pos + 1) % kHist;
            }
        }

    private:
        float h[kTaps] {};
        std::array<std::array<float, kHist>, kPhases> taps {};   // coefficients par phase, ordre "ancien → récent"
        std::array<std::array<float, 2 * kHist>, 2> hist {};
        int pos = 0;
    };

    // -------------------------------------------------------------------------
    //  Mesure de loudness ITU-R BS.1770 (LUFS) : momentané, court terme,
    //  intégré (histogramme sans verrou, calculé côté interface).
    // -------------------------------------------------------------------------
    class LoudnessMeter
    {
    public:
        static constexpr int kHistBins = 900;   // -80 .. +10 LUFS, pas de 0,1

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            const auto shelf = BiquadCoeffs::make (BiquadCoeffs::HighShelf, sr, 1681.974450955533, 0.7071752369554196, 3.999843853973347);
            const auto hp    = BiquadCoeffs::make (BiquadCoeffs::HighPass,  sr, 38.13547087602444,  0.5003270373238773, 0.0);
            k1 = shelf; k2 = hp;
            for (auto& s : st) s.reset();
            blockLen = (int) std::round (sr * 0.1);
            acc = 0; accN = 0; ringPos = 0; filled = 0;
            for (auto& r : ring) r = 0.0;
            resetIntegrated();
        }

        void resetIntegrated() { resetRequest.store (true); }

        void process (const float* const* ch, int numCh, int numSamples)
        {
            if (resetRequest.exchange (false))
                for (auto& h : hist) h.store (0, std::memory_order_relaxed);

            numCh = std::min (numCh, 2);
            for (int n = 0; n < numSamples; ++n)
            {
                double sum = 0;
                for (int c = 0; c < numCh; ++c)
                {
                    const float y = st[c * 2 + 1].process (k2, st[c * 2].process (k1, ch[c][n]));
                    sum += (double) y * y;
                }
                if (numCh == 1) sum *= 2.0;   // mono = deux canaux identiques
                acc += sum;
                if (++accN >= blockLen) pushBlock();
            }
        }

        float getMomentary() const { return momentary.load(); }
        float getShortTerm() const { return shortTerm.load(); }

        // Intégré avec double porte (-70 LUFS absolu, -10 LU relatif)
        float getIntegrated() const
        {
            double e = 0; uint64_t cnt = 0;
            for (int i = 0; i < kHistBins; ++i)
            {
                const uint32_t c = hist[i].load (std::memory_order_relaxed);
                if (c == 0) continue;
                const double L = binToLufs (i);
                if (L < -70.0) continue;
                e += c * std::pow (10.0, (L + 0.691) / 10.0); cnt += c;
            }
            if (cnt == 0) return -70.0f;
            const double rel = -0.691 + 10.0 * std::log10 (e / (double) cnt) - 10.0;
            e = 0; cnt = 0;
            for (int i = 0; i < kHistBins; ++i)
            {
                const uint32_t c = hist[i].load (std::memory_order_relaxed);
                const double L = binToLufs (i);
                if (c == 0 || L < -70.0 || L < rel) continue;
                e += c * std::pow (10.0, (L + 0.691) / 10.0); cnt += c;
            }
            if (cnt == 0) return -70.0f;
            return (float) (-0.691 + 10.0 * std::log10 (e / (double) cnt));
        }

        int getIntegratedBlockCount() const
        {
            int n = 0;
            for (auto& h : hist) n += (int) h.load (std::memory_order_relaxed);
            return n;
        }

    private:
        static double binToLufs (int i) { return -80.0 + (i + 0.5) * 0.1; }

        void pushBlock()
        {
            ring[(size_t) ringPos] = acc / std::max (1, accN);
            ringPos = (ringPos + 1) % 30;
            filled = std::min (filled + 1, 30);
            acc = 0; accN = 0;

            auto meanOf = [this] (int count)
            {
                double s = 0; const int c = std::min (count, filled);
                for (int k = 1; k <= c; ++k) s += ring[(size_t) ((ringPos - k + 30) % 30)];
                return c > 0 ? s / c : 0.0;
            };
            auto toLufs = [] (double ms) { return ms > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : -100.0f; };

            const float m = toLufs (meanOf (4));
            momentary.store (m);
            shortTerm.store (toLufs (meanOf (30)));

            if (filled >= 4 && m > -80.0f)
            {
                const int bin = juce::jlimit (0, kHistBins - 1, (int) ((m + 80.0f) / 0.1f));
                hist[bin].fetch_add (1, std::memory_order_relaxed);
            }
        }

        double sr = 48000.0;
        BiquadCoeffs k1, k2;
        BiquadState st[4];
        int blockLen = 4800, accN = 0, ringPos = 0, filled = 0;
        double acc = 0;
        std::array<double, 30> ring {};
        std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f };
        std::atomic<bool> resetRequest { true };
        std::array<std::atomic<uint32_t>, kHistBins> hist {};
    };

    // -------------------------------------------------------------------------
    //  Détecteur de crête (true-peak approché par suréchantillonnage x4)
    // -------------------------------------------------------------------------
    class PeakHold
    {
    public:
        void push (float v) { float cur = peak.load(); while (v > cur && ! peak.compare_exchange_weak (cur, v)) {} }
        float takeDb()      { return gainToDb (peak.exchange (0.0f)); }
    private:
        std::atomic<float> peak { 0.0f };
    };
}
