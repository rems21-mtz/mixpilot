#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../Shared/DSP.h"
#include "../Shared/Analysis.h"
#include "../Shared/Solver.h"
#include "../Shared/SharedBus.h"

class TrackProcessor : public juce::AudioProcessor,
                       private juce::Timer
{
public:
    TrackProcessor();
    ~TrackProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }
    const juce::String getName() const override      { return "MixPilot Track"; }
    bool acceptsMidi() const override                { return false; }
    bool producesMidi() const override               { return false; }
    bool isMidiEffect() const override               { return false; }
    double getTailLengthSeconds() const override     { return 0.0; }
    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void updateTrackProperties (const TrackProperties&) override;

    // -------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    mp::SpectrumAnalyzer inAnalyzer, outAnalyzer;
    mp::LoudnessMeter inMeter, outMeter;
    mp::PeakHold inputPeak;

    enum class Phase { Idle, Listening, Solving, Settling, Locked, NoSignal };
    void   startListening (int seconds);
    Phase  getPhase() const               { return phase; }
    float  getPhaseProgress() const;
    juce::String getStatusText() const;

    int    getResolvedInstrument() const  { return resolvedInstrument.load(); }
    mp::Detection getDetection() const    { return detection; }
    bool   hasProfile() const             { return learned.valid; }
    const mp::LearnResult& getLearned() const { return learned; }
    mp::TrackCorrection getCorrection() const { return correction; }
    float  getLearnedLufs() const         { return learnedLufs; }
    float  getAppliedTrimDb() const       { return appliedTrim.load(); }
    float  getCompGr() const              { return comp.gainReductionDb.load(); }
    float  getDuckGr() const              { return duckGrDb.load(); }
    float  getVoiceDuckGr() const         { return voiceDuckGrDb.load(); }
    int    getEffectiveGenre() const;
    int    getEffectiveOutput() const;
    bool   isLinked() const               { return mp::masterPresent(); }
    juce::String getTrackName() const     { const juce::SpinLock::ScopedLockType sl (nameLock); return trackName; }

    float  eqResponseDb (float f) const   { return eq.responseDb (f); }
    juce::StringArray getTips() const;
    bool   receivesMasterCorrection() const;
    double getCurrentSampleRate() const   { return currentSR; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void solve();
    void applyCorrection();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    double currentSR = 48000.0;
    uint64_t instanceId = mp::makeInstanceId();
    int slot = -1;
    juce::String trackName;
    mutable juce::SpinLock nameLock;

    // DSP
    mp::SmoothEQ<11> eq;              // 10 octaves + coupe-bas
    mp::Compressor comp;
    juce::dsp::LinkwitzRileyFilter<float> duckLp, duckHp;
    // instru complète : bande médium 800 Hz – 5 kHz creusée quand la voix chante
    juce::dsp::LinkwitzRileyFilter<float> vLp1, vHp1, vLp2, vHp2, vAp2;
    float vDuckGain = 1.0f, vDuckAtt = 0.0f, vDuckRel = 0.0f;
    std::atomic<float> voiceDuckGrDb { 0.0f }, voiceDuckDepth { 0.0f };
    // enveloppe de présence (si cette piste est une voix)
    float vHpState = 0.0f, vLpState = 0.0f, vHpCoeff = 0.0f, vLpCoeff = 0.0f, vEnv = 0.0f, vEnvAtt = 0.0f, vEnvRel = 0.0f, vMax = 1e-4f;
    juce::SmoothedValue<float> trimGain;
    float duckGain = 1.0f, duckAtt = 0.0f, duckRel = 0.0f;
    std::atomic<float> duckGrDb { 0.0f };
    // enveloppe du kick (si cette piste est un kick)
    float kickLp = 0.0f, kickLpCoeff = 0.0f, kickEnv = 0.0f, kickEnvRel = 0.0f, kickMax = 1e-4f, kickMaxDecay = 0.0f;

    // état partagé avec l'audio
    std::atomic<int>   resolvedInstrument { mp::Other };
    std::atomic<float> trimTarget { 0.0f }, appliedTrim { 0.0f }, duckDepth { 0.0f };
    std::atomic<bool>  compActive { false };
    std::atomic<float> compThr { 0.0f }, compRatio { 1.0f }, compAtt { 10.0f }, compRel { 100.0f };

    // apprentissage
    Phase   phase = Phase::Idle;
    int64_t phaseStart = 0, phaseLength = 0;
    int     lastListenRequest = 0;
    mp::LearnResult learned;
    mp::Detection   detection;
    mp::TrackCorrection correction;
    float   learnedLufs = -70.0f, trimResidual = 0.0f;
    int64_t lastSolveMs = 0;
    int     lastMixId = -1;
    float   inPeakHold = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackProcessor)
};
