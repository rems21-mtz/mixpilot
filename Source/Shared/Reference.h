#pragma once

// =============================================================================
//  MixPilot — morceau de référence : analyse hors temps réel + lecture
// =============================================================================

#include <juce_audio_formats/juce_audio_formats.h>
#include "Analysis.h"
#include "DSP.h"

namespace mp
{
    struct ReferenceData
    {
        juce::String name, path;
        LearnResult learn;                    // spectre et caractère
        float lufs = -70, truePeakDb = -100, sideRatio = 0, lra = 0;
        double durationSec = 0;
        std::vector<float> overview;          // forme d'onde résumée (0..1)
        std::vector<int16_t> pcm;             // audio pour l'écoute, entrelacé stéréo, à la fréquence de l'hôte
        int64_t numFrames = 0;
        double sampleRate = 48000;
    };

    // Analyse complète d'un fichier. Retourne nullptr si illisible ou annulé.
    inline std::unique_ptr<ReferenceData> analyseReference (const juce::File& file, double hostSampleRate,
                                                            std::atomic<float>& progress, const std::atomic<bool>& cancel,
                                                            juce::String& error)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr) { error = juce::String::fromUTF8 ("Unsupported format (WAV, AIFF, FLAC, MP3, M4A accepted)"); return {}; }
        if (reader->lengthInSamples < (juce::int64) (reader->sampleRate * 5)) { error = juce::String::fromUTF8 ("File too short (5 s minimum)"); return {}; }

        auto ref = std::make_unique<ReferenceData>();
        ref->name = file.getFileNameWithoutExtension();
        ref->path = file.getFullPathName();
        ref->sampleRate = hostSampleRate;
        const double fileSR = reader->sampleRate;
        const juce::int64 total = reader->lengthInSamples;
        ref->durationSec = (double) total / fileSR;

        SpectrumAnalyzer an; an.prepare (fileSR); an.startLearning();
        LoudnessMeter lm;    lm.prepare (fileSR);
        TruePeakDetector tp;
        double sumM = 0, sumS = 0;
        float maxTp = 0;

        // lecture : 6 minutes max pour l'écoute (l'analyse utilise tout le fichier)
        const juce::int64 maxPlayFrames = (juce::int64) (hostSampleRate * 360.0);
        const double ratio = fileSR / hostSampleRate;
        juce::LagrangeInterpolator interp[2];
        ref->pcm.reserve ((size_t) std::min<juce::int64> (maxPlayFrames, (juce::int64) (total / ratio) + 16) * 2);

        const int overviewBins = 600;
        ref->overview.assign ((size_t) overviewBins, 0.0f);

        // historique des LUFS court terme pour la plage dynamique (LRA approchée)
        std::vector<float> shortTerms;

        const int chunk = 16384;
        juce::AudioBuffer<float> buf (2, chunk), tpBuf (2, chunk), resampled (2, (int) (chunk / ratio) + 64);
        std::vector<float> pending[2];   // entrée pas encore consommée par le rééchantillonneur
        std::vector<float> level ((size_t) chunk);
        juce::int64 pos = 0;
        int sinceShort = 0;

        while (pos < total)
        {
            if (cancel.load()) return {};
            const int n = (int) std::min<juce::int64> (chunk, total - pos);
            buf.clear();
            reader->read (&buf, 0, n, pos, true, true);
            if (reader->numChannels == 1) buf.copyFrom (1, 0, buf, 0, 0, n);

            auto* const* ch = buf.getArrayOfWritePointers();
            an.push (ch, 2, n);
            an.update();
            lm.process (ch, 2, n);

            for (int c = 0; c < 2; ++c) tpBuf.copyFrom (c, 0, buf, c, 0, n);
            tp.process (tpBuf.getArrayOfWritePointers(), 2, n, level.data());
            for (int i = 0; i < n; ++i) maxTp = std::max (maxTp, level[(size_t) i]);

            for (int i = 0; i < n; ++i)
            {
                const double m = 0.5 * (ch[0][i] + ch[1][i]), s = 0.5 * (ch[0][i] - ch[1][i]);
                sumM += m * m; sumS += s * s;
                const int bin = (int) ((pos + i) * overviewBins / total);
                auto& o = ref->overview[(size_t) juce::jlimit (0, overviewBins - 1, bin)];
                o = std::max (o, std::max (std::abs (ch[0][i]), std::abs (ch[1][i])));
            }

            sinceShort += n;
            if (sinceShort >= (int) fileSR) { sinceShort = 0; shortTerms.push_back (lm.getShortTerm()); }

            // audio pour l'écoute, rééchantillonné à la fréquence de l'hôte
            const juce::int64 havePlay = (juce::int64) ref->pcm.size() / 2;
            if (havePlay < maxPlayFrames)
            {
                for (int c = 0; c < 2; ++c)
                    pending[c].insert (pending[c].end(), buf.getReadPointer (c), buf.getReadPointer (c) + n);
                const int outN = juce::jmax (0, (int) std::floor (((double) pending[0].size() - 8.0) / ratio));
                if (resampled.getNumSamples() < outN + 4) resampled.setSize (2, outN + 4);
                int used = 0;
                for (int c = 0; c < 2; ++c)
                    used = interp[c].process (ratio, pending[c].data(), resampled.getWritePointer (c), outN);
                for (int c = 0; c < 2; ++c)
                    pending[c].erase (pending[c].begin(), pending[c].begin() + juce::jmin (used, (int) pending[c].size()));
                for (int i = 0; i < outN && (juce::int64) ref->pcm.size() / 2 < maxPlayFrames; ++i)
                    for (int c = 0; c < 2; ++c)
                        ref->pcm.push_back ((int16_t) juce::jlimit (-32767.0f, 32767.0f, resampled.getSample (c, i) * 32767.0f));
            }

            pos += n;
            progress.store ((float) pos / (float) total);
        }

        ref->numFrames = (int64_t) ref->pcm.size() / 2;
        an.stopLearning();
        ref->learn = an.getResult();
        ref->lufs = lm.getIntegrated();
        ref->truePeakDb = gainToDb (maxTp);
        ref->sideRatio = sumM > 0 ? (float) (sumS / sumM) : 0.0f;

        // LRA approchée : écart entre les percentiles 10 et 95 des LUFS court terme (passages actifs)
        std::vector<float> act;
        for (auto v : shortTerms) if (v > ref->lufs - 20.0f) act.push_back (v);
        if (act.size() > 4)
        {
            std::sort (act.begin(), act.end());
            ref->lra = act[(size_t) (act.size() * 95 / 100)] - act[(size_t) (act.size() * 10 / 100)];
        }

        float mx = 0; for (auto v : ref->overview) mx = std::max (mx, v);
        if (mx > 0) for (auto& v : ref->overview) v /= mx;

        if (! ref->learn.valid) { error = juce::String::fromUTF8 ("File is silent or unreadable"); return {}; }
        return ref;
    }
}
