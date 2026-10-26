#pragma once

// =============================================================================
//  MixPilot — logique de décision (pure, testable sans audio)
// =============================================================================

#include <vector>
#include <algorithm>
#include "Profiles.h"
#include "Analysis.h"

namespace mp
{
    // -------------------------------------------------------------------------
    //  Piste
    // -------------------------------------------------------------------------
    struct TrackCorrection
    {
        float trimDb = 0;            // gain de placement dans le mix
        float hpfHz  = 20;           // nettoyage du bas
        Bands eqDb {};               // EQ par octave (match + anti-masquage)
        Bands maskDb {};             // part due à l'anti-masquage (affichage)
        Bands mixDb {};              // part demandée par le Master (répartition)
        float compThresholdDb = 0, compRatio = 1, compAttackMs = 10, compReleaseMs = 100;
        float duckDepthDb = 0;       // ducking basse sous le kick
        float voiceDuckDb = 0;       // instru complète : creusement des médiums quand la voix chante
        float targetLufs = -23;      // niveau visé dans le mix
    };

    struct OtherTrack { int instrument; Bands bandsDb; };

    inline Bands smooth3 (const Bands& b)
    {
        Bands o;
        for (int i = 0; i < kBands; ++i)
        {
            const float l = b[(size_t) std::max (0, i - 1)], r = b[(size_t) std::min (kBands - 1, i + 1)];
            o[(size_t) i] = 0.25f * l + 0.5f * b[(size_t) i] + 0.25f * r;
        }
        return o;
    }

    // Petits ajustements d'équilibre selon la sortie visée
    inline float outputLevelOffset (int output, int inst)
    {
        if (output == Club)  return inst == Kick ? 1.0f : (inst == Bass ? 0.5f : (inst == Pad ? -0.5f : 0.0f));
        if (output == Vinyl) return inst == Bass ? -1.0f : (inst == Hats ? -1.0f : 0.0f);
        if (output == YouTube || output == Streaming) return inst == Vocal ? 0.5f : 0.0f;
        return 0.0f;
    }

