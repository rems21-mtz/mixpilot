#pragma once

// =============================================================================
//  MixPilot — analyse spectrale, apprentissage et détection d'instrument
// =============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <deque>
#include <cmath>
#include "Profiles.h"

namespace mp
{
    struct LearnResult
    {
        bool  valid = false;
        float activeSeconds = 0;
        Bands bandsDb {};                       // énergie par octave (dB)
        float low = 0, lowmid = 0, mid = 0, high = 0;   // répartition (0..1)
        float centroidHz = 1000;
        float onsetRate = 0;                    // attaques par seconde
        float sustain = 0;                      // 0 = très percussif, 1 = tenu
        float modulation = 0;                   // variation du niveau (dB)
        float flatness = 0;                     // 0 = tonal, 1 = bruit
        float attackFrames = 3;                 // durée moyenne d'attaque (trames de 21 ms)
        float meanFrameDb = -30;                // niveau RMS moyen des passages actifs (dBFS)
    };

    struct Detection { int instrument = Other; float confidence = 0; };

    // -------------------------------------------------------------------------
    class SpectrumAnalyzer
    {
    public:
        static constexpr int fftOrder = 11;
        static constexpr int fftSize  = 1 << fftOrder;   // 2048
        static constexpr int hop      = fftSize / 2;

