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
        static const char* w[] = { "sub (30 Hz)", "bass (60 Hz)", "upper bass (125 Hz)", "low mids (250 Hz)",
                                   "warm mids (500 Hz)", "mids (1 kHz)", "upper mids (2 kHz)", "presence (4 kHz)",
                                   "brilliance (8 kHz)", "air (16 kHz)" };
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
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "output", 1 }, "Output", outputs, mp::Streaming));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "amount", 1 }, "Amount", NormalisableRange<float> (0, 1), 0.8f, pc));

    for (int b = 0; b < mp::kBands; ++b)
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + String (b), 1 }, "EQ " + String (mp::kBandFreqs[(size_t) b], 0) + " Hz",
                                                      NormalisableRange<float> (-6, 6, 0.01f), 0.0f, dB));
    // Décalage de fréquence des bandes (points déplacés à la souris sur le spectre)
    auto oct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 2) + " oct"; });
    for (int b = 0; b < mp::kBands; ++b)
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqShift" + String (b), 2 }, "EQ freq " + String (mp::kBandFreqs[(size_t) b], 0) + " Hz",
                                                      NormalisableRange<float> (-mp::kMaxBandShiftOct, mp::kMaxBandShiftOct, 0.001f), 0.0f, oct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "highTame", 1 }, "Extreme highs", NormalisableRange<float> (-6, 0, 0.01f), 0.0f, dB));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "eqOn", 1 }, "EQ on", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glue", 1 }, "Glue", NormalisableRange<float> (0, 1), 0.4f, pc));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "dynOn", 1 }, "Dynamics on", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0, 2), 1.0f, pc));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "monoBass", 1 }, "Mono bass", NormalisableRange<float> (0, 300, 1), 0.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1 ? String ("Off") : String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "imageOn", 1 }, "Image on", true));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "warmth", 1 }, "Warmth", NormalisableRange<float> (0, 1), 0.2f, pc));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "push", 1 }, "Push", NormalisableRange<float> (0, 24, 0.01f), 0.0f, dB));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "clip", 1 }, "Clip", NormalisableRange<float> (0, 1), 0.4f, pc));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling", NormalisableRange<float> (-3, 0, 0.01f), -1.0f, dB));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (10, 500, 1, 0.5f), 100.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "maxOn", 1 }, "Maximizer on", true));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));

    // Nouveautés : référence et répartition
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Target", StringArray { "Style", "Reference" }, 0));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "refLevel", 1 }, "Match reference loudness", false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "masterShare", 1 }, "Split (Master share)", NormalisableRange<float> (0, 1), 0.5f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return "Tracks " + String (roundToInt ((1 - v) * 100)) + " % / Master " + String (roundToInt (v * 100)) + " %"; })));
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
    for (int b = 0; b < mp::kBands; ++b)
    {
        eqGainParams[b]  = apvts.getRawParameterValue ("eq" + String (b));
        eqShiftParams[b] = apvts.getRawParameterValue ("eqShift" + String (b));
    }

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
        mp::Bands g {}, shift {};
        for (int b = 0; b < mp::kBands; ++b)
        {
            g[(size_t) b]     = eqGainParams[b]->load();
            shift[(size_t) b] = eqShiftParams[b]->load();
        }
        mp::configureOctaveEQ (eq, 0, g, eqOn, shift);
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
        case Phase::Idle:       return u ("Run the Assistant during the loudest part of the song");
        case Phase::WaitTracks: return u ("1/3 · Tracks are analysing and correcting themselves…");
        case Phase::Listening:  return u ("2/3 · Listening to the full mix…");
        case Phase::Verifying:  return u ("2/3 · Checking after track corrections…");
        case Phase::Settling:   return u ("3/3 · Setting the final loudness…");
        case Phase::Done:       return u ("Done. Adjust to taste, everything stays editable.");
        case Phase::NoSignal:   return u ("No sound detected: start playback and try again");
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
            if (refPending == nullptr && ! refCancel.load()) refError = err.isEmpty() ? u ("Read error") : err;
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
        tips.add (u ("Add MixPilot Track at the end of every track: the Master can then correct the tracks directly (see SPLIT)."));
    if (! lastLearn.valid && phase == Phase::Idle)
        tips.add (u ("Run the ASSISTANT during the loudest part of the song to get a complete first setting."));
    if (phase == Phase::NoSignal)
        tips.add (u ("No sound was heard: start playback in Ableton, then run the ASSISTANT again."));

    if (lastLearn.valid)
    {
        if (push > 12.0f)
            tips.add (u ("The Master has to push by +") + String (push, 1) + u (" dB: your mix arrives very quiet. Raise the tracks (or let MixPilot Track do it) to squash the sound less."));
        if (limGrAvg > 6.0f)
            tips.add (u ("The maximizer is reducing by about ") + String (limGrAvg, 1) + u (" dB: the sound may get squashed. Pick a quieter output or lower PUSH."));
        if (glueGrDb.load() > 5.0f)
            tips.add (u ("GLUE is compressing hard: lower it if the mix loses punch."));
        if (phase == Phase::Done && integ > -60.0f && std::abs (integ - targetLufs) > 1.0f)
            tips.add (u ("Loudness is ") + String (integ - targetLufs, 1) + u (" LU from the target: run the ASSISTANT again on the loudest part, or adjust PUSH."));

        // déséquilibre spectral restant
        int worst = -1; float w = 0;
        for (int b = 0; b < mp::kBands; ++b)
            if (std::abs (lastCorrection.mixDiff[(size_t) b]) > std::abs (w)) { w = lastCorrection.mixDiff[(size_t) b]; worst = b; }
        if (worst >= 0 && std::abs (w) > 2.5f)
            tips.add (u ("Compared with the target, your mix has ") + (w > 0 ? u ("too little ") : u ("too much ")) + u (bandWord (worst))
                      + u (": the correction is shared according to the SPLIT slider."));
    }

    if (inPeakHold > 0.5f)
        tips.add (u ("The mix hits the Master above 0 dBFS (") + String (inPeakHold, 1) + u (" dB): lower the Master fader or the tracks to keep headroom."));

    if (lastLearn.valid && mp::outputIsNormalised ((int) param ("output")))
        tips.add (u ("Output ") + u (op.name) + u (": the song is set to ") + String (targetLufs, 0)
                  + u (" LUFS because the platform turns louder songs down. For a louder sound in Ableton, pick SoundCloud, Club or CD."));
    if (lastLearn.valid && phase == Phase::Done && getLimiterGr() < 0.5f && limGrAvg < 0.5f && ! mp::outputIsNormalised ((int) param ("output")))
        tips.add (u ("The maximizer is barely working: raise PUSH or CLIP for more loudness, or run the ASSISTANT again."));
    if ((int) param ("output") == mp::Club && param ("monoBass") < 1.0f)
        tips.add (u ("For club play, turn on MONO BASS (around 120 Hz): the low end will stay solid on the PA."));

    // kick / basse
    bool kick = false, bassNoDuck = false;
    for (auto& t : tracks)
    {
        if (t.instrument == mp::Kick && t.hasProfile) kick = true;
        if (t.instrument == mp::Bass && t.hasProfile && t.duckDb < 0.1f) bassNoDuck = true;
    }
    if (kick && bassNoDuck)
        tips.add (u ("Kick and bass are fighting over the low end: turn on SPACE on the bass track (MixPilot Track)."));

    if (refShared != nullptr)
    {
        if ((int) param ("target") == 0)
            tips.add (u ("A reference is loaded: set TARGET = Reference so the Assistant follows it."));
        else if (param ("refLevel") < 0.5f && refShared->lufs > targetLufs + 3.0f)
            tips.add (u ("Your reference is at ") + String (refShared->lufs, 1) + u (" LUFS, your output aims for ") + String (targetLufs, 0)
                      + u (" LUFS: that's normal, platforms turn loud songs down. Compare with LISTEN REF (level matched)."));
    }
    else if (lastLearn.valid)
        tips.add (u ("Tip: drop a song you like into REFERENCE to aim for its balance and compare at equal loudness."));

    if (lastLearn.valid && ! tracks.empty() && param ("masterShare") > 0.95f)
        tips.add (u ("SPLIT is fully right: the Master corrects alone. Slide towards TRACKS so the tracks responsible correct themselves (more natural sound)."));

    if (tips.isEmpty())
        tips.add (u ("Everything looks good. Compare with A/B (level matched) to judge the result without being fooled by loudness."));
    return tips;
}

