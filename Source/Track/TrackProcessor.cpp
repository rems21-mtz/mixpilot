#include "TrackProcessor.h"
#include "TrackEditor.h"

using namespace juce;

namespace
{
    constexpr int kSolveWaitMs = 800;    // laisse les autres pistes publier leur profil
    constexpr int kSettleMs    = 4000;
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout TrackProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pc = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)) + " %"; });

    StringArray insts { "Auto" };
    for (int i = 0; i < mp::NumInstruments; ++i) insts.add (String::fromUTF8 (mp::instrumentProfile (i).name));
    StringArray genres { "Follow Master" };
    for (int i = 0; i < mp::NumGenres; ++i) genres.add (String::fromUTF8 (mp::genreProfile (i).name));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "instrument", 1 }, "Instrument", insts, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "genre", 1 }, "Style", genres, 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "amount", 1 }, "Amount", NormalisableRange<float> (0, 1), 0.8f, pc));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "gainOn", 1 }, "Gain on", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "eqOn", 1 },   "EQ on", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "compOn", 1 }, "Compression on", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "duckOn", 1 }, "Space on", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "bypass", 1 }, "Bypass", false));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "followMaster", 1 }, "Follow Master corrections", true));

    // Retouches manuelles de l'EQ (points déplacés à la souris), ajoutées par-dessus l'analyse
    auto dB  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    auto oct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 2) + " oct"; });
    for (int b = 0; b < mp::kBands; ++b)
    {
        const String hz = String (mp::kBandFreqs[(size_t) b], 0) + " Hz";
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqAdj" + String (b), 2 }, "EQ edit " + hz,
                                                      NormalisableRange<float> (-24, 24, 0.01f), 0.0f, dB));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqShift" + String (b), 2 }, "EQ freq " + hz,
                                                      NormalisableRange<float> (-mp::kMaxBandShiftOct, mp::kMaxBandShiftOct, 0.001f), 0.0f, oct));
    }
    return l;
}

TrackProcessor::TrackProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRACK", createLayout())
{
    duckLp.setType (dsp::LinkwitzRileyFilterType::lowpass);
    duckHp.setType (dsp::LinkwitzRileyFilterType::highpass);
    vLp1.setType (dsp::LinkwitzRileyFilterType::lowpass);  vHp1.setType (dsp::LinkwitzRileyFilterType::highpass);
    vLp2.setType (dsp::LinkwitzRileyFilterType::lowpass);  vHp2.setType (dsp::LinkwitzRileyFilterType::highpass);
    vAp2.setType (dsp::LinkwitzRileyFilterType::allpass);
    slot = mp::claimSlot (instanceId);
    lastListenRequest = mp::bus().listenRequest.load();
    startTimerHz (30);
}

TrackProcessor::~TrackProcessor()
{
    stopTimer();
    mp::releaseSlot (slot, instanceId);
}

bool TrackProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == AudioChannelSet::stereo() || o == AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void TrackProcessor::prepareToPlay (double sr, int block)
{
    currentSR = sr;
    const int nCh = jlimit (1, 2, getTotalNumOutputChannels());
    eq.prepare (sr);
    comp.prepare (sr);
    dsp::ProcessSpec spec { sr, (uint32) jmax (block, 32), (uint32) nCh };
    for (auto* f : { &duckLp, &duckHp }) { f->prepare (spec); f->setCutoffFrequency (180.0f); }
    for (auto* f : { &vLp1, &vHp1 }) { f->prepare (spec); f->setCutoffFrequency (800.0f); }
    for (auto* f : { &vLp2, &vHp2, &vAp2 }) { f->prepare (spec); f->setCutoffFrequency (5000.0f); }
    vDuckGain = 1.0f;
    vDuckAtt = std::exp (-1.0f / (float) (0.015 * sr));
    vDuckRel = std::exp (-1.0f / (float) (0.250 * sr));
    vHpCoeff = 1.0f - std::exp (-MathConstants<float>::twoPi * 900.0f / (float) sr);
    vLpCoeff = 1.0f - std::exp (-MathConstants<float>::twoPi * 4500.0f / (float) sr);
    vEnvAtt  = std::exp (-1.0f / (float) (0.005 * sr));
    vEnvRel  = std::exp (-1.0f / (float) (0.150 * sr));
    trimGain.reset (sr, 0.3);
    trimGain.setCurrentAndTargetValue (mp::dbToGain (appliedTrim.load()));
    duckGain = 1.0f;
    duckAtt  = std::exp (-1.0f / (float) (0.002 * sr));
    duckRel  = std::exp (-1.0f / (float) (0.090 * sr));
    kickLpCoeff  = 1.0f - std::exp (-MathConstants<float>::twoPi * 150.0f / (float) sr);
    kickEnvRel   = std::exp (-1.0f / (float) (0.080 * sr));
    kickMaxDecay = std::exp (-1.0f / (float) (8.0 * sr));
    inMeter.prepare (sr); outMeter.prepare (sr);
    inAnalyzer.prepare (sr); outAnalyzer.prepare (sr);
    applyCorrection();
}