    inline TrackCorrection solveTrack (int inst, const LearnResult& me, float learnedLufs, int genre, int output, float amount,
                                       const std::vector<OtherTrack>& others, bool kickPresent, bool voicePresent = false)
    {
        TrackCorrection c;
        const auto& ip = instrumentProfile (inst);
        const auto& gp = genreProfile (genre);
        amount = juce::jlimit (0.0f, 1.0f, amount);

        // 1) Niveau dans le mix
        c.targetLufs = ip.loudness + genreLevelOffset (genre, inst) + outputLevelOffset (output, inst);
        if (learnedLufs > -69.0f)
            c.trimDb = juce::jlimit (-24.0f, 18.0f, (c.targetLufs - learnedLufs) * amount);

        // 2) Coupe-bas
        c.hpfHz = 20.0f + (ip.hpfHz - 20.0f) * amount;

        // 3) EQ : rapprocher le spectre de la forme idéale de l'instrument
        if (me.valid)
        {
            // une instru complète vise l'équilibre d'un mix du style choisi
            const Bands& shape = inst == Instrumental ? gp.masterCurve : ip.shape;
            const Bands meas = normalised (me.bandsDb, ip.weight);
            const Bands targ = normalised (shape,      ip.weight);
            Bands diff {};
            for (int b = 0; b < kBands; ++b)
                diff[(size_t) b] = (targ[(size_t) b] - meas[(size_t) b]) * ip.weight[(size_t) b];
            diff = smooth3 (diff);

            // 4) Anti-masquage : on cède la place aux pistes prioritaires
            float myMax = -200; for (auto v : me.bandsDb) myMax = std::max (myMax, v);
            Bands dip {};
            for (const auto& o : others)
            {
                const auto& op = instrumentProfile (o.instrument);
                if (op.priority <= ip.priority) continue;
                float oMax = -200; for (auto v : o.bandsDb) oMax = std::max (oMax, v);
                for (int b = 1; b < kBands - 1; ++b)
                {
                    // la basse et le kick se partagent le bas par ducking, pas par EQ
                    if (b <= 2 && ((inst == Bass && o.instrument == Kick) || (inst == Kick && o.instrument == Bass))) continue;
                    const bool mine   = me.bandsDb[(size_t) b] > myMax - 9.0f;
                    const bool theirs = o.bandsDb[(size_t) b]  > oMax  - 6.0f;
                    if (mine && theirs) dip[(size_t) b] -= 1.5f;
                }
            }
            for (auto& d : dip) d = std::max (d, -3.0f);
            dip = smooth3 (dip);

            for (int b = 0; b < kBands; ++b)
            {
                // sous le coupe-bas, inutile d'égaliser
                if (kBandFreqs[(size_t) b] < c.hpfHz * 1.2f) diff[(size_t) b] = 0;
                // pas de boost là où l'instrument n'a presque rien (on monterait du bruit)
                if (me.bandsDb[(size_t) b] < myMax - 30.0f) diff[(size_t) b] = std::min (diff[(size_t) b], 0.0f);
                const float match = juce::jlimit (-5.0f, 4.0f, diff[(size_t) b] * 0.7f);
                c.maskDb[(size_t) b] = dip[(size_t) b] * amount;
                c.eqDb[(size_t) b]   = juce::jlimit (-6.0f, 4.0f, (match + dip[(size_t) b]) * amount);
            }
        }

        // 5) Compression adaptée à l'instrument et au style.
        //    Le seuil tient compte de l'effet de l'EQ (placée avant le compresseur).
        float eqDelta = 0.0f;
        if (me.valid)
        {
            double before = 0, after = 0;
            for (int b = 0; b < kBands; ++b)
            {
                const double p = std::pow (10.0, me.bandsDb[(size_t) b] / 10.0);
                const double hp = kBandFreqs[(size_t) b] < c.hpfHz ? 0.25 : 1.0;
                before += p; after += p * hp * std::pow (10.0, c.eqDb[(size_t) b] / 10.0);
            }
            if (before > 0 && after > 0) eqDelta = (float) (10.0 * std::log10 (after / before));
        }
        c.compRatio       = 1.0f + (ip.ratio - 1.0f) * gp.compDensity * amount;
        c.compThresholdDb = (me.valid ? me.meanFrameDb : -24.0f) + eqDelta + ip.threshOffsetDb;
        c.compAttackMs    = ip.attackMs;
        c.compReleaseMs   = ip.releaseMs;

        // 6) Basse qui se pousse quand le kick frappe
        if (inst == Instrumental && voicePresent)
        {
            static const float vByGenre[NumGenres] = { 2.5f, 3.0f, 3.0f, 2.0f, 3.5f };
            c.voiceDuckDb = vByGenre[std::clamp (genre, 0, NumGenres - 1)] * amount;
        }

        if (inst == Bass && kickPresent)
        {
            static const float depthByGenre[NumGenres] = { 5.0f, 4.5f, 3.0f, 2.0f, 3.5f };
            c.duckDepthDb = depthByGenre[std::clamp (genre, 0, NumGenres - 1)] * amount;
        }
        return c;
    }

    // -------------------------------------------------------------------------
    //  Master
    // -------------------------------------------------------------------------
    struct MasterTarget
    {
        Bands curve {};          // forme spectrale visée (style ou référence)
        float lufs = -14.0f;     // loudness visée
        float refSideRatio = -1; // énergie Side/Mid de la référence (-1 = pas de référence)
    };

    struct MasterCorrection
    {
        Bands eqDb {};           // part appliquée par le Master
        Bands mixDiff {};        // correction totale nécessaire au mix
        Bands trackDb {};        // part déléguée aux pistes
        float glue = 0.4f, width = 1.0f, warmth = 0.2f, clip = 0.4f;
        float monoBassHz = 0, highTameDb = 0, ceilingDb = -1, releaseMs = 100;
        float pushDb = 0;
    };

    inline MasterTarget styleTarget (int genre, int output)
    {
        MasterTarget t; t.curve = genreProfile (genre).masterCurve; t.lufs = outputLufs (output, genre); return t;
    }