//==============================================================================
//  Révision du mix
//==============================================================================
mp::MixReview MasterProcessor::buildReview() const
{
    mp::MixReview rv;
    rv.sections = { { u ("Overall mix"), {} }, { u ("Tracks"), {} }, { u ("Track conflicts"), {} } };
    enum { G = 0, T = 1, C = 2 };
    auto freqTxt = [] (float f) { return f >= 1000 ? String (f / 1000, f >= 10000 ? 0 : 0) + " kHz" : String ((int) f) + " Hz"; };
    auto sgn = [] (float v) { return (v >= 0 ? "+" : "") + String (v, 1); };

    const auto& op = mp::outputProfile ((int) param ("output"));
    const float integ = outMeter.getIntegrated();

    // ------------------------------------------------------------------ global
    if (! lastLearn.valid)
        rv.add (G, mp::SevWarn, "Assistant", u ("Not run yet: run the ASSISTANT on the loudest part to set up tracks and master."));
    else
        rv.add (G, mp::SevOk, "Assistant", u ("Set up for ") + u (mp::genreProfile ((int) param ("genre")).name) + " / " + u (op.name) + ".");

    if (integ > -60.0f)
    {
        const float d = integ - targetLufs;
        if (std::abs (d) <= 1.0f) rv.add (G, mp::SevOk, "Final loudness", String (integ, 1) + u (" LUFS, on target (") + String (targetLufs, 1) + ").");
        else rv.add (G, mp::SevWarn, "Final loudness", String (integ, 1) + u (" LUFS, ") + sgn (d) + u (" LU from the target. Run the ASSISTANT again on the loudest part, or adjust PUSH."));
    }
    else rv.add (G, mp::SevInfo, "Final loudness", u ("Play a few seconds to measure (RESET button to start over)."));

    {
        const float tp = truePeakHoldForReview;
        if (tp > -99.0f)
        {
            if (tp > op.ceilingDb + 0.05f) rv.add (G, mp::SevProblem, u ("Peaks"), u ("True peak ") + String (tp, 1) + u (" dBTP above the ceiling (") + String (op.ceilingDb, 1) + u ("): check that MAXIMIZER is on and lower PUSH."));
            else rv.add (G, mp::SevOk, u ("Peaks"), u ("Max true peak ") + String (tp, 1) + u (" dBTP, under the ceiling."));
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
            static const char* zone[] = { "sub", "bass", "upper bass", "low mids", "warm mids", "mids", "upper mids", "presence", "brilliance", "air" };
            if (worst >= 0 && std::abs (wv) > 3.0f)
                rv.add (G, mp::SevWarn, u ("Balance"), (wv > 0 ? u ("Too much ") : u ("Too little ")) + u (zone[worst]) + u (" (") + freqTxt (mp::kBandFreqs[(size_t) worst]) + ", " + sgn (wv)
                        + u (" dB vs target). Run the ASSISTANT again; if it persists, check below which track dominates this range."));
            else rv.add (G, mp::SevOk, u ("Balance"), u ("Close to the ") + (referenceIsTarget() ? u ("reference") : u ("style")) + u (" target: no range off by more than 3 dB."));
        }
    }

    // stéréo
    {
        const float ll = stLL.load(), rr = stRR.load(), lr = stLR.load(), mm = stMM.load(), ss = stSS.load();
        if (mm > 1e-6f)
        {
            const float corr = lr / std::sqrt (std::max (1e-12f, ll * rr));
            const float side = ss / mm;
            if (corr < 0.0f) rv.add (G, mp::SevProblem, u ("Stereo"), u ("Negative correlation (") + String (corr, 2) + u ("): phase problem, the sound may vanish in mono. Check stereo wideners on the tracks, and lower WIDTH."));
            else if (side < 0.02f) rv.add (G, mp::SevInfo, u ("Stereo"), u ("Mix is almost mono: it lacks space. Raise WIDTH, or add stereo to pads and effects."));
            else if (side > 0.45f) rv.add (G, mp::SevWarn, u ("Stereo"), u ("Very wide mix (risky in mono and in clubs). Lower WIDTH or turn on MONO BASS."));
            else rv.add (G, mp::SevOk, u ("Stereo"), u ("Healthy width, correlation ") + String (corr, 2) + ".");
        }
        if ((int) param ("output") == mp::Club && param ("monoBass") < 1.0f)
            rv.add (G, mp::SevWarn, u ("Club low end"), u ("MONO BASS is off: turn it on (120 Hz) for a solid low end on the PA."));
    }

    // dynamique
    if (limGrAvg > 6.0f) rv.add (G, mp::SevWarn, "Compression", u ("The maximizer reduces by ") + String (limGrAvg, 1) + u (" dB on average: the sound may get squashed. Lower PUSH, raise CLIP, or pick a quieter output."));
    else if (glueGrDb.load() > 5.0f) rv.add (G, mp::SevWarn, "Compression", u ("GLUE is compressing hard: lower it if the mix lacks punch."));
    else if (lastLearn.valid) rv.add (G, mp::SevOk, "Compression", u ("Dynamics under control (maximizer ") + String (limGrAvg, 1) + u (" dB on average)."));
    if (inPeakHold > 0.5f) rv.add (G, mp::SevWarn, "Headroom", u ("The mix hits the Master above 0 dBFS: lower the Master fader or the tracks by a few dB."));

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
        if (s.bypassed.load()) { rv.add (T, mp::SevInfo, title, u ("BYPASS on: this track is not being corrected.")); continue; }
        if (! t.prof) { rv.add (T, mp::SevWarn, title, u ("Not analysed yet: run the ASSISTANT or click LISTEN on the track while it plays.")); continue; }

        StringArray notes; int sev = mp::SevOk;
        const float inPk = s.inputPeakDb.load();
        if (inPk > 0.5f) { sev = mp::SevProblem; notes.add (u ("input is clipping (") + String (inPk, 1) + u (" dBFS): lower the instrument or effect before MixPilot")); }
        if (! s.manual.load() && s.confidence.load() < 0.6f) { sev = std::max (sev, (int) mp::SevWarn); notes.add (u ("uncertain detection: check INSTRUMENT")); }
        const float trim = s.trimDb.load();
        const float level = s.outIntegrated.load(), tgt = s.targetLufs.load();
        if (level > -60.0f && std::abs (level - tgt) > 2.0f)
        { sev = std::max (sev, (int) mp::SevWarn); notes.add (u ("level ") + sgn (level - tgt) + u (" dB from its place in the mix (GAIN off, limit reached or track changed since analysis: listen again)")); }
        if (trim > 12.0f) { sev = std::max (sev, (int) mp::SevInfo); notes.add (u ("very quiet source (+") + String (trim, 1) + u (" dB): raise it in the instrument")); }
        if (trim < -12.0f) { sev = std::max (sev, (int) mp::SevInfo); notes.add (u ("very loud source (") + String (trim, 1) + u (" dB): lower it in the instrument")); }
        if (s.gainReductionDb.load() > 8.0f) { sev = std::max (sev, (int) mp::SevWarn); notes.add (u ("heavy compression: lower the track's AMOUNT")); }
        if (! s.following.load()) notes.add (u ("FOLLOW MASTER off"));
        if (t.mask < -1.0f && t.maskBand >= 0) notes.add (u ("makes ") + String (-t.mask, 1) + u (" dB of room around ") + freqTxt (mp::kBandFreqs[(size_t) t.maskBand]));
        if (t.inst == mp::Bass && t.duck > 0.05f) notes.add (u ("ducks under the kick (") + String (t.duck, 1) + " dB)");
        if (t.inst == mp::Instrumental && t.vduck > 0.05f) notes.add (u ("dips under the vocal (") + String (t.vduck, 1) + " dB)");
        if (std::abs (s.mixShareDb.load()) > 0.3f) notes.add (u ("applies ") + sgn (s.mixShareDb.load()) + u (" dB requested by the Master"));
        rv.add (T, sev, title, u ("Gain ") + sgn (trim) + u (" dB") + (notes.isEmpty() ? u (". Nothing to report.") : "; " + notes.joinIntoString ("; ") + "."));
    }
    if (tl.empty())
        rv.add (T, mp::SevInfo, u ("No linked tracks"), u ("Add MixPilot Track at the end of every track for a track-by-track review."));

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
            const String where = u (" around ") + freqTxt (mp::kBandFreqs[(size_t) best]);
            const bool kb = (A.inst == mp::Kick && B.inst == mp::Bass) || (A.inst == mp::Bass && B.inst == mp::Kick);
            const bool iv = (A.inst == mp::Instrumental && mp::isVoice (B.inst)) || (B.inst == mp::Instrumental && mp::isVoice (A.inst));
            const auto& bass = A.inst == mp::Bass ? A : B;
            const auto& inst = A.inst == mp::Instrumental ? A : B;
            const auto& low  = mp::instrumentProfile (A.inst).priority < mp::instrumentProfile (B.inst).priority ? A : B;
            if (kb)
                rv.add (C, bass.duck > 0.05f ? mp::SevOk : mp::SevWarn, pair, u ("Fighting over the low end") + where
                        + (bass.duck > 0.05f ? u (": handled, the bass ducks under the kick.") : u (": turn on SPACE on the bass track.")));
            else if (iv)
                rv.add (C, inst.vduck > 0.05f ? mp::SevOk : mp::SevWarn, pair, u ("Vocal and instrumental overlap") + where
                        + (inst.vduck > 0.05f ? u (": handled, the instrumental dips when the vocal sings.") : u (": turn on SPACE on the full instrumental.")));
            else
                rv.add (C, low.mask < -0.9f ? mp::SevOk : mp::SevInfo, pair, u ("Same frequency range") + where
                        + (low.mask < -0.9f ? u (": handled, ") + low.name + u (" makes room.") : u (": if it sounds muddy, lower one of them a bit or change octave.")));
            ++nConf;
        }
    if (nConf == 0 && tl.size() > 1) rv.add (C, mp::SevOk, u ("No conflicts"), u ("Linked tracks occupy different ranges."));

    // mashup : la voix passe-t-elle au-dessus de l'instru ?
    for (auto& v : tl)
        for (auto& in : tl)
            if (mp::isVoice (v.inst) && in.inst == mp::Instrumental && v.prof && in.prof)
            {
                const float vp = 0.5f * (v.ob[6] + v.ob[7]), ip = 0.5f * (in.ob[6] + in.ob[7]) - in.vduck;
                const float margin = vp - ip;
                if (margin < 0.0f)
                    rv.add (C, mp::SevWarn, v.name + u (" over ") + in.name, u ("The vocal may get buried in the 2 – 4 kHz range (") + sgn (margin)
                            + u (" dB): raise the a cappella by ") + String (std::ceil (-margin) + 1, 0) + u (" dB or raise the instrumental's AMOUNT."));
                else if (margin > 12.0f)
                    rv.add (C, mp::SevInfo, v.name + u (" over ") + in.name, u ("The vocal clearly dominates (") + sgn (margin) + u (" dB in the presence range): you can lower it a bit to blend it better."));
                else
                    rv.add (C, mp::SevOk, v.name + u (" over ") + in.name, u ("The vocal sits well above the instrumental (") + sgn (margin) + u (" dB in the presence range)."));
            }

    // ------------------------------------------------------------------ note
    rv.score = juce::jlimit (0, 100, 100 - 15 * rv.problems - 6 * rv.warnings);
    rv.verdict = rv.score >= 90 ? u ("Mix ready: nothing blocking.")
               : rv.score >= 70 ? u ("Good mix: a few points to check.")
               : rv.score >= 50 ? u ("Needs work: several points to fix.")
                                : u ("Major problems: fix the red items first.");
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
