#include "MasterProcessor.h"
#include "MasterEditor.h"

using namespace juce;

namespace
{
    constexpr int   kListenMs    = 8000;
    constexpr int   kSettleMs    = 4000;
    constexpr int   kVerifyWaitMs = 1200, kVerifyListenMs = 6000;   // vérification après correction des pistes
    constexpr int   kTrackWaitMs = 8000 + 4000 + 1500;   // écoute + réglage des pistes
    // Marges true peak (dB). Sur les plateformes qui normalisent (Spotify, YouTube), une marge plus large
    // ne coûte aucun volume (la plateforme recale le morceau) et protège mieux l'encodage.
    // Le vinyle aussi : son niveau final est fixé à la gravure, quelques dixièmes de dB ne comptent pas.
    bool  prudent (int output)       { return mp::outputIsNormalised (output) || output == mp::Vinyl; }
    float limiterMargin (int output) { return prudent (output) ? 0.50f : 0.30f; }
    float safetyMargin  (int output) { return prudent (output) ? 0.30f : 0.12f; }

    String u (const char* s) { return String::fromUTF8 (s); }

    const char* bandWord (int b)
    {
        static const char* w[] = { "l'extrême grave (30 Hz)", "les graves (60 Hz)", "le haut des graves (125 Hz)", "les bas-médiums (250 Hz)",
                                   "les médiums chauds (500 Hz)", "les médiums (1 kHz)", "le haut-médium (2 kHz)", "la présence (4 kHz)",
                                   "la brillance (8 kHz)", "l'air (16 kHz)" };
        return w[jlimit (0, mp::kBands - 1, b)];
    }

    String bandsToString (const mp::Bands& b) { String s; for (auto v : b) s << String (v, 3) << " "; return s.trim(); }
    bool stringToBands (const String& str, mp::Bands& b)
    {
        StringArray t; t.addTokens (str, " ", "");
        if (t.size() != mp::kBands) return false;
        for (int i = 0; i < mp::kBands; ++i) b[(size_t) i] = t[i].getFloatValue();
        return true;
    }
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout MasterProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto dB = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto pc = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)) + " %"; });

    StringArray genres, outputs;
    for (int i = 0; i < mp::NumGenres;  ++i) genres.add  (u (mp::genreProfile (i).name));
    for (int i = 0; i < mp::NumOutputs; ++i) outputs.add (u (mp::outputProfile (i).name));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "genre", 1 },  "Style",  genres,  mp::Pop));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "output", 1 }, "Sortie", outputs, mp::Streaming));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "amount", 1 }, u ("Intensité"), NormalisableRange<float> (0, 1), 0.8f, pc));

    for (int b = 0; b < mp::kBands; ++b)
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + String (b), 1 }, "EQ " + String (mp::kBandFreqs[(size_t) b], 0) + " Hz",
                                                      NormalisableRange<float> (-6, 6, 0.01f), 0.0f, dB));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "highTame", 1 }, u ("Aigus extrêmes"), NormalisableRange<float> (-6, 0, 0.01f), 0.0f, dB));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "eqOn", 1 }, "EQ actif", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glue", 1 }, "Glue", NormalisableRange<float> (0, 1), 0.4f, pc));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "dynOn", 1 }, "Dynamique active", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Largeur", NormalisableRange<float> (0, 2), 1.0f, pc));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "monoBass", 1 }, "Basses mono", NormalisableRange<float> (0, 300, 1), 0.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1 ? String ("Off") : String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "imageOn", 1 }, "Image active", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "warmth", 1 }, "Chaleur", NormalisableRange<float> (0, 1), 0.2f, pc));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "push", 1 }, "Push", NormalisableRange<float> (0, 24, 0.01f), 0.0f, dB));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "clip", 1 }, "Clip", NormalisableRange<float> (0, 1), 0.4f, pc));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Plafond", NormalisableRange<float> (-3, 0, 0.01f), -1.0f, dB));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, u ("Relâchement"), NormalisableRange<float> (10, 500, 1, 0.5f), 100.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "maxOn", 1 }, "Maximizer actif", true));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));

    // Nouveautés : référence et répartition
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Cible", StringArray { "Style", u ("Référence") }, 0));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "refLevel", 1 }, u ("Viser le volume de la référence"), false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "masterShare", 1 }, u ("Répartition (part du Master)"), NormalisableRange<float> (0, 1), 0.5f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return "Pistes " + String (roundToInt ((1 - v) * 100)) + " % / Master " + String (roundToInt (v * 100)) + " %"; })));
    return l;
}

//==============================================================================
MasterProcessor::MasterProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MASTER", createLayout())
{
    lp1.setType (dsp::LinkwitzRileyFilterType::lowpass);  hp1.setType (dsp::LinkwitzRileyFilterType::highpass);
    lp2.setType (dsp::LinkwitzRileyFilterType::lowpass);  hp2.setType (dsp::LinkwitzRileyFilterType::highpass);
    ap2.setType (dsp::LinkwitzRileyFilterType::allpass);
    for (auto* c : { &compLow, &compMid, &compHigh }) c->setRms (true);

    mp::bus().masterOwner.store (masterId);
    startTimerHz (30);
}

MasterProcessor::~MasterProcessor()
{
    stopTimer();
    refCancel.store (true);
    loaderPool.removeAllJobs (true, 5000);
    auto& b = mp::bus();
    if (b.masterOwner.load() == masterId)
    {
        b.masterHeartbeat.store (0);
        for (int k = 0; k < mp::kBands; ++k) b.mixCorrection[k].store (0.0f);
        b.mixCorrectionId.fetch_add (1);
    }
}