//==============================================================================
void TrackProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = jmin (2, getTotalNumOutputChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0) return;
    auto* const* ch = buffer.getArrayOfWritePointers();

    inMeter.process (ch, nCh, n);
    inAnalyzer.push (ch, nCh, n);
    {
        float pk = 0; for (int c = 0; c < nCh; ++c) pk = jmax (pk, buffer.getMagnitude (c, 0, n));
        inputPeak.push (pk);
    }

    const int inst = resolvedInstrument.load();

    // Enveloppe du kick, publiée pour les basses
    if (inst == mp::Kick && slot >= 0)
    {
        float blockEnv = 0;
        for (int i = 0; i < n; ++i)
        {
            float m = 0; for (int c = 0; c < nCh; ++c) m += ch[c][i];
            kickLp += kickLpCoeff * (m / (float) nCh - kickLp);
            const float a = std::abs (kickLp);
            kickEnv = a > kickEnv ? a : a + kickEnvRel * (kickEnv - a);
            kickMax = jmax (kickEnv, kickMax * kickMaxDecay, 1e-4f);
            blockEnv = jmax (blockEnv, kickEnv / kickMax);
        }
        mp::bus().slots[slot].duckEnv.store (blockEnv);
    }

    // Enveloppe de présence d'une voix (900 Hz – 4,5 kHz), publiée pour les instrus complètes
    if (mp::isVoice (inst) && slot >= 0)
    {
        float blockEnv = 0;
        for (int i = 0; i < n; ++i)
        {
            float m = 0; for (int c = 0; c < nCh; ++c) m += ch[c][i];
            m /= (float) nCh;
            vHpState += vHpCoeff * (m - vHpState);
            const float band = m - vHpState;
            vLpState += vLpCoeff * (band - vLpState);
            const float a = std::abs (vLpState);
            vEnv = a > vEnv ? a + vEnvAtt * (vEnv - a) : a + vEnvRel * (vEnv - a);
            vMax = jmax (vEnv, vMax * kickMaxDecay, 1e-4f);
            blockEnv = jmax (blockEnv, vEnv / vMax);
        }
        mp::bus().slots[slot].duckEnv.store (blockEnv);
    }

    if (param ("bypass") > 0.5f)
    {
        outMeter.process (ch, nCh, n); outAnalyzer.push (ch, nCh, n);
        return;
    }

    // 1) Coupe-bas + EQ
    eq.process (buffer);

    // 2) Compression
    if (compActive.load() && param ("compOn") > 0.5f)
    {
        comp.setParams (compThr.load(), compRatio.load(), compAtt.load(), compRel.load(), 6.0f);
        comp.process (buffer);
    }
    else comp.gainReductionDb.store (0.0f);

    // 3) Espace : la basse se pousse quand le kick frappe
    const float depth = duckDepth.load();
    if (inst == mp::Bass && depth > 0.05f && param ("duckOn") > 0.5f)
    {
        float kick = 0;
        auto& b = mp::bus();
        for (int s = 0; s < mp::kMaxTracks; ++s)
        {
            auto& sl = b.slots[s];
            if (s == slot || sl.owner.load (std::memory_order_relaxed) == 0) continue;
            if (sl.instrument.load (std::memory_order_relaxed) != mp::Kick) continue;
            if (! mp::isAlive (sl.heartbeat.load (std::memory_order_relaxed))) continue;
            kick = jmax (kick, sl.duckEnv.load (std::memory_order_relaxed));
        }
        const float target = mp::dbToGain (-depth * jlimit (0.0f, 1.0f, kick));
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            duckGain = target < duckGain ? target + duckAtt * (duckGain - target) : target + duckRel * (duckGain - target);
            minG = jmin (minG, duckGain);
            for (int c = 0; c < nCh; ++c)
            {
                const float x = ch[c][i];
                const float lo = duckLp.processSample (c, x);
                const float hi = duckHp.processSample (c, x);
                ch[c][i] = lo * duckGain + hi;
            }
        }
        duckGrDb.store (-mp::gainToDb (minG));
    }
    else duckGrDb.store (0.0f);

    // 3b) Instru complète : les médiums se creusent quand la voix chante (place pour l'acapella)
    const float vDepth = voiceDuckDepth.load();
    if (inst == mp::Instrumental && vDepth > 0.05f && param ("duckOn") > 0.5f)
    {
        float voice = 0;
        auto& b = mp::bus();
        for (int s = 0; s < mp::kMaxTracks; ++s)
        {
            auto& sl = b.slots[s];
            if (s == slot || sl.owner.load (std::memory_order_relaxed) == 0) continue;
            if (! mp::isVoice (sl.instrument.load (std::memory_order_relaxed))) continue;
            if (! mp::isAlive (sl.heartbeat.load (std::memory_order_relaxed))) continue;
            voice = jmax (voice, sl.duckEnv.load (std::memory_order_relaxed));
        }
        // la voix doit être franchement présente pour déclencher (évite de pomper sur les respirations)
        const float drive  = jlimit (0.0f, 1.0f, (voice - 0.15f) / 0.6f);
        const float target = mp::dbToGain (-vDepth * drive);
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            vDuckGain = target < vDuckGain ? target + vDuckAtt * (vDuckGain - target) : target + vDuckRel * (vDuckGain - target);
            minG = jmin (minG, vDuckGain);
            for (int c = 0; c < nCh; ++c)
            {
                const float x = ch[c][i];
                const float lo = vAp2.processSample (c, vLp1.processSample (c, x));
                const float rest = vHp1.processSample (c, x);
                const float mid = vLp2.processSample (c, rest);
                const float hi  = vHp2.processSample (c, rest);
                ch[c][i] = lo + mid * vDuckGain + hi;
            }
        }
        voiceDuckGrDb.store (-mp::gainToDb (minG));
    }
    else voiceDuckGrDb.store (0.0f);

    // 4) Gain de placement
    trimGain.setTargetValue (param ("gainOn") > 0.5f ? mp::dbToGain (trimTarget.load()) : 1.0f);
    if (trimGain.isSmoothing() || std::abs (trimGain.getTargetValue() - 1.0f) > 1e-5f)
        for (int i = 0; i < n; ++i) { const float g = trimGain.getNextValue(); for (int c = 0; c < nCh; ++c) ch[c][i] *= g; }

    outMeter.process (ch, nCh, n);
    outAnalyzer.push (ch, nCh, n);
}