        SpectrumAnalyzer() : fft (fftOrder), window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false)
        {
            fifoData.resize (1 << 16);
            fifo.setTotalSize ((int) fifoData.size());
            history.assign ((size_t) fftSize, 0.0f);
            fftBuf.assign ((size_t) fftSize * 2, 0.0f);
            display.assign ((size_t) fftSize / 2, -120.0f);
        }

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            fifo.reset();
            bandOfBin.assign ((size_t) fftSize / 2, -1);
            for (int k = 1; k < fftSize / 2; ++k)
            {
                const double f = k * sr / fftSize;
                for (int b = 0; b < kBands; ++b)
                    if (f >= kBandFreqs[(size_t) b] / std::sqrt (2.0) && f < kBandFreqs[(size_t) b] * std::sqrt (2.0))
                        bandOfBin[(size_t) k] = b;
            }
        }

        // ---- thread audio ----
        void push (const float* const* ch, int numCh, int numSamples)
        {
            int s1, n1, s2, n2;
            fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
            auto write = [&] (int start, int count, int offset)
            {
                for (int i = 0; i < count; ++i)
                {
                    float v = 0;
                    for (int c = 0; c < numCh; ++c) v += ch[c][offset + i];
                    fifoData[(size_t) (start + i)] = v / (float) std::max (1, numCh);
                }
            };
            write (s1, n1, 0);
            write (s2, n2, n1);
            fifo.finishedWrite (n1 + n2);
        }

        // ---- thread message (timer) ----
        void update()
        {
            while (fifo.getNumReady() >= hop)
            {
                int s1, n1, s2, n2;
                fifo.prepareToRead (hop, s1, n1, s2, n2);
                std::memmove (history.data(), history.data() + hop, sizeof (float) * (size_t) (fftSize - hop));
                std::memcpy (history.data() + fftSize - hop, fifoData.data() + s1, sizeof (float) * (size_t) n1);
                if (n2 > 0) std::memcpy (history.data() + fftSize - hop + n1, fifoData.data() + s2, sizeof (float) * (size_t) n2);
                fifo.finishedRead (n1 + n2);
                processFrame();
            }
        }

        const std::vector<float>& getDisplayDb() const { return display; }

        // spectre par octave moyenné sur ~4 s (passages joués uniquement), pour les bilans
        Bands getLongTermBandsDb() const
        {
            Bands b;
            for (int k = 0; k < kBands; ++k) b[(size_t) k] = (float) (10.0 * std::log10 (std::max (ltPow[(size_t) k], 1e-20)));
            return b;
        }
        bool hasLongTerm() const { return ltFrames > 40; }
        double getSampleRate() const { return sr; }

        void startLearning()
        {
            learning = true;
            bandPow.fill (0.0); learnFrames = 0; activeFrames = 0; onsets = 0; sustained = 0;
            sumDb = 0; sumDb2 = 0; recent.clear(); refractory = 0;
            sumFlat = 0; sumAttack = 0; attackCount = 0; attackCounting = false; attackFramesCur = 0; lastDb = -200;
        }
        void stopLearning() { learning = false; }
        bool isLearning() const { return learning; }

        LearnResult getResult() const
        {
            LearnResult r;
            const double secPerFrame = hop / sr;
            r.activeSeconds = (float) (activeFrames * secPerFrame);
            if (activeFrames < 10) return r;

            double tot = 0;
            for (int b = 0; b < kBands; ++b) tot += bandPow[(size_t) b];
            if (tot <= 0) return r;

            double logC = 0;
            for (int b = 0; b < kBands; ++b)
            {
                const double p = bandPow[(size_t) b] / activeFrames;
                r.bandsDb[(size_t) b] = (float) (10.0 * std::log10 (std::max (p, 1e-20)));
                logC += bandPow[(size_t) b] / tot * std::log2 (kBandFreqs[(size_t) b]);
            }
            auto frac = [&] (int a, int b) { double s = 0; for (int i = a; i <= b; ++i) s += bandPow[(size_t) i]; return (float) (s / tot); };
            r.low = frac (0, 2); r.lowmid = frac (3, 4); r.mid = frac (5, 6); r.high = frac (7, 9);
            r.centroidHz = (float) std::pow (2.0, logC);
            r.onsetRate  = (float) (onsets / (activeFrames * secPerFrame));
            r.sustain    = (float) sustained / (float) activeFrames;
            const double mean = sumDb / activeFrames;
            r.meanFrameDb = (float) mean;
            r.modulation = (float) std::sqrt (std::max (0.0, sumDb2 / activeFrames - mean * mean));
            r.flatness   = (float) (sumFlat / activeFrames);
            r.attackFrames = attackCount > 0 ? (float) (sumAttack / attackCount) : 6.0f;
            r.valid = true;
            return r;
        }

    private:
        void processFrame()
        {
            // Niveau du dernier "hop"
            double ms = 0;
            for (int i = fftSize - hop; i < fftSize; ++i) ms += (double) history[(size_t) i] * history[(size_t) i];
            const float frameDb = (float) (10.0 * std::log10 (std::max (ms / hop, 1e-12)));

            std::copy (history.begin(), history.end(), fftBuf.begin());
            std::fill (fftBuf.begin() + fftSize, fftBuf.end(), 0.0f);
            window.multiplyWithWindowingTable (fftBuf.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);

            const float norm = 4.0f / (float) fftSize;   // Hann : gain cohérent 0,5
            for (int k = 0; k < fftSize / 2; ++k)
            {
                const float db = 20.0f * std::log10 (std::max (fftBuf[(size_t) k] * norm, 1e-7f));
                float& d = display[(size_t) k];
                d = db > d ? d + (db - d) * 0.6f : d + (db - d) * 0.12f;
            }

            // moyenne longue durée par octave (toujours active)
            if (frameDb > -60.0f)
            {
                std::array<double, kBands> fp {};
                for (int k = 1; k < fftSize / 2; ++k)
                {
                    const int b = bandOfBin[(size_t) k];
                    if (b >= 0) fp[(size_t) b] += (double) fftBuf[(size_t) k] * fftBuf[(size_t) k];
                }
                const double a = ltFrames < 40 ? 1.0 / (ltFrames + 1) : 0.005;
                for (int b = 0; b < kBands; ++b) ltPow[(size_t) b] += a * (fp[(size_t) b] - ltPow[(size_t) b]);
                ++ltFrames;
            }

            if (! learning) return;
            ++learnFrames;

            recent.push_back (frameDb);
            if (recent.size() > 20) recent.pop_front();

            if (frameDb < -55.0f) return;   // silence : ignoré
            ++activeFrames;
            sumDb += frameDb; sumDb2 += (double) frameDb * frameDb;

            double logSum = 0, linSum = 0; int nFlat = 0;
            for (int k = 1; k < fftSize / 2; ++k)
            {
                const double p = (double) fftBuf[(size_t) k] * fftBuf[(size_t) k];
                const int b = bandOfBin[(size_t) k];
                if (b >= 0) bandPow[(size_t) b] += p;
                const double f = k * sr / fftSize;
                if (f >= 200.0 && f <= 10000.0) { logSum += std::log (p + 1e-20); linSum += p; ++nFlat; }
            }
            if (nFlat > 0 && linSum > 0)
                sumFlat += std::exp (logSum / nFlat) / (linSum / nFlat);

            // durée d'attaque : nombre de trames où le niveau continue de monter après une attaque
            if (attackCounting)
            {
                if (frameDb > lastDb + 0.5f && attackFramesCur < 12) { ++attackFramesCur; lastDb = frameDb; }
                else { sumAttack += attackFramesCur; ++attackCount; attackCounting = false; }
            }

            float recentMax = -200.0f, recentMin = 200.0f;
            for (size_t i = 0; i < recent.size(); ++i) recentMax = std::max (recentMax, recent[i]);
            for (size_t i = recent.size() >= 4 ? recent.size() - 4 : 0; i + 1 < recent.size(); ++i) recentMin = std::min (recentMin, recent[i]);

            if (refractory > 0) --refractory;
            else if (recent.size() >= 4 && frameDb - recentMin > 9.0f)
            {
                ++onsets; refractory = 4;
                attackCounting = true; attackFramesCur = 1; lastDb = frameDb;
            }
            if (frameDb > recentMax - 6.0f && recent.size() >= 10)
            {
                // tenu = le niveau reste proche du max récent sur toute la fenêtre
                float minWin = 200.0f;
                for (auto v : recent) minWin = std::min (minWin, v);
                if (recentMax - minWin < 8.0f) ++sustained;
            }
        }

        double sr = 48000.0;
        juce::dsp::FFT fft;
        juce::dsp::WindowingFunction<float> window;
        juce::AbstractFifo fifo { 1 << 16 };
        std::vector<float> fifoData, history, fftBuf, display;
        std::vector<int> bandOfBin;

        bool learning = false;
        std::array<double, kBands> bandPow {}, ltPow {};
        int ltFrames = 0;
        int learnFrames = 0, activeFrames = 0, onsets = 0, sustained = 0, refractory = 0;
        double sumDb = 0, sumDb2 = 0, sumFlat = 0, sumAttack = 0;
        int attackCount = 0, attackFramesCur = 0;
        bool attackCounting = false;
        float lastDb = -200;
        std::deque<float> recent;
    };

    // Caractère percussif (0..1) : attaques fréquentes ET rapides
    inline float percussiveness (const LearnResult& r)
    {
        const float onsetN = juce::jlimit (0.0f, 1.0f, r.onsetRate / 2.5f);
        const float sharp  = juce::jlimit (0.0f, 1.0f, (3.5f - r.attackFrames) / 2.5f);
        return onsetN * sharp * (1.0f - 0.7f * r.sustain);
    }

    // -------------------------------------------------------------------------
    //  Détection d'instrument par scores (heuristique explicable)
    // -------------------------------------------------------------------------
    inline Detection classify (const LearnResult& r)
    {
        Detection d;
        if (! r.valid) return d;

        const float perc   = percussiveness (r);
        const float body   = r.lowmid + r.mid;
        const float modN   = juce::jlimit (0.0f, 1.0f, (r.modulation - 3.0f) / 8.0f);
        const float bright = juce::jlimit (0.0f, 1.0f, (std::log2 (r.centroidHz) - std::log2 (500.0f)) / 4.0f);
        const float noisy  = juce::jlimit (0.0f, 1.0f, (r.flatness - 0.08f) / 0.25f);
        const float tonal  = 1.0f - noisy;

        float s[NumInstruments] {};
        s[Kick]   = 1.8f * r.low + 0.3f * perc + 2.0f * modN - 0.6f * r.sustain - 0.8f * r.high - 0.4f;
        s[Bass]   = 1.8f * r.low + 0.6f * r.sustain - 1.0f * modN - 0.8f * r.high + 0.5f;
        s[Snare]  = 1.0f * body + 1.4f * perc + 0.8f * noisy - 1.2f * r.low + 0.3f * r.high - 0.5f;
        s[Hats]   = 2.0f * r.high + 0.8f * perc + 0.5f * noisy - 1.5f * r.low - 0.4f * body;
        s[Vocal]  = 1.3f * body + 0.9f * modN + 0.8f * tonal - 1.2f * r.low - 0.8f * perc - 0.4f * r.sustain - 0.2f;
        s[Lead]   = 1.0f * body + 0.5f * r.high + 0.5f * r.sustain + 0.3f * bright + 0.2f * tonal - 1.0f * r.low - 0.4f * perc - 0.3f;
        s[Pad]    = 0.9f * body + 0.9f * r.sustain + 0.3f * tonal - 0.9f * perc - 0.3f * modN - 0.4f * r.low - 0.1f * bright - 0.25f;
        s[Guitar] = 1.2f * body + 0.3f * perc + 0.2f * modN - 0.6f * r.high - 0.8f * r.low - 0.3f * bright - 0.25f;
        s[Other]  = 0.15f;
        // Instru complète : du contenu dans TOUTES les octaves (graves, médiums et aigus à moins de ~30 dB du maximum),
        // avec des graves bien présents. Un kick ou une basse n'ont presque rien dans les aigus ; une caisse claire n'a pas de graves.
        {
            float mx = -200; for (auto v : r.bandsDb) mx = std::max (mx, v);
            auto cover = [&] (int b0, int b1, float range)
            {
                float avg = 0; for (int k = b0; k <= b1; ++k) avg += r.bandsDb[(size_t) k]; avg /= (float) (b1 - b0 + 1);
                return juce::jlimit (0.0f, 1.0f, (avg - (mx - range)) / 20.0f);
            };
            const float hiCover  = cover (7, 8, 35.0f);   // 4 – 8 kHz
            const float midCover = cover (4, 6, 30.0f);   // 500 Hz – 2 kHz
            const float lowTerm  = juce::jlimit (0.0f, 1.0f, (r.low - 0.2f) / 0.3f);
            s[Instrumental] = 2.6f * hiCover + 1.2f * midCover + 1.6f * lowTerm - 2.0f;
        }
        s[Acapella] = -10.0f;   // jamais choisi automatiquement : une voix est détectée comme « Voix »

        int best = Other, second = Other;
        for (int i = 0; i < NumInstruments; ++i)
            if (s[i] > s[best]) best = i;
        float secondScore = -1e9f;
        for (int i = 0; i < NumInstruments; ++i)
            if (i != best && s[i] > secondScore) { secondScore = s[i]; second = i; }

        juce::ignoreUnused (second);
        d.instrument = best;
        d.confidence = juce::jlimit (0.3f, 0.99f, 0.5f + (s[best] - secondScore) * 1.5f);
        return d;
    }

    // Supprime la moyenne pondérée (pour comparer des formes, pas des niveaux)
    inline Bands normalised (const Bands& b, const Bands& w)
    {
        double s = 0, ws = 0;
        for (int i = 0; i < kBands; ++i) { s += b[(size_t) i] * w[(size_t) i]; ws += w[(size_t) i]; }
        const float mean = ws > 0 ? (float) (s / ws) : 0.0f;
        Bands out;
        for (int i = 0; i < kBands; ++i) out[(size_t) i] = b[(size_t) i] - mean;
        return out;
    }
}