bool MasterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == AudioChannelSet::stereo() || o == AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void MasterProcessor::prepareToPlay (double sr, int block)
{
    const bool srChanged = std::abs (sr - currentSR) > 1.0;
    currentSR = sr; maxBlock = jmax (block, 32);
    const int nCh = jlimit (1, 2, getTotalNumOutputChannels());

    eq.prepare (sr);
    dsp::ProcessSpec spec { sr, (uint32) maxBlock, (uint32) nCh };
    for (auto* f : { &lp1, &hp1 }) { f->prepare (spec); f->setCutoffFrequency (120.0f); }
    for (auto* f : { &lp2, &hp2, &ap2 }) { f->prepare (spec); f->setCutoffFrequency (2500.0f); }
    for (auto* c : { &compLow, &compMid, &compHigh }) c->prepare (sr);
    bandLow.setSize (nCh, maxBlock); bandMid.setSize (nCh, maxBlock); bandHigh.setSize (nCh, maxBlock);
    sideHp.reset(); sideHpFreq = -1;

    oversampler = std::make_unique<dsp::Oversampling<float>> ((size_t) nCh, 2, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    limiter.prepare (sr, nCh, 1.5f);
    safetyLimiter.prepare (sr, nCh, 0.7f);
    safetyLimiter.setRelease (40.0f);
    tpDetector.reset(); tpDetector2.reset(); tpMeter.reset();
    levelBuf.assign ((size_t) maxBlock, 0.0f);
    meterBuf.setSize (nCh, maxBlock);

    pushGain.reset (sr * 4.0, 0.08);    pushGain.setCurrentAndTargetValue (mp::dbToGain (param ("push")));
    compareGain.reset (sr, 0.05);       compareGain.setCurrentAndTargetValue (1.0f);
    refMix.reset (sr, 0.03);            refMix.setCurrentAndTargetValue (refListen.load() ? 1.0f : 0.0f);
    refGain.reset (sr, 0.2);            refGain.setCurrentAndTargetValue (1.0f);

    latency = (int) std::round (oversampler->getLatencyInSamples()) + 2 * mp::TruePeakDetector::kDelay + limiter.getLatency() + safetyLimiter.getLatency();
    setLatencySamples (latency);
    dryDelayBuf.setSize (nCh, jmax (1, latency)); dryDelayBuf.clear(); dryDelayPos = 0;

    inMeter.prepare (sr);  outMeter.prepare (sr);
    inAnalyzer.prepare (sr); outAnalyzer.prepare (sr);

    // la référence est rééchantillonnée à la fréquence du projet : on la recharge si elle a changé
    if (srChanged && refShared != nullptr) loadReference (File (refShared->path), false);
}

//==============================================================================
void MasterProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n   = buffer.getNumSamples();
    const int nCh = jmin (2, getTotalNumOutputChannels(), dryDelayBuf.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || oversampler == nullptr) return;

    if (n > maxBlock)   // l'hôte dépasse la taille annoncée : on découpe
    {
        for (int start = 0; start < n; start += maxBlock)
        {
            const int len = jmin (maxBlock, n - start);
            AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
            MidiBuffer dummy; processBlock (sub, dummy);
        }
        return;
    }

    auto* const* ch = buffer.getArrayOfWritePointers();
    inMeter.process (ch, nCh, n);
    inAnalyzer.push (ch, nCh, n);
    {
        float pk = 0; for (int c = 0; c < nCh; ++c) pk = jmax (pk, buffer.getMagnitude (c, 0, n));
        inputPeak.push (pk);
    }

    // énergie Mid / Side pendant l'écoute de l'Assistant (pour comparer la largeur à la référence)
    if (msLearning.load() && nCh == 2)
    {
        double m2 = 0, s2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double m = 0.5 * (ch[0][i] + ch[1][i]), s = 0.5 * (ch[0][i] - ch[1][i]);
            m2 += m * m; s2 += s * s;
        }
        sumMid.store (sumMid.load() + m2); sumSide.store (sumSide.load() + s2);
    }

    // Copie sèche retardée (pour bypass / comparaison alignés)
    const int dl = dryDelayBuf.getNumSamples();
    auto dryAt = [&] (int c, int i, float x) -> float
    {
        if (latency <= 0) return x;
        float* r = dryDelayBuf.getWritePointer (c);
        const int p = (dryDelayPos + i) % dl;
        const float d = r[p]; r[p] = x; return d;
    };

    const bool bypass = param ("bypass") > 0.5f;
    const bool cmp    = comparing.load();

    if (bypass || cmp)
    {
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i) ch[c][i] = dryAt (c, i, ch[c][i]);
        dryDelayPos = (dryDelayPos + n) % jmax (1, dl);
        if (cmp)
            for (int i = 0; i < n; ++i) { const float g = compareGain.getNextValue(); for (int c = 0; c < nCh; ++c) ch[c][i] *= g; }
    }
    else
    {
        for (int c = 0; c < nCh; ++c) for (int i = 0; i < n; ++i) (void) dryAt (c, i, ch[c][i]);
        dryDelayPos = (dryDelayPos + n) % jmax (1, dl);

        // 1) EQ ------------------------------------------------------------------
        const bool eqOn = param ("eqOn") > 0.5f;
        mp::Bands g {};
        for (int b = 0; b < mp::kBands; ++b) g[(size_t) b] = apvts.getRawParameterValue ("eq" + String (b))->load();
        mp::configureOctaveEQ (eq, 0, g, eqOn);
        eq.setBand (10, mp::BiquadCoeffs::HighShelf, 12000.0f, 0.7f, param ("highTame"), eqOn);
        eq.process (buffer);

        // 2) Glue multibande -----------------------------------------------------
        const float glue = param ("glue");
        if (param ("dynOn") > 0.5f && glue > 0.001f)
        {
            const float ref = glueRefLufs.load();
            compLow.setParams  (ref + 1.0f - 7.0f * glue, 1.0f + 2.0f * glue, 30, 200, 8);
            compMid.setParams  (ref - 2.0f - 7.0f * glue, 1.0f + 1.6f * glue, 15, 150, 8);
            compHigh.setParams (ref - 7.0f - 7.0f * glue, 1.0f + 1.3f * glue,  8, 100, 8);
            bandLow.setSize (nCh, n, false, false, true); bandMid.setSize (nCh, n, false, false, true); bandHigh.setSize (nCh, n, false, false, true);
            for (int c = 0; c < nCh; ++c)
            {
                auto* lo = bandLow.getWritePointer (c); auto* mi = bandMid.getWritePointer (c); auto* hi = bandHigh.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    const float x = ch[c][i];
                    lo[i] = ap2.processSample (c, lp1.processSample (c, x));
                    const float rest = hp1.processSample (c, x);
                    mi[i] = lp2.processSample (c, rest);
                    hi[i] = hp2.processSample (c, rest);
                }
            }
            compLow.process (bandLow); compMid.process (bandMid); compHigh.process (bandHigh);
            for (int c = 0; c < nCh; ++c)
            {
                auto* lo = bandLow.getReadPointer (c); auto* mi = bandMid.getReadPointer (c); auto* hi = bandHigh.getReadPointer (c);
                for (int i = 0; i < n; ++i) ch[c][i] = lo[i] + mi[i] + hi[i];
            }
            glueGrDb.store (jmax (compLow.gainReductionDb.load(), compMid.gainReductionDb.load(), compHigh.gainReductionDb.load()));
        }
        else glueGrDb.store (0.0f);

        // 3) Image stéréo --------------------------------------------------------
        const float width = param ("width"), mono = param ("monoBass");
        if (nCh == 2 && param ("imageOn") > 0.5f && (std::abs (width - 1.0f) > 0.001f || mono >= 1.0f))
        {
            if (mono >= 1.0f && std::abs (mono - sideHpFreq) > 0.5f)
            { sideHpCoeffs = mp::BiquadCoeffs::make (mp::BiquadCoeffs::HighPass, currentSR, mono, 0.7071, 0); sideHpFreq = mono; }
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (ch[0][i] + ch[1][i]);
                float s = 0.5f * (ch[0][i] - ch[1][i]);
                if (mono >= 1.0f) s = sideHp.process (sideHpCoeffs, s);
                s *= width;
                ch[0][i] = m + s; ch[1][i] = m - s;
            }
        }

        // 4) Chaleur + Push suréchantillonnés x4 ----------------------------------
        const bool maxOn = param ("maxOn") > 0.5f;
        pushGain.setTargetValue (maxOn ? mp::dbToGain (param ("push")) : 1.0f);
        const int outIdx = (int) param ("output");
        limiter.setCeiling (param ("ceiling") - limiterMargin (outIdx));
        limiter.setRelease (param ("release"));
        const float warmth = param ("warmth");
        const float drive  = 1.0f + 3.0f * warmth;
        constexpr float satRef = 1.5f;
        // écrêteur doux : seuil au plafond (clip = 100 %) jusqu'à +4 dB au-dessus (clip = 0 %)
        const float clipAmt = maxOn ? param ("clip") : 0.0f;
        const float clipT   = mp::dbToGain (param ("ceiling") - limiterMargin (outIdx) + 4.0f * (1.0f - clipAmt));
        const float clipK   = clipT * 0.75f;   // début de l'arrondi (-2,5 dB sous le seuil)

        dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        float* upCh[2] = { up.getChannelPointer (0), nCh > 1 ? up.getChannelPointer (1) : nullptr };

        for (int i = 0; i < un; ++i)
        {
            const float pg = pushGain.getNextValue();
            for (int c = 0; c < nCh; ++c)
            {
                float x = upCh[c][i];
                if (warmth > 0.001f) x = satRef * std::tanh (drive * x / satRef) / drive;
                x *= pg;
                if (clipAmt > 0.01f)
                {
                    const float ax = std::abs (x);
                    if (ax > clipK)
                    {
                        const float y = clipK + (clipT - clipK) * std::tanh ((ax - clipK) / (clipT - clipK));
                        x = x < 0 ? -y : y;
                    }
                }
                upCh[c][i] = x;
            }
        }
        oversampler->processSamplesDown (block);

        // 5) Limiteur true peak --------------------------------------------------
        tpDetector.process (buffer.getArrayOfWritePointers(), nCh, n, levelBuf.data());
        if (! maxOn) limiter.setCeiling (24.0f);   // latence constante même désactivé
        limiter.process (buffer.getArrayOfWritePointers(), n, levelBuf.data());
        if (! maxOn) limiter.gainReductionDb.store (0.0f);

        // 2e étage true peak : quelques centièmes de dB au plus, ne crée pas de nouvelles crêtes
        tpDetector2.process (buffer.getArrayOfWritePointers(), nCh, n, levelBuf.data());
        safetyLimiter.setCeiling (maxOn ? param ("ceiling") - safetyMargin (outIdx) : 24.0f);
        safetyLimiter.process (buffer.getArrayOfWritePointers(), n, levelBuf.data());
    }

    // Mesures de la sortie traitée
    {
        meterBuf.setSize (nCh, n, false, false, true);
        for (int c = 0; c < nCh; ++c) meterBuf.copyFrom (c, 0, buffer, c, 0, n);
        tpMeter.process (meterBuf.getArrayOfWritePointers(), nCh, n, levelBuf.data());
        float tp = 0;
        for (int i = 0; i < n; ++i) tp = jmax (tp, levelBuf[(size_t) i]);
        truePeak.push (tp);
        truePeakReview.push (tp);
    }
    outMeter.process (ch, nCh, n);
    outAnalyzer.push (ch, nCh, n);

    // stéréo de la sortie (corrélation, largeur) pour la révision
    if (nCh == 2)
    {
        double ll = 0, rr = 0, lr = 0, mm = 0, ss = 0;
        for (int i = 0; i < n; ++i)
        {
            const double l = ch[0][i], r = ch[1][i], m = 0.5 * (l + r), sd = 0.5 * (l - r);
            ll += l * l; rr += r * r; lr += l * r; mm += m * m; ss += sd * sd;
        }
        const float k = std::exp (-(float) n / (float) (3.0 * currentSR));
        auto upd = [k] (std::atomic<float>& a, double v) { a.store ((float) (a.load() * k + v)); };
        upd (stLL, ll); upd (stRR, rr); upd (stLR, lr); upd (stMM, mm); upd (stSS, ss);
    }

    // 6) Écoute de la référence (fondu de 30 ms, volume égal possible) ----------
    refMix.setTargetValue (refListen.load() ? 1.0f : 0.0f);
    if (refMix.isSmoothing() || refMix.getTargetValue() > 0.0f)
    {
        const SpinLock::ScopedTryLockType lock (refLock);
        const auto* r = lock.isLocked() ? refAudio : nullptr;
        if (r != nullptr && r->numFrames > 0)
        {
            const float st = outMeter.getShortTerm();
            const float matchDb = refMatch.load() && st > -60.0f ? jlimit (-24.0f, 12.0f, st - r->lufs) : 0.0f;
            refGain.setTargetValue (mp::dbToGain (matchDb));
            int64_t p = refPos.load();
            for (int i = 0; i < n; ++i)
            {
                if (p >= r->numFrames) p = 0;
                const float mix = refMix.getNextValue(), gain = refGain.getNextValue();
                const float l = r->pcm[(size_t) (2 * p)] / 32767.0f, rr = r->pcm[(size_t) (2 * p + 1)] / 32767.0f;
                if (nCh == 2)
                {
                    ch[0][i] = ch[0][i] * (1.0f - mix) + l  * gain * mix;
                    ch[1][i] = ch[1][i] * (1.0f - mix) + rr * gain * mix;
                }
                else ch[0][i] = ch[0][i] * (1.0f - mix) + 0.5f * (l + rr) * gain * mix;
                if (mix > 0.0f) ++p;
            }
            refPos.store (p);
        }
        else refMix.skip (n);
    }
}