//==============================================================================
int TrackProcessor::getEffectiveGenre() const
{
    const int g = (int) param ("genre");
    if (g > 0) return g - 1;
    return mp::masterPresent() ? mp::bus().genre.load() : mp::Pop;
}

int TrackProcessor::getEffectiveOutput() const
{
    return mp::masterPresent() ? mp::bus().output.load() : mp::Streaming;
}

void TrackProcessor::startListening (int seconds)
{
    inAnalyzer.startLearning();
    inMeter.resetIntegrated();
    phase = Phase::Listening;
    phaseStart = mp::nowMs();
    phaseLength = jlimit (3, 30, seconds) * 1000;
    trimResidual = 0.0f;
}

float TrackProcessor::getPhaseProgress() const
{
    if (phaseLength <= 0) return 0.0f;
    return jlimit (0.0f, 1.0f, (float) (mp::nowMs() - phaseStart) / (float) phaseLength);
}

String TrackProcessor::getStatusText() const
{
    switch (phase)
    {
        case Phase::Idle:      return String::fromUTF8 ("Click LISTEN while the track plays (or run the Master's Assistant)");
        case Phase::Listening: return String::fromUTF8 ("Listening to the track…");
        case Phase::Solving:   return String::fromUTF8 ("Comparing with the other tracks…");
        case Phase::Settling:  return String::fromUTF8 ("Setting the level…");
        case Phase::Locked:    return String::fromUTF8 ("Corrected for: ") + String::fromUTF8 (mp::genreProfile (getEffectiveGenre()).name)
                                      + " / " + String::fromUTF8 (mp::outputProfile (getEffectiveOutput()).name);
        case Phase::NoSignal:  return String::fromUTF8 ("No sound detected: start playback and try again");
    }
    return {};
}