    // share = part confiée aux pistes (0 = tout sur le Master, 1 = tout sur les pistes)
    inline MasterCorrection solveMaster (const LearnResult& in, float inLufs, float mixSideRatio, int genre, int output,
                                         float amount, const MasterTarget& target, float share)
    {
        MasterCorrection m;
        const auto& gp = genreProfile (genre);
        const auto& op = outputProfile (output);
        amount = juce::jlimit (0.0f, 1.0f, amount);
        share  = juce::jlimit (0.0f, 1.0f, share);

        if (in.valid)
        {
            const Bands w { 0.5f, 1, 1, 1, 1, 1, 1, 1, 1, 0.6f };
            const Bands meas = normalised (in.bandsDb, w);
            const Bands targ = normalised (target.curve, w);
            Bands diff {};
            for (int b = 0; b < kBands; ++b) diff[(size_t) b] = (targ[(size_t) b] - meas[(size_t) b]) * w[(size_t) b];
            diff = smooth3 (diff);
            for (int b = 0; b < kBands; ++b)
            {
                const float d = juce::jlimit (-6.0f, 6.0f, diff[(size_t) b] * 0.9f) * amount;
                m.mixDiff[(size_t) b] = d;
                m.eqDb[(size_t) b]    = juce::jlimit (-6.0f, 5.0f, d * (1.0f - share));
                m.trackDb[(size_t) b] = d - m.eqDb[(size_t) b];
            }
        }

        m.glue       = juce::jlimit (0.0f, 1.0f, (gp.glue + op.glueBoost) * amount);
        m.width      = 1.0f + (gp.width - 1.0f) * amount;
        if (mixSideRatio > 1e-6f)
        {
            // largeur ajustée vers l'image stéréo de la référence, ou celle typique du style
            const float wanted = target.refSideRatio >= 0 ? target.refSideRatio : gp.sideTarget;
            const float w = std::sqrt (std::max (wanted, 1e-6f) / mixSideRatio);
            m.width = 1.0f + (juce::jlimit (0.7f, 1.25f, w) - 1.0f) * amount;   // au-delà, les aigus deviennent durs
        }
        // écrêtage doux : surtout utile quand on vise fort (sorties non normalisées)
        m.clip = gp.clip * (outputIsNormalised (output) ? 0.5f : 1.0f) * juce::jmin (1.0f, amount / 0.8f);
        m.warmth     = gp.warmth * amount;
        m.monoBassHz = op.monoBassHz;
        m.highTameDb = op.highTameDb;
        m.ceilingDb  = op.ceilingDb;
        m.releaseMs  = gp.limiterReleaseMs;
        m.pushDb     = inLufs > -69.0f ? juce::jlimit (0.0f, 24.0f, target.lufs - inLufs) : 0.0f;
        return m;
    }

    // -------------------------------------------------------------------------
    //  Répartition Master -> Pistes
    // -------------------------------------------------------------------------

    // Atténuation (dB) d'un coupe-bas 12 dB/oct à la fréquence f
    inline float hpfAttenDb (float f, float fc)
    {
        if (fc <= 21.0f) return 0.0f;
        const double r4 = std::pow (f / fc, 4.0);
        return (float) (10.0 * std::log10 (r4 / (1.0 + r4)));
    }

    // Spectre d'une piste après ses propres corrections (= ce qu'elle apporte au mix)
    inline Bands estimateOutBands (const Bands& learned, const TrackCorrection& c, float trimDb)
    {
        Bands o;
        for (int b = 0; b < kBands; ++b)
            o[(size_t) b] = learned[(size_t) b] + trimDb + c.eqDb[(size_t) b] + hpfAttenDb (kBandFreqs[(size_t) b], c.hpfHz);
        return o;
    }

    // Pour chaque bande, répartit la correction D (dB) entre les pistes selon leur contribution,
    // de façon à ce que la somme bouge de D. Retourne les gains de la piste "me".
    inline Bands shareMixCorrection (const std::vector<Bands>& outBands, int me, const Bands& D, float maxDb = 6.0f)
    {
        Bands g {};
        if (me < 0 || me >= (int) outBands.size()) return g;
        for (int b = 0; b < kBands; ++b)
        {
            const float d = D[(size_t) b];
            if (std::abs (d) < 0.05f) continue;
            std::vector<double> P;
            double tot = 0, maxP = 0;
            for (auto& ob : outBands) { const double p = std::pow (10.0, ob[(size_t) b] / 10.0); P.push_back (p); tot += p; maxP = std::max (maxP, p); }
            if (tot <= 0 || maxP <= 0) continue;
            const double want = std::pow (10.0, d / 10.0);
            auto gainOf = [&] (double k, size_t t) { return juce::jlimit (-(double) maxDb, (double) maxDb, k * P[t] / maxP); };
            auto f = [&] (double k)
            {
                double sum = 0;
                for (size_t t = 0; t < P.size(); ++t) sum += P[t] * std::pow (10.0, gainOf (k, t) / 10.0);
                return sum / tot - want;
            };
            double lo = -60, hi = 60;
            for (int it = 0; it < 50; ++it) { const double mid = 0.5 * (lo + hi); (f (mid) > 0 ? hi : lo) = mid; }
            g[(size_t) b] = (float) gainOf (0.5 * (lo + hi), (size_t) me);
        }
        return g;
    }
}