//==============================================================================
void MasterProcessor::setParam (const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

float MasterProcessor::getTrackShare() const
{
    int profiled = 0;
    for (auto& s : mp::bus().slots)
        if (s.owner.load() != 0 && mp::isAlive (s.heartbeat.load()) && s.hasProfile.load()) ++profiled;
    return profiled > 0 ? 1.0f - param ("masterShare") : 0.0f;
}

bool MasterProcessor::referenceIsTarget() const
{
    return refShared != nullptr && (int) param ("target") == 1;
}

void MasterProcessor::applyCorrection (const mp::MasterCorrection& m)
{
    for (int b = 0; b < mp::kBands; ++b) setParam ("eq" + String (b), m.eqDb[(size_t) b]);
    setParam ("highTame", m.highTameDb);
    setParam ("glue",     m.glue);
    setParam ("width",    m.width);
    setParam ("monoBass", m.monoBassHz);
    setParam ("warmth",   m.warmth);
    setParam ("ceiling",  m.ceilingDb);
    setParam ("release",  m.releaseMs);
    setParam ("clip",     m.clip);
    setParam ("push",     jlimit (0.0f, 24.0f, m.pushDb + pushResidual));
}

void MasterProcessor::publishMixCorrection (const mp::Bands& d)
{
    auto& b = mp::bus();
    for (int k = 0; k < mp::kBands; ++k) b.mixCorrection[k].store (d[(size_t) k]);
    b.trackShare.store (getTrackShare());
    b.mixCorrectionId.fetch_add (1);
}

void MasterProcessor::resolveFromLearned()
{
    if (! lastLearn.valid) return;
    const int genre = (int) param ("genre"), output = (int) param ("output");

    mp::MasterTarget t = mp::styleTarget (genre, output);
    if (referenceIsTarget())
    {
        t.curve = refShared->learn.bandsDb;
        t.refSideRatio = refShared->sideRatio;
        if (param ("refLevel") > 0.5f) t.lufs = jlimit (-20.0f, -6.0f, refShared->lufs);
    }
    targetLufs = t.lufs;

    lastCorrection = mp::solveMaster (lastLearn, lastInLufs, lastSideRatio, genre, output, param ("amount"), t, getTrackShare());
    // le Master complète ce que les pistes n'obtiennent pas réellement (mesuré lors de la vérification)
    for (int k = 0; k < mp::kBands; ++k)
    {
        const float achieved = lastCorrection.trackDb[(size_t) k] * trackEfficiency[(size_t) k];
        lastCorrection.eqDb[(size_t) k] = jlimit (-4.0f, 4.0f, lastCorrection.mixDiff[(size_t) k] - achieved);
    }
    applyCorrection (lastCorrection);
    publishMixCorrection (lastCorrection.trackDb);
}

void MasterProcessor::startAssistant()
{
    bool anyTrack = false;
    auto& b = mp::bus();
    for (auto& s : b.slots)
        if (s.owner.load() != 0 && mp::isAlive (s.heartbeat.load())) { anyTrack = true; break; }

    // on repart d'une base neutre : la correction déléguée aux pistes est remise à zéro
    publishMixCorrection ({});
    pushResidual = 0.0f;
    phaseStart = mp::nowMs();
    if (anyTrack)
    {
        b.listenSeconds.store (kListenMs / 1000);
        b.listenRequest.fetch_add (1);
        phase = Phase::WaitTracks; phaseLength = kTrackWaitMs;
    }
    else
    {
        inAnalyzer.startLearning(); inMeter.resetIntegrated();
        sumMid.store (0); sumSide.store (0); msLearning.store (true);
        phase = Phase::Listening; phaseLength = kListenMs;
    }
}

void MasterProcessor::cancelAssistant()
{
    inAnalyzer.stopLearning();
    verifyLearning = false;
    msLearning.store (false);
    phase = lastLearn.valid ? Phase::Done : Phase::Idle;
}

float MasterProcessor::getPhaseProgress() const
{
    if (phaseLength <= 0) return 0.0f;
    return jlimit (0.0f, 1.0f, (float) (mp::nowMs() - phaseStart) / (float) phaseLength);
}

String MasterProcessor::getStatusText() const
{
    switch (phase)
    {
        case Phase::Idle:       return u ("Lance l'Assistant pendant le passage le plus fort du morceau");
        case Phase::WaitTracks: return u ("1/3 · Les pistes s'analysent et se corrigent…");
        case Phase::Listening:  return u ("2/3 · Écoute du mix complet…");
        case Phase::Verifying:  return u ("2/3 · Vérification après correction des pistes…");
        case Phase::Settling:   return u ("3/3 · Calage du volume final…");
        case Phase::Done:       return u ("Terminé. Ajuste à ton goût, tout reste modifiable.");
        case Phase::NoSignal:   return u ("Aucun son détecté : lance la lecture puis recommence");
    }
    return {};
}

void MasterProcessor::setCompare (bool on)
{
    if (on)
    {
        const float diff = outMeter.getShortTerm() - inMeter.getShortTerm();
        compareGain.setCurrentAndTargetValue (mp::dbToGain (jlimit (-30.0f, 30.0f, std::isfinite (diff) ? diff : 0.0f)));
    }
    comparing.store (on);
}

std::vector<MasterProcessor::TrackInfo> MasterProcessor::getTracks() const
{
    std::vector<TrackInfo> v;
    for (auto& s : mp::bus().slots)
    {
        if (s.owner.load() == 0 || ! mp::isAlive (s.heartbeat.load())) continue;
        char name[32]; s.getName (name);
        v.push_back ({ String::fromUTF8 (name), s.instrument.load(), s.trimDb.load(), s.phase.load(), s.loudness.load(),
                       s.hasProfile.load() != 0, s.duckDb.load() });
    }
    return v;
}

//==============================================================================
//  Référence
//==============================================================================
void MasterProcessor::loadReference (const File& file, bool applyCorrection)
{
    refApplyOnInstall = applyCorrection;
    refCancel.store (true);
    loaderPool.removeAllJobs (true, 3000);
    refCancel.store (false);
    refLoading.store (true);
    refProgress.store (0.0f);
    { const SpinLock::ScopedLockType l (pendingLock); refError.clear(); }
    const double sr = currentSR;
    loaderPool.addJob ([this, file, sr]
    {
        String err;
        auto data = mp::analyseReference (file, sr, refProgress, refCancel, err);
        {
            const SpinLock::ScopedLockType l (pendingLock);
            refPending = std::move (data);
            refPathToLoad = file.getFullPathName();
            if (refPending == nullptr && ! refCancel.load()) refError = err.isEmpty() ? u ("Erreur de lecture") : err;
        }
        refLoading.store (false);
    });
}

void MasterProcessor::installReference (std::unique_ptr<mp::ReferenceData> data, bool apply)
{
    std::shared_ptr<mp::ReferenceData> sp (std::move (data));
    {
        const SpinLock::ScopedLockType l (refLock);
        refAudio = sp.get();
        refPos.store (0);
    }
    refShared = sp;   // l'ancienne référence est libérée ici, plus utilisée par l'audio
    if (apply) resolveFromLearned();
    else if (lastLearn.valid && referenceIsTarget())
    {
        // rechargement d'un projet : on republie la cible sans toucher aux réglages sauvegardés
        mp::MasterTarget t = mp::styleTarget ((int) param ("genre"), (int) param ("output"));
        t.curve = refShared->learn.bandsDb; t.refSideRatio = refShared->sideRatio;
        if (param ("refLevel") > 0.5f) t.lufs = jlimit (-20.0f, -6.0f, refShared->lufs);
        targetLufs = t.lufs;
        lastCorrection = mp::solveMaster (lastLearn, lastInLufs, lastSideRatio, (int) param ("genre"), (int) param ("output"),
                                          param ("amount"), t, 1.0f - param ("masterShare"));
        publishMixCorrection (lastCorrection.trackDb);
    }
}

void MasterProcessor::clearReference()
{
    refListen.store (false);
    {
        const SpinLock::ScopedLockType l (refLock);
        refAudio = nullptr;
    }
    refShared.reset();
    setParam ("target", 0.0f);
    resolveFromLearned();
}

void MasterProcessor::seekReference (double fraction)
{
    if (refShared != nullptr) refPos.store ((int64_t) (jlimit (0.0, 1.0, fraction) * (double) refShared->numFrames));
}

double MasterProcessor::getReferencePosition() const
{
    return refShared != nullptr && refShared->numFrames > 0 ? (double) refPos.load() / (double) refShared->numFrames : 0.0;
}

//==============================================================================
//  Conseils automatiques
//==============================================================================
StringArray MasterProcessor::getTips() const
{
    StringArray tips;
    const auto tracks = getTracks();
    const auto& op = mp::outputProfile ((int) param ("output"));
    const float integ = outMeter.getIntegrated();
    const float push = param ("push");

    if (tracks.empty())
        tips.add (u ("Ajoute MixPilot Track à la fin de chaque piste : le Master pourra alors corriger les pistes directement (voir RÉPARTITION)."));
    if (! lastLearn.valid && phase == Phase::Idle)
        tips.add (u ("Lance l'ASSISTANT pendant le passage le plus fort du morceau pour obtenir un premier réglage complet."));
    if (phase == Phase::NoSignal)
        tips.add (u ("Aucun son n'a été entendu : lance la lecture dans Ableton, puis relance l'ASSISTANT."));

    if (lastLearn.valid)
    {
        if (push > 12.0f)
            tips.add (u ("Le Master doit pousser de +") + String (push, 1) + u (" dB : ton mix arrive très faible. Monte les pistes (ou laisse les MixPilot Track le faire) pour moins écraser le son."));
        if (limGrAvg > 6.0f)
            tips.add (u ("Le maximizer réduit d'environ ") + String (limGrAvg, 1) + u (" dB : le son risque d'être écrasé. Choisis une sortie moins forte ou baisse PUSH."));
        if (glueGrDb.load() > 5.0f)
            tips.add (u ("La GLUE compresse fort : baisse-la si le mix perd du punch."));
        if (phase == Phase::Done && integ > -60.0f && std::abs (integ - targetLufs) > 1.0f)
            tips.add (u ("Le volume est à ") + String (integ - targetLufs, 1) + u (" LU de la cible : relance l'ASSISTANT sur le passage le plus fort, ou ajuste PUSH."));

        // déséquilibre spectral restant
        int worst = -1; float w = 0;
        for (int b = 0; b < mp::kBands; ++b)
            if (std::abs (lastCorrection.mixDiff[(size_t) b]) > std::abs (w)) { w = lastCorrection.mixDiff[(size_t) b]; worst = b; }
        if (worst >= 0 && std::abs (w) > 2.5f)
            tips.add (u ("Par rapport à la cible, ton mix ") + (w > 0 ? u ("manque de ") : u ("a trop de ")) + u (bandWord (worst))
                      + u (" : la correction est répartie selon le curseur RÉPARTITION."));
    }

    if (inPeakHold > 0.5f)
        tips.add (u ("Le mix arrive au Master au-dessus de 0 dBFS (") + String (inPeakHold, 1) + u (" dB) : baisse le fader Master ou les pistes pour garder de la marge."));

    if (lastLearn.valid && mp::outputIsNormalised ((int) param ("output")))
        tips.add (u ("Sortie ") + u (op.name) + u (" : le morceau est calé à ") + String (targetLufs, 0)
                  + u (" LUFS car la plateforme baisse les morceaux plus forts. Pour un son plus fort dans Ableton, choisis SoundCloud, Club ou CD."));
    if (lastLearn.valid && phase == Phase::Done && getLimiterGr() < 0.5f && limGrAvg < 0.5f && ! mp::outputIsNormalised ((int) param ("output")))
        tips.add (u ("Le maximizer travaille à peine : tu peux monter PUSH ou CLIP pour plus de volume, ou relancer l'ASSISTANT."));
    if ((int) param ("output") == mp::Club && param ("monoBass") < 1.0f)
        tips.add (u ("Pour le club, active BASSES MONO (vers 120 Hz) : les graves seront solides sur la sono."));

    // kick / basse
    bool kick = false, bassNoDuck = false;
    for (auto& t : tracks)
    {
        if (t.instrument == mp::Kick && t.hasProfile) kick = true;
        if (t.instrument == mp::Bass && t.hasProfile && t.duckDb < 0.1f) bassNoDuck = true;
    }
    if (kick && bassNoDuck)
        tips.add (u ("Le kick et la basse se disputent les graves : active ESPACE sur la piste basse (MixPilot Track)."));

    if (refShared != nullptr)
    {
        if ((int) param ("target") == 0)
            tips.add (u ("Une référence est chargée : choisis CIBLE = Référence pour que l'Assistant s'en inspire."));
        else if (param ("refLevel") < 0.5f && refShared->lufs > targetLufs + 3.0f)
            tips.add (u ("Ta référence est à ") + String (refShared->lufs, 1) + u (" LUFS, ta sortie vise ") + String (targetLufs, 0)
                      + u (" LUFS : normal, les plateformes baissent les morceaux trop forts. Compare avec ÉCOUTER RÉF. (volume égal)."));
    }
    else if (lastLearn.valid)
        tips.add (u ("Astuce : glisse un morceau que tu aimes dans RÉFÉRENCE pour viser son équilibre et le comparer à volume égal."));

    if (lastLearn.valid && ! tracks.empty() && param ("masterShare") > 0.95f)
        tips.add (u ("RÉPARTITION est tout à droite : le Master corrige seul. Glisse vers PISTES pour que les pistes responsables se corrigent elles-mêmes (son plus naturel)."));

    if (tips.isEmpty())
        tips.add (u ("Tout est en ordre. Compare avec A/B (volume égal) pour juger le résultat sans être trompé par le volume."));
    return tips;
}

//==============================================================================
//  Révision du mix
//==============================================================================
mp::MixReview MasterProcessor::buildReview() const
{
    mp::MixReview rv;
    rv.sections = { { u ("Mix global"), {} }, { u ("Pistes"), {} }, { u ("Conflits entre pistes"), {} } };
    enum { G = 0, T = 1, C = 2 };
    auto freqTxt = [] (float f) { return f >= 1000 ? String (f / 1000, f >= 10000 ? 0 : 0) + " kHz" : String ((int) f) + " Hz"; };
    auto sgn = [] (float v) { return (v >= 0 ? "+" : "") + String (v, 1); };

    const auto& op = mp::outputProfile ((int) param ("output"));
    const float integ = outMeter.getIntegrated();

    // ------------------------------------------------------------------ global
    if (! lastLearn.valid)
        rv.add (G, mp::SevWarn, "Assistant", u ("Pas encore lancé : lance l'ASSISTANT sur le passage le plus fort pour régler pistes et master."));
    else
        rv.add (G, mp::SevOk, "Assistant", u ("Réglage effectué pour ") + u (mp::genreProfile ((int) param ("genre")).name) + " / " + u (op.name) + ".");

    if (integ > -60.0f)
    {
        const float d = integ - targetLufs;
        if (std::abs (d) <= 1.0f) rv.add (G, mp::SevOk, "Volume final", String (integ, 1) + u (" LUFS, conforme à la cible (") + String (targetLufs, 1) + ").");
        else rv.add (G, mp::SevWarn, "Volume final", String (integ, 1) + u (" LUFS, soit ") + sgn (d) + u (" LU par rapport à la cible. Relance l'ASSISTANT sur le passage le plus fort, ou ajuste PUSH."));
    }
    else rv.add (G, mp::SevInfo, "Volume final", u ("Lance la lecture quelques secondes pour mesurer (bouton RESET pour repartir de zéro)."));

    {
        const float tp = truePeakHoldForReview;
        if (tp > -99.0f)
        {
            if (tp > op.ceilingDb + 0.05f) rv.add (G, mp::SevProblem, u ("Crêtes"), u ("True peak ") + String (tp, 1) + u (" dBTP au-dessus du plafond (") + String (op.ceilingDb, 1) + u (") : vérifie que MAXIMIZER est actif et baisse PUSH."));
            else rv.add (G, mp::SevOk, u ("Crêtes"), u ("True peak max ") + String (tp, 1) + u (" dBTP, sous le plafond."));
        }
    }

    // équilibre spectral de la sortie (spectre en direct) comparé à la cible
    {
        const mp::Bands ob = outAnalyzer.getLongTermBandsDb();
        const bool ok = outAnalyzer.hasLongTerm() && integ > -60.0f;
        if (ok)
        {
            const mp::Bands w { 0.5f, 1, 1, 1, 1, 1, 1, 1, 1, 0.6f };
            const mp::Bands tgt = referenceIsTarget() ? refShared->learn.bandsDb : mp::genreProfile ((int) param ("genre")).masterCurve;
            const auto a = mp::normalised (ob, w), t = mp::normalised (tgt, w);
            int worst = -1; float wv = 0;
            for (int b = 1; b < 9; ++b) if (std::abs (a[(size_t) b] - t[(size_t) b]) > std::abs (wv)) { wv = a[(size_t) b] - t[(size_t) b]; worst = b; }
            static const char* zone[] = { "extrême grave", "graves", "haut des graves", "bas-médiums", "médiums chauds", "médiums", "haut-médium", "présence", "brillance", "air" };
            if (worst >= 0 && std::abs (wv) > 3.0f)
                rv.add (G, mp::SevWarn, u ("Équilibre"), u ("Trop ") + (wv > 0 ? u ("de ") : u ("peu de ")) + u (zone[worst]) + u (" (") + freqTxt (mp::kBandFreqs[(size_t) worst]) + ", " + sgn (wv)
                        + u (" dB vs la cible). Relance l'ASSISTANT ; si ça persiste, regarde quelle piste domine cette zone ci-dessous."));
            else rv.add (G, mp::SevOk, u ("Équilibre"), u ("Proche de la cible ") + (referenceIsTarget() ? u ("(référence)") : u ("du style")) + u (" : aucune zone à plus de 3 dB."));
        }
    }

    // stéréo
    {
        const float ll = stLL.load(), rr = stRR.load(), lr = stLR.load(), mm = stMM.load(), ss = stSS.load();
        if (mm > 1e-6f)
        {
            const float corr = lr / std::sqrt (std::max (1e-12f, ll * rr));
            const float side = ss / mm;
            if (corr < 0.0f) rv.add (G, mp::SevProblem, u ("Stéréo"), u ("Corrélation négative (") + String (corr, 2) + u (") : problème de phase, le son peut disparaître en mono. Vérifie les élargisseurs stéréo sur les pistes, et baisse LARGEUR."));
            else if (side < 0.02f) rv.add (G, mp::SevInfo, u ("Stéréo"), u ("Mix presque mono : ça manque d'espace. Monte LARGEUR, ou ajoute de la stéréo sur les nappes et effets."));
            else if (side > 0.45f) rv.add (G, mp::SevWarn, u ("Stéréo"), u ("Mix très large (risque en mono et en club). Baisse LARGEUR ou active BASSES MONO."));
            else rv.add (G, mp::SevOk, u ("Stéréo"), u ("Largeur saine, corrélation ") + String (corr, 2) + ".");
        }
        if ((int) param ("output") == mp::Club && param ("monoBass") < 1.0f)
            rv.add (G, mp::SevWarn, u ("Graves en club"), u ("BASSES MONO est désactivé : active-le (120 Hz) pour des graves solides sur sono."));
    }

    // dynamique
    if (limGrAvg > 6.0f) rv.add (G, mp::SevWarn, "Compression", u ("Le maximizer réduit en moyenne de ") + String (limGrAvg, 1) + u (" dB : le son risque d'être écrasé. Baisse PUSH, monte CLIP, ou choisis une sortie moins forte."));
    else if (glueGrDb.load() > 5.0f) rv.add (G, mp::SevWarn, "Compression", u ("La GLUE compresse fort : baisse-la si le mix manque de punch."));
    else if (lastLearn.valid) rv.add (G, mp::SevOk, "Compression", u ("Dynamique maîtrisée (maximizer ") + String (limGrAvg, 1) + u (" dB en moyenne)."));
    if (inPeakHold > 0.5f) rv.add (G, mp::SevWarn, "Marge", u ("Le mix arrive au Master au-dessus de 0 dBFS : baisse le fader Master ou les pistes de quelques dB."));

    // ------------------------------------------------------------------ pistes
    struct TI { int slot; String name; int inst; bool prof; mp::Bands ob; float duck, vduck, mask; int maskBand; };
    std::vector<TI> tl;
    auto& b = mp::bus();
    for (int si = 0; si < mp::kMaxTracks; ++si)
    {
        auto& s = b.slots[si];
        if (s.owner.load() == 0 || ! mp::isAlive (s.heartbeat.load())) continue;
        char nm[32]; s.getName (nm);
        TI t { si, String::fromUTF8 (nm), s.instrument.load(), s.hasProfile.load() != 0, {}, s.duckDb.load(), s.voiceDuckDb.load(), s.maskDb.load(), s.maskBand.load() };
        for (int k = 0; k < mp::kBands; ++k) t.ob[(size_t) k] = s.outBands[k].load();
        tl.push_back (t);

        const String title = t.name + " (" + (t.prof ? u (mp::instrumentProfile (t.inst).name) : String ("?")) + ")";
        if (s.bypassed.load()) { rv.add (T, mp::SevInfo, title, u ("BYPASS actif : cette piste n'est pas corrigée.")); continue; }
        if (! t.prof) { rv.add (T, mp::SevWarn, title, u ("Pas encore analysée : lance l'ASSISTANT ou clique ÉCOUTER sur la piste pendant qu'elle joue.")); continue; }

        StringArray notes; int sev = mp::SevOk;
        const float inPk = s.inputPeakDb.load();
        if (inPk > 0.5f) { sev = mp::SevProblem; notes.add (u ("le son arrive saturé (") + String (inPk, 1) + u (" dBFS) : baisse l'instrument ou l'effet avant MixPilot")); }
        if (! s.manual.load() && s.confidence.load() < 0.6f) { sev = std::max (sev, (int) mp::SevWarn); notes.add (u ("détection incertaine : vérifie l'INSTRUMENT")); }
        const float trim = s.trimDb.load();
        const float level = s.outIntegrated.load(), tgt = s.targetLufs.load();
        if (level > -60.0f && std::abs (level - tgt) > 2.0f)
        { sev = std::max (sev, (int) mp::SevWarn); notes.add (u ("niveau ") + sgn (level - tgt) + u (" dB par rapport à sa place dans le mix (GAIN désactivé, limite atteinte ou piste modifiée depuis l'analyse : relance l'écoute)")); }
        if (trim > 12.0f) { sev = std::max (sev, (int) mp::SevInfo); notes.add (u ("source très faible (+") + String (trim, 1) + u (" dB) : monte-la dans l'instrument")); }
        if (trim < -12.0f) { sev = std::max (sev, (int) mp::SevInfo); notes.add (u ("source très forte (") + String (trim, 1) + u (" dB) : baisse-la dans l'instrument")); }
        if (s.gainReductionDb.load() > 8.0f) { sev = std::max (sev, (int) mp::SevWarn); notes.add (u ("compression forte : baisse l'INTENSITÉ de la piste")); }
        if (! s.following.load()) notes.add (u ("SUIVRE MASTER désactivé"));
        if (t.mask < -1.0f && t.maskBand >= 0) notes.add (u ("laisse ") + String (-t.mask, 1) + u (" dB de place vers ") + freqTxt (mp::kBandFreqs[(size_t) t.maskBand]));
        if (t.inst == mp::Bass && t.duck > 0.05f) notes.add (u ("s'efface sous le kick (") + String (t.duck, 1) + " dB)");
        if (t.inst == mp::Instrumental && t.vduck > 0.05f) notes.add (u ("se creuse sous la voix (") + String (t.vduck, 1) + " dB)");
        if (std::abs (s.mixShareDb.load()) > 0.3f) notes.add (u ("applique ") + sgn (s.mixShareDb.load()) + u (" dB demandés par le Master"));
        rv.add (T, sev, title, u ("Gain ") + sgn (trim) + u (" dB") + (notes.isEmpty() ? u (". Rien à signaler.") : " ; " + notes.joinIntoString (" ; ") + "."));
    }
    if (tl.empty())
        rv.add (T, mp::SevInfo, u ("Aucune piste reliée"), u ("Ajoute MixPilot Track à la fin de chaque piste pour une révision piste par piste."));

    // ------------------------------------------------------------------ conflits
    int nConf = 0;
    for (size_t i = 0; i < tl.size(); ++i)
        for (size_t j = i + 1; j < tl.size(); ++j)
        {
            const auto& A = tl[i]; const auto& B = tl[j];
            if (! A.prof || ! B.prof) continue;
            float ma = -200, mb = -200;
            for (int k = 0; k < mp::kBands; ++k) { ma = std::max (ma, A.ob[(size_t) k]); mb = std::max (mb, B.ob[(size_t) k]); }
            if (ma < -90 || mb < -90) continue;
            int best = -1; float bestScore = 0;
            for (int k = 1; k < mp::kBands - 1; ++k)
            {
                const float ra = A.ob[(size_t) k] - ma, rb = B.ob[(size_t) k] - mb;
                const float diff = std::abs (A.ob[(size_t) k] - B.ob[(size_t) k]);
                if (ra > -6.0f && rb > -6.0f && diff < 6.0f)
                {
                    const float sc = 12.0f - diff + ra + rb;
                    if (sc > bestScore) { bestScore = sc; best = k; }
                }
            }
            if (best < 0) continue;
            const String pair = A.name + u (" ↔ ") + B.name;
            const String where = u (" vers ") + freqTxt (mp::kBandFreqs[(size_t) best]);
            const bool kb = (A.inst == mp::Kick && B.inst == mp::Bass) || (A.inst == mp::Bass && B.inst == mp::Kick);
            const bool iv = (A.inst == mp::Instrumental && mp::isVoice (B.inst)) || (B.inst == mp::Instrumental && mp::isVoice (A.inst));
            const auto& bass = A.inst == mp::Bass ? A : B;
            const auto& inst = A.inst == mp::Instrumental ? A : B;
            const auto& low  = mp::instrumentProfile (A.inst).priority < mp::instrumentProfile (B.inst).priority ? A : B;
            if (kb)
                rv.add (C, bass.duck > 0.05f ? mp::SevOk : mp::SevWarn, pair, u ("Se disputent les graves") + where
                        + (bass.duck > 0.05f ? u (" : réglé, la basse s'efface sous le kick.") : u (" : active ESPACE sur la piste basse.")));
            else if (iv)
                rv.add (C, inst.vduck > 0.05f ? mp::SevOk : mp::SevWarn, pair, u ("La voix et l'instru se chevauchent") + where
                        + (inst.vduck > 0.05f ? u (" : réglé, l'instru se creuse quand la voix chante.") : u (" : active ESPACE sur l'instru complète.")));
            else
                rv.add (C, low.mask < -0.9f ? mp::SevOk : mp::SevInfo, pair, u ("Même zone de fréquences") + where
                        + (low.mask < -0.9f ? u (" : réglé, ") + low.name + u (" laisse la place.") : u (" : si ça sonne brouillon, baisse un peu l'une des deux ou change d'octave.")));
            ++nConf;
        }
    if (nConf == 0 && tl.size() > 1) rv.add (C, mp::SevOk, u ("Aucun conflit"), u ("Les pistes reliées occupent des zones différentes."));

    // mashup : la voix passe-t-elle au-dessus de l'instru ?
    for (auto& v : tl)
        for (auto& in : tl)
            if (mp::isVoice (v.inst) && in.inst == mp::Instrumental && v.prof && in.prof)
            {
                const float vp = 0.5f * (v.ob[6] + v.ob[7]), ip = 0.5f * (in.ob[6] + in.ob[7]) - in.vduck;
                const float margin = vp - ip;
                if (margin < 0.0f)
                    rv.add (C, mp::SevWarn, v.name + u (" sur ") + in.name, u ("La voix risque d'être couverte dans la zone 2 – 4 kHz (") + sgn (margin)
                            + u (" dB) : monte l'acapella de ") + String (std::ceil (-margin) + 1, 0) + u (" dB ou augmente l'INTENSITÉ de l'instru."));
                else if (margin > 12.0f)
                    rv.add (C, mp::SevInfo, v.name + u (" sur ") + in.name, u ("La voix domine très nettement (") + sgn (margin) + u (" dB dans la présence) : tu peux la baisser un peu pour mieux la fondre."));
                else
                    rv.add (C, mp::SevOk, v.name + u (" sur ") + in.name, u ("La voix passe bien au-dessus de l'instru (") + sgn (margin) + u (" dB dans la présence)."));
            }

    // ------------------------------------------------------------------ note
    rv.score = juce::jlimit (0, 100, 100 - 15 * rv.problems - 6 * rv.warnings);
    rv.verdict = rv.score >= 90 ? u ("Mix prêt : rien de bloquant.")
               : rv.score >= 70 ? u ("Bon mix : quelques points à vérifier.")
               : rv.score >= 50 ? u ("À retravailler : plusieurs points à corriger.")
                                : u ("Problèmes importants : corrige les points en rouge d'abord.");
    return rv;
}

//==============================================================================
void MasterProcessor::timerCallback()
{
    inAnalyzer.update();
    outAnalyzer.update();
    limGrAvg = limGrAvg * 0.97f + 0.03f * getLimiterGr();
    inPeakHold = jmax (inputPeak.takeDb(), inPeakHold - 0.2f);
    truePeakHoldForReview = jmax (truePeakReview.takeDb(), truePeakHoldForReview - 0.005f);   // maintien ~10 s

    // référence fraîchement analysée ?
    {
        std::unique_ptr<mp::ReferenceData> pending;
        {
            const SpinLock::ScopedLockType l (pendingLock);
            pending = std::move (refPending);
        }
        if (pending != nullptr) installReference (std::move (pending), refApplyOnInstall);
    }

    auto& b = mp::bus();
    const int   genre  = (int) param ("genre");
    const int   output = (int) param ("output");
    const float amount = param ("amount");
    const int   target = (int) param ("target");
    const int   refLvl = param ("refLevel") > 0.5f ? 1 : 0;
    const float share  = param ("masterShare");
    b.masterOwner.store (masterId);
    b.masterHeartbeat.store (mp::nowMs());
    b.genre.store (genre); b.output.store (output); b.amount.store (amount);
    b.masterLufs.store (outMeter.getShortTerm());

    int profiled = 0;
    for (auto& s : b.slots) if (s.owner.load() && mp::isAlive (s.heartbeat.load()) && s.hasProfile.load()) ++profiled;

    // Un réglage a changé après l'analyse : on recalcule et on informe les pistes
    const bool changed = genre != lastGenre || output != lastOutput || std::abs (amount - lastAmount) > 0.005f
                      || target != lastTarget || refLvl != lastRefLevel || std::abs (share - lastShare) > 0.005f;
    if (changed && lastGenre >= 0 && (phase == Phase::Done || phase == Phase::Idle))
        resolveFromLearned();
    lastGenre = genre; lastOutput = output; lastAmount = amount; lastTarget = target; lastRefLevel = refLvl; lastShare = share;
    lastProfiledTracks = profiled;

    // Si une autre instance a effacé la correction partagée, on la rétablit
    if (lastLearn.valid && (phase == Phase::Done || phase == Phase::Idle))
    {
        bool same = true;
        for (int k = 0; k < mp::kBands; ++k)
            same = same && std::abs (b.mixCorrection[k].load() - lastCorrection.trackDb[(size_t) k]) < 0.001f;
        if (! same) publishMixCorrection (lastCorrection.trackDb);
    }

    const int64_t elapsed = mp::nowMs() - phaseStart;
    switch (phase)
    {
        case Phase::WaitTracks:
            if (elapsed >= phaseLength)
            {
                inAnalyzer.startLearning(); inMeter.resetIntegrated();
                sumMid.store (0); sumSide.store (0); msLearning.store (true);
                phase = Phase::Listening; phaseStart = mp::nowMs(); phaseLength = kListenMs;
            }
            break;

        case Phase::Listening:
            if (elapsed >= phaseLength)
            {
                inAnalyzer.stopLearning();
                msLearning.store (false);
                const auto r = inAnalyzer.getResult();
                const float inL = inMeter.getIntegrated();
                if (! r.valid || inL < -60.0f) { phase = Phase::NoSignal; phaseLength = 0; break; }
                lastLearn = r; lastInLufs = inL;
                lastSideRatio = sumMid.load() > 0 ? (float) (sumSide.load() / sumMid.load()) : -1.0f;
                glueRefLufs.store (inL);
                trackEfficiency.fill (1.0f);
                resolveFromLearned();
                outMeter.resetIntegrated();
                settleIter = 0;
                if (getTrackShare() > 0.0f)
                {
                    // les pistes appliquent leur part : on vérifie ce qu'elles obtiennent vraiment
                    phase = Phase::Verifying; phaseStart = mp::nowMs(); phaseLength = kVerifyWaitMs + kVerifyListenMs;
                    verifyLearning = false;
                }
                else { phase = Phase::Settling; phaseStart = mp::nowMs(); phaseLength = kSettleMs; }
            }
            break;

        case Phase::Verifying:
            if (! verifyLearning && elapsed >= kVerifyWaitMs)
            {
                inAnalyzer.startLearning(); inMeter.resetIntegrated();
                verifyLearning = true;
            }
            if (elapsed >= phaseLength)
            {
                inAnalyzer.stopLearning();
                verifyLearning = false;
                const auto r2 = inAnalyzer.getResult();
                const float inL2 = inMeter.getIntegrated();
                if (r2.valid && inL2 > -60.0f)
                {
                    // changement réellement obtenu par les pistes, bande par bande (formes normalisées)
                    const mp::Bands w { 0.5f, 1, 1, 1, 1, 1, 1, 1, 1, 0.6f };
                    const auto before = mp::normalised (lastLearn.bandsDb, w), after = mp::normalised (r2.bandsDb, w);
                    for (int k = 0; k < mp::kBands; ++k)
                    {
                        const float asked = lastCorrection.trackDb[(size_t) k];
                        const float got   = after[(size_t) k] - before[(size_t) k];
                        trackEfficiency[(size_t) k] = std::abs (asked) > 0.2f ? jlimit (0.0f, 1.5f, got / asked) : 1.0f;
                    }
                    glueRefLufs.store (inL2);
                    resolveFromLearned();
                }
                outMeter.resetIntegrated();
                phase = Phase::Settling; phaseStart = mp::nowMs(); phaseLength = kSettleMs;
            }
            break;

        case Phase::Settling:
            if (elapsed >= phaseLength)
            {
                const float outL = outMeter.getIntegrated();
                float err = 0.0f;
                if (outL > -60.0f)
                {
                    err = jlimit (-8.0f, 8.0f, targetLufs - outL);
                    // au-delà du seuil du limiteur, le volume monte moins vite que le push : on compense
                    const float k = err > 0 ? 1.0f + jlimit (0.0f, 0.8f, getLimiterGr() / 6.0f) : 1.0f;
                    const float step = err * k;
                    pushResidual += step;
                    setParam ("push", jlimit (0.0f, 24.0f, param ("push") + step));
                }
                outMeter.resetIntegrated();
                if (++settleIter >= 4 || std::abs (err) < 0.25f) { phase = Phase::Done; phaseLength = 0; }
                else phaseStart = mp::nowMs();
            }
            break;

        default: break;
    }
}

//==============================================================================
AudioProcessorEditor* MasterProcessor::createEditor() { return new MasterEditor (*this); }

void MasterProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("glueRef", glueRefLufs.load(), nullptr);
    state.setProperty ("refPath", refShared != nullptr ? refShared->path : String(), nullptr);
    state.setProperty ("pushResidual", pushResidual, nullptr);
    if (lastLearn.valid)
    {
        state.setProperty ("learnBands", bandsToString (lastLearn.bandsDb), nullptr);
        state.setProperty ("learnLufs", lastInLufs, nullptr);
        state.setProperty ("learnSide", lastSideRatio, nullptr);
        state.setProperty ("trackEff", bandsToString (trackEfficiency), nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MasterProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto t = ValueTree::fromXml (*xml);
    glueRefLufs.store ((float) (double) t.getProperty ("glueRef", t.getProperty ("refLufs", -18.0)));
    pushResidual = (float) (double) t.getProperty ("pushResidual", 0.0);
    apvts.replaceState (t);

    mp::Bands b {};
    if (stringToBands (t.getProperty ("learnBands").toString(), b))
    {
        lastLearn.valid = true; lastLearn.bandsDb = b;
        lastInLufs    = (float) (double) t.getProperty ("learnLufs", -20.0);
        lastSideRatio = (float) (double) t.getProperty ("learnSide", -1.0);
        if (! stringToBands (t.getProperty ("trackEff").toString(), trackEfficiency)) trackEfficiency.fill (1.0f);
        phase = Phase::Done;
        // republie la correction déléguée aux pistes (sans toucher aux réglages sauvegardés)
        const int genre = (int) param ("genre"), output = (int) param ("output");
        auto tg = mp::styleTarget (genre, output);
        targetLufs = tg.lufs;
        lastCorrection = mp::solveMaster (lastLearn, lastInLufs, lastSideRatio, genre, output, param ("amount"), tg, 1.0f - param ("masterShare"));
        publishMixCorrection (lastCorrection.trackDb);
        lastGenre = genre; lastOutput = output; lastAmount = param ("amount"); lastShare = param ("masterShare");
        lastTarget = (int) param ("target"); lastRefLevel = param ("refLevel") > 0.5f ? 1 : 0;
    }

    const String path = t.getProperty ("refPath").toString();
    if (path.isNotEmpty() && File (path).existsAsFile()) loadReference (File (path), false);
}