void TrackProcessor::solve()
{
    if (! learned.valid) return;

    const int choice = (int) param ("instrument");
    const int inst   = choice > 0 ? choice - 1 : detection.instrument;
    resolvedInstrument.store (inst);

    std::vector<mp::OtherTrack> others;
    bool kickPresent = false, voicePresent = false;
    for (int s = 0; s < mp::kMaxTracks; ++s)
    {
        if (s == slot) continue;
        auto& sl = mp::bus().slots[s];
        if (sl.owner.load() == 0 || ! mp::isAlive (sl.heartbeat.load()) || ! sl.hasProfile.load()) continue;
        mp::OtherTrack o { sl.instrument.load(), {} };
        for (int b = 0; b < mp::kBands; ++b) o.bandsDb[(size_t) b] = sl.bands[b].load();
        if (o.instrument == mp::Kick) kickPresent = true;
        if (mp::isVoice (o.instrument)) voicePresent = true;
        others.push_back (o);
    }

    correction = mp::solveTrack (inst, learned, learnedLufs, getEffectiveGenre(), getEffectiveOutput(), param ("amount"), others, kickPresent, voicePresent);

    // Ce que cette piste apporte au mix après ses propres corrections (retouches manuelles comprises)
    const float trimNow = correction.trimDb + trimResidual;
    auto withEdits = correction;
    for (int k = 0; k < mp::kBands; ++k) withEdits.eqDb[(size_t) k] += userEqDb (k);
    const auto myOut = mp::estimateOutBands (learned.bandsDb, withEdits, trimNow);
    if (slot >= 0)
        for (int k = 0; k < mp::kBands; ++k) mp::bus().slots[slot].outBands[k].store (myOut[(size_t) k]);

    // Part de correction demandée par le Master, répartie selon la contribution de chaque piste
    correction.mixDb = {};
    if (receivesMasterCorrection())
    {
        mp::Bands D {};
        for (int k = 0; k < mp::kBands; ++k) D[(size_t) k] = mp::bus().mixCorrection[k].load();
        std::vector<mp::Bands> outs { myOut };
        for (int s = 0; s < mp::kMaxTracks; ++s)
        {
            if (s == slot) continue;
            auto& sl = mp::bus().slots[s];
            if (sl.owner.load() == 0 || ! mp::isAlive (sl.heartbeat.load()) || ! sl.hasProfile.load()) continue;
            mp::Bands ob {};
            bool ok = true;
            for (int k = 0; k < mp::kBands; ++k) { ob[(size_t) k] = sl.outBands[k].load(); ok = ok && ob[(size_t) k] > -99.0f; }
            if (ok) outs.push_back (ob);
        }
        correction.mixDb = mp::shareMixCorrection (outs, 0, D);
    }

    lastSolveMs = mp::nowMs();
    applyCorrection();
}

