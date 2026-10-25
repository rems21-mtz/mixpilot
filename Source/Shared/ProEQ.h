#pragma once

// =============================================================================
//  MixPilot — EQ libre (12 bandes) : cloche, shelves, coupe-bas/haut, notch,
//  stéréo / Mid / Side / gauche / droite, EQ dynamique et écoute en solo.
//  Placé après l'EQ de l'analyse, dans Track comme dans Master.
// =============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include "ProEqModel.h"

namespace mp
{
    class ProEQ
    {
    public:
        static constexpr int kMaxStages = 4;
        static constexpr int kSub = 32;       // sous-bloc de mise à jour des coefficients

        // ---- paramètres -----------------------------------------------------
        static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l, int version)
        {
            using namespace juce;
            auto txt = [] (std::function<String (float)> f) { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
            auto range = [] (float lo, float hi, float centre, float step = 0.0f)
            {
                NormalisableRange<float> r (lo, hi, step);
                r.setSkewForCentre (centre);
                return r;
            };
            auto hz  = txt ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
            auto dB  = txt ([] (float v) { return (v > 0.005f ? "+" : "") + String (v, 1) + " dB"; });
            auto q   = txt ([] (float v) { return String (v, 2); });
            auto ms  = txt ([] (float v) { return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; });
            auto rat = txt ([] (float v) { return String (v, 1) + ":1"; });

            StringArray types, chans, slopes;
            for (int t = 0; t < NumProTypes; ++t)    types.add (proTypeName (t));
            for (int c = 0; c < NumProChannels; ++c) chans.add (proChannelName (c));
            for (int s = 0; s < 4; ++s)              slopes.add (proSlopeName (s));

            static const float defFreq[kProBands] = { 80, 160, 320, 640, 1250, 2500, 5000, 10000, 120, 450, 1800, 7000 };
            for (int k = 0; k < kProBands; ++k)
            {
                const String n = "Band " + String (k + 1) + " ";
                auto id = [k, version] (ProField f) { return ParameterID { proParamId (k, f), version }; };
                l.add (std::make_unique<AudioParameterBool>   (id (FOn),     n + "used",    false));
                l.add (std::make_unique<AudioParameterBool>   (id (FActive), n + "on",      true));
                l.add (std::make_unique<AudioParameterChoice> (id (FType),   n + "type",    types, PBell));
                l.add (std::make_unique<AudioParameterFloat>  (id (FFreq),   n + "freq",    range (20, 20000, 640, 0.01f), defFreq[k], hz));
                l.add (std::make_unique<AudioParameterFloat>  (id (FGain),   n + "gain",    NormalisableRange<float> (-18, 18, 0.01f), 0.0f, dB));
                l.add (std::make_unique<AudioParameterFloat>  (id (FQ),      n + "Q",       range (0.1f, 18, 1, 0.001f), 1.0f, q));
                l.add (std::make_unique<AudioParameterChoice> (id (FSlope),  n + "slope",   slopes, 1));
                l.add (std::make_unique<AudioParameterChoice> (id (FCh),     n + "channel", chans, PStereo));
                l.add (std::make_unique<AudioParameterBool>   (id (FDyn),    n + "dynamic", false));
                l.add (std::make_unique<AudioParameterFloat>  (id (FThr),    n + "threshold", NormalisableRange<float> (-60, 0, 0.1f), -24.0f, dB));
                l.add (std::make_unique<AudioParameterFloat>  (id (FRange),  n + "range",   NormalisableRange<float> (-18, 18, 0.01f), -6.0f, dB));
                l.add (std::make_unique<AudioParameterFloat>  (id (FAtt),    n + "attack",  range (0.1f, 200, 10, 0.01f), 10.0f, ms));
                l.add (std::make_unique<AudioParameterFloat>  (id (FRel),    n + "release", range (5, 2000, 150, 0.1f), 120.0f, ms));
                l.add (std::make_unique<AudioParameterFloat>  (id (FRatio),  n + "ratio",   range (1, 20, 3, 0.01f), 2.0f, rat));
            }
        }

        void attach (juce::AudioProcessorValueTreeState& s)
        {
            for (int k = 0; k < kProBands; ++k)
                for (int f = 0; f < NumProFields; ++f)
                    raw[k][f] = s.getRawParameterValue (proParamId (k, (ProField) f));
        }

        ProBand readBand (int k) const
        {
            auto v = [this, k] (ProField f) { return raw[k][f]->load(); };
            ProBand b;
            b.on = v (FOn) > 0.5f;   b.active = v (FActive) > 0.5f;
            b.type = juce::roundToInt (v (FType)); b.freq = v (FFreq); b.gainDb = v (FGain); b.q = v (FQ);
            b.slope = juce::roundToInt (v (FSlope)); b.ch = juce::roundToInt (v (FCh)); b.dyn = v (FDyn) > 0.5f;
            b.thr = v (FThr); b.range = v (FRange); b.att = v (FAtt); b.rel = v (FRel); b.ratio = v (FRatio);
            return b;
        }

        // ---- conception des filtres -----------------------------------------
        static int design (int type, int slope, double sr, float freq, float q, float gainDb, BiquadCoeffs* out)
        {
            switch (type)
            {
                case PLowShelf:  out[0] = BiquadCoeffs::make (BiquadCoeffs::LowShelf,  sr, freq, juce::jmin (q, 4.0f), gainDb); return 1;
                case PHighShelf: out[0] = BiquadCoeffs::make (BiquadCoeffs::HighShelf, sr, freq, juce::jmin (q, 4.0f), gainDb); return 1;
                case PNotch:     out[0] = BiquadCoeffs::make (BiquadCoeffs::Notch,     sr, freq, q, 0.0); return 1;
                case PLowCut:
                case PHighCut:
                {
                    // Butterworth en cascade ; le Q de la bande règle la résonance du dernier étage
                    static const float bw[4][4] = { { 0.7071f }, { 0.5412f, 1.3066f }, { 0.5176f, 0.7071f, 1.9319f }, { 0.5098f, 0.6013f, 0.9000f, 2.5629f } };
                    const int n = juce::jlimit (1, kMaxStages, slope + 1);
                    const auto t = type == PLowCut ? BiquadCoeffs::HighPass : BiquadCoeffs::LowPass;
                    for (int i = 0; i < n; ++i)
                        out[i] = BiquadCoeffs::make (t, sr, freq, bw[n - 1][i] * (i == n - 1 ? juce::jlimit (0.5f, 6.0f, q / 0.7071f) : 1.0f), 0.0);
                    return n;
                }
                case PBell:
                default:         out[0] = BiquadCoeffs::make (BiquadCoeffs::Peak, sr, freq, q, gainDb); return 1;
            }
        }

        static void curve (const ProBand& b, double sr, const float* freqs, float* outDb, int n)
        {
            BiquadCoeffs c[kMaxStages];
            const int ns = design (b.type, b.slope, sr, b.freq, b.q, b.gainDb + b.dynDb, c);
            for (int i = 0; i < n; ++i)
            {
                double s = 0;
                for (int k = 0; k < ns; ++k) s += c[k].magnitudeDb (freqs[i], sr);
                outDb[i] = b.on && b.active ? (float) s : 0.0f;
            }
        }

        // ---- audio ----------------------------------------------------------
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& b : state) b.live = false;
            for (auto& s : listenState) s.reset();
        }

        double getSampleRate() const { return sr; }

        void process (juce::AudioBuffer<float>& buf)
        {
            const int n = buf.getNumSamples(), nCh = juce::jmin (2, buf.getNumChannels());
            auto* const* d = buf.getArrayOfWritePointers();
            const float smooth = 1.0f - std::exp (-(float) kSub / (0.03f * (float) sr));

            for (int k = 0; k < kProBands; ++k)
            {
                const ProBand t = readBand (k);
                auto& s = state[k];
                if (! t.on || ! t.active) { s.live = false; dynDb[k].store (0.0f); continue; }

                if (! s.live || t.type != s.type || t.slope != s.slope || t.ch != s.ch)
                {
                    if (! s.live) { s.freq = t.freq; s.gain = t.gainDb; s.q = t.q; s.env = 0.0f; s.offset = 0.0f; }
                    s.type = t.type; s.slope = t.slope; s.ch = t.ch; s.live = true;
                    for (auto& st : s.st) for (auto& z : st) z.reset();
                    s.det.reset();
                }

                const bool dyn = t.dyn && proHasGain (t.type);
                const float attC = std::exp (-1.0f / (float) (juce::jmax (0.05f, t.att) * 0.001 * sr));
                const float relC = std::exp (-1.0f / (float) (juce::jmax (1.0f,  t.rel) * 0.001 * sr));
                if (! dyn) s.offset = 0.0f;

                for (int i0 = 0; i0 < n; i0 += kSub)
                {
                    const int len = juce::jmin (kSub, n - i0);

                    // lissage des réglages (fréquence et Q en échelle log)
                    s.freq *= std::pow (t.freq / s.freq, smooth);
                    s.q    *= std::pow (t.q / s.q, smooth);
                    s.gain += (t.gainDb - s.gain) * smooth;
                    if (std::abs (std::log (t.freq / s.freq)) < 1e-4f) s.freq = t.freq;
                    if (std::abs (t.gainDb - s.gain) < 1e-3f) s.gain = t.gainDb;

                    if (dyn)
                    {
                        // détecteur : passe-bande sur la zone de la bande, dans le canal choisi
                        const auto dc = BiquadCoeffs::make (BiquadCoeffs::BandPass, sr, s.freq, juce::jmax (0.5f, s.q), 0.0);
                        for (int i = i0; i < i0 + len; ++i)
                        {
                            const float a = std::abs (s.det.process (dc, detectorInput (d, nCh, t.ch, i)));
                            s.env = a > s.env ? a + attC * (s.env - a) : a + relC * (s.env - a);
                        }
                        const float over = gainToDb (s.env * 1.4142f) - t.thr;
                        const float gr = juce::jmax (0.0f, over) * (1.0f - 1.0f / juce::jmax (1.0f, t.ratio));
                        s.offset = t.range < 0.0f ? juce::jmax (t.range, -gr) : juce::jmin (t.range, gr);
                    }

                    const float g = s.gain + s.offset;
                    if (proHasGain (t.type) && std::abs (g) < 0.01f) continue;

                    BiquadCoeffs c[kMaxStages];
                    const int ns = design (t.type, t.slope, sr, s.freq, s.q, g, c);
                    auto run = [&] (float x, int z) { for (int j = 0; j < ns; ++j) x = s.st[j][z].process (c[j], x); return x; };

                    for (int i = i0; i < i0 + len; ++i)
                    {
                        switch (t.ch)
                        {
                            case PStereo: for (int c2 = 0; c2 < nCh; ++c2) d[c2][i] = run (d[c2][i], c2); break;
                            case PLeft:   d[0][i] = run (d[0][i], 0); break;
                            case PRight:  if (nCh == 2) d[1][i] = run (d[1][i], 1); break;
                            case PMid:
                            case PSide:
                            {
                                if (nCh < 2) { if (t.ch == PMid) d[0][i] = run (d[0][i], 0); break; }
                                float m = 0.5f * (d[0][i] + d[1][i]), sd = 0.5f * (d[0][i] - d[1][i]);
                                if (t.ch == PMid) m = run (m, 0); else sd = run (sd, 0);
                                d[0][i] = m + sd; d[1][i] = m - sd;
                                break;
                            }
                            default: break;
                        }
                    }
                }
                dynDb[k].store (s.offset);
            }
        }

        // Écoute en solo de la zone d'une bande (à appeler en fin de chaîne)
        void listen (juce::AudioBuffer<float>& buf)
        {
            const int k = solo.load();
            if (k < 0 || k >= kProBands) return;
            const ProBand b = readBand (k);
            if (! b.on) return;

            BiquadCoeffs c;
            switch (b.type)
            {
                case PLowCut: case PLowShelf:   c = BiquadCoeffs::make (BiquadCoeffs::LowPass,  sr, b.freq, 0.7071, 0.0); break;
                case PHighCut: case PHighShelf: c = BiquadCoeffs::make (BiquadCoeffs::HighPass, sr, b.freq, 0.7071, 0.0); break;
                default:                        c = BiquadCoeffs::make (BiquadCoeffs::BandPass, sr, b.freq, juce::jmax (0.7f, b.q), 0.0); break;
            }

            const int n = buf.getNumSamples(), nCh = juce::jmin (2, buf.getNumChannels());
            auto* const* d = buf.getArrayOfWritePointers();
            for (int i = 0; i < n; ++i)
            {
                if (b.ch == PStereo || nCh < 2)
                {
                    for (int z = 0; z < nCh; ++z) d[z][i] = listenState[z].process (c, d[z][i]);
                    continue;
                }
                const float v = listenState[0].process (c, detectorInput (d, nCh, b.ch, i));
                d[0][i] = d[1][i] = v;
            }
        }

        std::atomic<float> dynDb[kProBands] {};
        std::atomic<int> solo { -1 };

    private:
        static float detectorInput (float* const* d, int nCh, int ch, int i)
        {
            if (nCh < 2) return ch == PSide || ch == PRight ? 0.0f : d[0][i];
            switch (ch)
            {
                case PLeft:  return d[0][i];
                case PRight: return d[1][i];
                case PSide:  return 0.5f * (d[0][i] - d[1][i]);
                default:     return 0.5f * (d[0][i] + d[1][i]);
            }
        }

        struct BandState
        {
            bool live = false;
            int type = PBell, slope = 1, ch = PStereo;
            float freq = 1000.0f, gain = 0.0f, q = 1.0f, env = 0.0f, offset = 0.0f;
            BiquadState st[kMaxStages][2];
            BiquadState det;
        };

        double sr = 48000.0;
        std::atomic<float>* raw[kProBands][NumProFields] {};
        BandState state[kProBands];
        BiquadState listenState[2];
    };

    // -------------------------------------------------------------------------
    //  Lien interface <-> paramètres (thread message)
    // -------------------------------------------------------------------------
    class ProEqParams : public ProEqModel
    {
    public:
        ProEqParams (juce::AudioProcessorValueTreeState& s, ProEQ& e) : apvts (s), eq (e) {}

        ProBand band (int k) const override
        {
            auto b = eq.readBand (k);
            b.dynDb = b.dyn && proHasGain (b.type) ? eq.dynDb[k].load() : 0.0f;
            return b;
        }

        void setField (int k, ProField f, float v) override
        {
            if (auto* p = apvts.getParameter (proParamId (k, f)))
                p->setValueNotifyingHost (p->convertTo0to1 (v));
        }
        void beginGesture (int k, ProField f) override { if (auto* p = apvts.getParameter (proParamId (k, f))) p->beginChangeGesture(); }
        void endGesture (int k, ProField f) override   { if (auto* p = apvts.getParameter (proParamId (k, f))) p->endChangeGesture(); }

        void bandCurve (int k, const float* freqs, float* outDb, int n) const override
        {
            ProEQ::curve (band (k), eq.getSampleRate(), freqs, outDb, n);
        }

        int createBand (float freq, float gainDb, int type) override
        {
            for (int k = 0; k < kProBands; ++k)
            {
                if (eq.readBand (k).on) continue;
                setNow (k, FType, (float) type);
                setNow (k, FFreq, juce::jlimit (20.0f, 20000.0f, freq));
                setNow (k, FGain, proHasGain (type) ? juce::jlimit (-18.0f, 18.0f, gainDb) : 0.0f);
                setNow (k, FQ, type == PLowShelf || type == PHighShelf || proIsCut (type) ? 0.71f : (type == PNotch ? 4.0f : 1.0f));
                setNow (k, FSlope, 1.0f);
                setNow (k, FCh, (float) PStereo);
                setNow (k, FDyn, 0.0f);
                setNow (k, FActive, 1.0f);
                setNow (k, FOn, 1.0f);
                return k;
            }
            return -1;
        }

        void deleteBand (int k) override
        {
            if (eq.solo.load() == k) eq.solo.store (-1);
            setNow (k, FOn, 0.0f);
            setNow (k, FGain, 0.0f);
            setNow (k, FDyn, 0.0f);
        }

        int  getSolo() const override { return eq.solo.load(); }
        void setSolo (int k) override { eq.solo.store (k); }

    private:
        juce::AudioProcessorValueTreeState& apvts;
        ProEQ& eq;
    };
}