void TrackProcessor::applyCorrection()
{
    const auto& c = correction;
    const bool haveSolution = learned.valid;
    const bool eqOn = param ("eqOn") > 0.5f && haveSolution;

    mp::Bands g {}, shift {};
    for (int k = 0; k < mp::kBands; ++k)
    {
        g[(size_t) k] = getBandGainDb (k);
        shift[(size_t) k] = param (("eqShift" + String (k)).toRawUTF8());
    }
    mp::configureOctaveEQ (eq, 0, g, eqOn, shift);
    editsApplied = currentEdits();
    eq.setBand (10, mp::BiquadCoeffs::HighPass, c.hpfHz, 0.7071f, 0.0f, eqOn && c.hpfHz > 21.0f);

    compThr.store (c.compThresholdDb); compRatio.store (c.compRatio);
    compAtt.store (c.compAttackMs);    compRel.store (c.compReleaseMs);
    compActive.store (haveSolution && c.compRatio > 1.01f);
    duckDepth.store (haveSolution ? c.duckDepthDb : 0.0f);
    voiceDuckDepth.store (haveSolution ? c.voiceDuckDb : 0.0f);

    const float trim = haveSolution ? jlimit (-30.0f, 24.0f, c.trimDb + trimResidual) : 0.0f;
    trimTarget.store (trim);
    appliedTrim.store (trim);
}

//==============================================================================
void TrackProcessor::timerCallback()
{
    inAnalyzer.update();
    outAnalyzer.update();

    auto& b = mp::bus();
    if (slot < 0) slot = mp::claimSlot (instanceId);   // nouvelle tentative si le bus était plein
    if (slot >= 0)
    {
        auto& s = b.slots[slot];
        if (s.owner.load() != instanceId) slot = mp::claimSlot (instanceId);   // emplacement repris
    }

    // Demande d'écoute globale venant du Master
    const int req = b.listenRequest.load();
    if (req != lastListenRequest)
    {
        lastListenRequest = req;
        if (param ("bypass") < 0.5f) startListening (b.listenSeconds.load());
    }

    // Un point d'EQ a bougé (souris, automation) : on l'applique tout de suite
    if (learned.valid && currentEdits() != editsApplied) applyCorrection();

    const int64_t elapsed = mp::nowMs() - phaseStart;
    switch (phase)
    {
        case Phase::Listening:
            if (elapsed >= phaseLength)
            {
                inAnalyzer.stopLearning();
                const auto r = inAnalyzer.getResult();
                const float L = inMeter.getIntegrated();
                if (! r.valid || L < -65.0f) { phase = Phase::NoSignal; phaseLength = 0; break; }
                learned = r; learnedLufs = L;
                detection = mp::classify (r);
                const int choice = (int) param ("instrument");
                resolvedInstrument.store (choice > 0 ? choice - 1 : detection.instrument);
                if (slot >= 0)
                {
                    auto& s = b.slots[slot];
                    for (int k = 0; k < mp::kBands; ++k) s.bands[k].store (r.bandsDb[(size_t) k]);
                    s.instrument.store (resolvedInstrument.load());
                    s.learnedLoudness.store (L);
                    s.hasProfile.store (1);
                }
                phase = Phase::Solving; phaseStart = mp::nowMs(); phaseLength = kSolveWaitMs;
            }
            break;

        case Phase::Solving:
            if (elapsed >= phaseLength)
            {
                solve();
                outMeter.resetIntegrated();
                phase = Phase::Settling; phaseStart = mp::nowMs(); phaseLength = kSettleMs;
            }
            break;

        case Phase::Settling:
            if (elapsed >= phaseLength)
            {
                // Le gain final tient compte de l'EQ et de la compression réellement appliqués
                const float out = outMeter.getIntegrated();
                if (out > -65.0f && param ("gainOn") > 0.5f)
                {
                    const float desired = learnedLufs + correction.trimDb;
                    trimResidual = jlimit (-8.0f, 8.0f, desired - out);
                    applyCorrection();
                }
                phase = Phase::Locked; phaseLength = 0;
            }
            break;

        case Phase::Locked:
        case Phase::Idle:
        case Phase::NoSignal:
            // recalcul léger : style, intensité, instrument ou autres pistes ont pu changer
            if (learned.valid && (mp::nowMs() - lastSolveMs > 500 || b.mixCorrectionId.load() != lastMixId))
            {
                lastMixId = b.mixCorrectionId.load();
                solve();
            }
            break;
    }
    inPeakHold = jmax (inputPeak.takeDb(), inPeakHold - 0.2f);

    if (slot >= 0)
    {
        auto& s = b.slots[slot];
        s.heartbeat.store (mp::nowMs());
        s.instrument.store (resolvedInstrument.load());
        s.phase.store (phase == Phase::Listening ? mp::PhaseListening
                     : (phase == Phase::Solving || phase == Phase::Settling) ? mp::PhaseSettling
                     : phase == Phase::Locked ? mp::PhaseLocked : mp::PhaseIdle);
        s.loudness.store (outMeter.getShortTerm());
        s.trimDb.store (appliedTrim.load());
        s.gainReductionDb.store (comp.gainReductionDb.load());
        for (int k = 0; k < mp::kBands; ++k) s.eq[k].store (getBandGainDb (k));
        s.duckDb.store (param ("duckOn") > 0.5f ? duckDepth.load() : 0.0f);
        s.inputPeakDb.store (inPeakHold);
        s.confidence.store (detection.confidence);
        s.manual.store ((int) param ("instrument") > 0 ? 1 : 0);
        s.targetLufs.store (correction.targetLufs);
        s.outIntegrated.store (outMeter.getIntegrated());
        {
            int wb = -1; float w = 0, mx = 0;
            for (int k = 0; k < mp::kBands; ++k)
            {
                if (correction.maskDb[(size_t) k] < w) { w = correction.maskDb[(size_t) k]; wb = k; }
                if (std::abs (correction.mixDb[(size_t) k]) > std::abs (mx)) mx = correction.mixDb[(size_t) k];
            }
            s.maskDb.store (w); s.maskBand.store (wb); s.mixShareDb.store (mx);
        }
        s.voiceDuckDb.store (param ("duckOn") > 0.5f ? voiceDuckDepth.load() : 0.0f);
        s.bypassed.store (param ("bypass") > 0.5f ? 1 : 0);
        s.following.store (param ("followMaster") > 0.5f ? 1 : 0);
        if (learned.valid && ! s.hasProfile.load())
        {
            for (int k = 0; k < mp::kBands; ++k) s.bands[k].store (learned.bandsDb[(size_t) k]);
            s.hasProfile.store (1);
        }
        const auto nm = getTrackName();
        s.setName ((nm.isNotEmpty() ? nm : String ("Track ") + String (slot + 1)).toRawUTF8());
    }
}

bool TrackProcessor::receivesMasterCorrection() const
{
    return mp::masterPresent() && param ("followMaster") > 0.5f && learned.valid;
}

StringArray TrackProcessor::getTips() const
{
    auto u = [] (const char* t) { return String::fromUTF8 (t); };
    StringArray tips;
    const auto& c = correction;
    const bool manual = (int) param ("instrument") > 0;

    if (param ("bypass") > 0.5f)
        tips.add (u ("BYPASS is on: this track is not being corrected."));
    if (! isLinked())
        tips.add (u ("Add MixPilot Master on the Master track: this track will then follow the style, output and mix corrections."));
    if (! learned.valid)
        tips.add (u ("Click LISTEN while the track plays (8 s), or run the ASSISTANT from the Master to analyse every track at once."));
    if (phase == Phase::NoSignal)
        tips.add (u ("No sound was heard: start playback (the track must be playing), then try again."));
    if (inPeakHold > 0.5f)
        tips.add (u ("Input is clipping (") + String (inPeakHold, 1) + u (" dBFS): lower the instrument or effect placed before MixPilot."));

    if (learned.valid)
    {
        if (! manual && detection.confidence < 0.6f)
            tips.add (u ("Uncertain detection (") + String (roundToInt (detection.confidence * 100)) + u (" %): check INSTRUMENT and pick it by hand if needed."));
        if (manual && detection.instrument != getResolvedInstrument())
            tips.add (u ("Instrument picked by hand. The analysis heard: ") + u (mp::instrumentProfile (detection.instrument).name) + ".");
        if (appliedTrim.load() > 12.0f)
            tips.add (u ("Very quiet source (+") + String (appliedTrim.load(), 1) + u (" dB): raise it in the instrument for a better signal-to-noise ratio."));
        if (appliedTrim.load() < -12.0f)
            tips.add (u ("Very loud source (") + String (appliedTrim.load(), 1) + u (" dB): lower it in the instrument to keep headroom."));
        if (comp.gainReductionDb.load() > 8.0f)
            tips.add (u ("Heavy compression: lower AMOUNT if the track loses life."));
        if (getResolvedInstrument() == mp::Bass && c.duckDepthDb > 0.05f && param ("duckOn") < 0.5f)
            tips.add (u ("Turn on SPACE: the bass will make way for the kick (clearer low end)."));
        if (getResolvedInstrument() == mp::Bass && c.duckDepthDb <= 0.05f && isLinked())
            tips.add (u ("No linked kick: put MixPilot Track on your kick so the bass makes room for it automatically."));

        int worst = -1; float w = 0;
        for (int b = 0; b < mp::kBands; ++b) if (std::abs (c.mixDb[(size_t) b]) > std::abs (w)) { w = c.mixDb[(size_t) b]; worst = b; }
        if (worst >= 0 && std::abs (w) > 0.3f)
        {
            const float f = mp::kBandFreqs[(size_t) worst];
            tips.add (u ("The Master asks for ") + (w > 0 ? "+" : "") + String (w, 1) + u (" dB around ")
                      + (f >= 1000 ? String (f / 1000, 0) + " kHz" : String ((int) f) + " Hz")
                      + u (" on this track to balance the mix (purple bars)."));
        }
        if (getResolvedInstrument() == mp::Instrumental && c.voiceDuckDb <= 0.05f && isLinked())
            tips.add (u ("Mashup: put MixPilot Track on the a cappella (instrument \"A cappella\"): this instrumental will make room for it automatically."));
        if (getResolvedInstrument() == mp::Instrumental && c.voiceDuckDb > 0.05f && param ("duckOn") < 0.5f)
            tips.add (u ("Turn on SPACE: the instrumental will dip its mids when the vocal sings."));
        if (getResolvedInstrument() == mp::Vocal && ! manual)
            tips.add (u ("If this is a full a cappella (vocal from another song), set INSTRUMENT = A cappella to give it priority."));
        if (isLinked() && param ("followMaster") < 0.5f)
            tips.add (u ("FOLLOW MASTER is off: this track ignores the corrections requested by the Master."));
    }
    if (tips.isEmpty())
        tips.add (u ("Everything looks good. Tip: hover over any element for an explanation, or click ?."));
    return tips;
}

void TrackProcessor::updateTrackProperties (const TrackProperties& p)
{
    if (p.name.isNotEmpty())
    {
        const SpinLock::ScopedLockType sl (nameLock);
        trackName = p.name;
    }
}

//==============================================================================
float TrackProcessor::userEqDb (int band) const
{
    return param (("eqAdj" + String (band)).toRawUTF8());
}

float TrackProcessor::getAnalysisBandGainDb (int band) const
{
    return jlimit (-9.0f, 6.0f, correction.eqDb[(size_t) band] + correction.mixDb[(size_t) band]);
}

float TrackProcessor::getBandGainDb (int band) const
{
    return jlimit (-12.0f, 12.0f, getAnalysisBandGainDb (band) + userEqDb (band));
}

float TrackProcessor::getBandFreq (int band) const
{
    return mp::octaveEQFreq (band) * std::exp2 (param (("eqShift" + String (band)).toRawUTF8()));
}

bool TrackProcessor::isBandEdited (int band) const
{
    return std::abs (userEqDb (band)) > 0.05f || std::abs (param (("eqShift" + String (band)).toRawUTF8())) > 0.005f;
}

bool TrackProcessor::isEqEditable() const
{
    return learned.valid && param ("eqOn") > 0.5f && param ("bypass") < 0.5f;
}

TrackProcessor::EqEdits TrackProcessor::currentEdits() const
{
    EqEdits e {};
    for (int k = 0; k < mp::kBands; ++k)
    {
        e[(size_t) (2 * k)]     = userEqDb (k);
        e[(size_t) (2 * k + 1)] = param (("eqShift" + String (k)).toRawUTF8());
    }
    return e;
}

//==============================================================================
AudioProcessorEditor* TrackProcessor::createEditor() { return new TrackEditor (*this); }

void TrackProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree lt ("LEARNED");
    lt.setProperty ("valid", learned.valid, nullptr);
    lt.setProperty ("lufs", learnedLufs, nullptr);
    lt.setProperty ("residual", trimResidual, nullptr);
    lt.setProperty ("detected", detection.instrument, nullptr);
    lt.setProperty ("confidence", detection.confidence, nullptr);
    lt.setProperty ("secs", learned.activeSeconds, nullptr);
    String bands;
    for (auto v : learned.bandsDb) bands << String (v, 2) << " ";
    lt.setProperty ("bands", bands.trim(), nullptr);
    lt.setProperty ("feat", String (learned.low, 4) + " " + String (learned.lowmid, 4) + " " + String (learned.mid, 4) + " "
                          + String (learned.high, 4) + " " + String (learned.centroidHz, 1) + " " + String (learned.onsetRate, 3) + " "
                          + String (learned.sustain, 3) + " " + String (learned.modulation, 3) + " "
                          + String (learned.flatness, 4) + " " + String (learned.attackFrames, 2) + " " + String (learned.meanFrameDb, 2), nullptr);
    state.removeChild (state.getChildWithName ("LEARNED"), nullptr);
    state.appendChild (lt, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TrackProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = ValueTree::fromXml (*xml);
    const auto lt = state.getChildWithName ("LEARNED");
    apvts.replaceState (state);

    if (lt.isValid() && (bool) lt.getProperty ("valid", false))
    {
        StringArray b; b.addTokens (lt.getProperty ("bands").toString(), " ", "");
        StringArray f; f.addTokens (lt.getProperty ("feat").toString(), " ", "");
        if (b.size() == mp::kBands && f.size() >= 8)
        {
            for (int k = 0; k < mp::kBands; ++k) learned.bandsDb[(size_t) k] = b[k].getFloatValue();
            learned.low = f[0].getFloatValue(); learned.lowmid = f[1].getFloatValue(); learned.mid = f[2].getFloatValue();
            learned.high = f[3].getFloatValue(); learned.centroidHz = f[4].getFloatValue(); learned.onsetRate = f[5].getFloatValue();
            learned.sustain = f[6].getFloatValue(); learned.modulation = f[7].getFloatValue();
            if (f.size() >= 10) { learned.flatness = f[8].getFloatValue(); learned.attackFrames = f[9].getFloatValue(); }
            if (f.size() >= 11) learned.meanFrameDb = f[10].getFloatValue();
            learned.activeSeconds = (float) (double) lt.getProperty ("secs", 8.0);
            learned.valid = true;
            learnedLufs  = (float) (double) lt.getProperty ("lufs", -70.0);
            trimResidual = (float) (double) lt.getProperty ("residual", 0.0);
            detection.instrument = (int) lt.getProperty ("detected", (int) mp::Other);
            detection.confidence = (float) (double) lt.getProperty ("confidence", 0.5);
            phase = Phase::Locked;
            solve();
        }
    }
}
