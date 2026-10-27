#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Shared/ProEQ.h"
#include <juce_dsp/juce_dsp.h>
#include "../Shared/DSP.h"
#include "../Shared/Analysis.h"
#include "../Shared/Solver.h"
#include "../Shared/SharedBus.h"
#include "../Shared/Reference.h"
#include "../Shared/Review.h"

class MasterProcessor : public juce::AudioProcessor,
                        private juce::Timer
{
public:
    MasterProcessor();
    ~MasterProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }
    const juce::String getName() const override      { return "MixPilot Master"; }
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

    // -------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    mp::ProEQ proEq;                       // bandes ajoutées à la main
    mp::ProEqParams proModel { apvts, proEq };

    mp::SpectrumAnalyzer inAnalyzer, outAnalyzer;
    mp::LoudnessMeter    inMeter, outMeter;
    mp::PeakHold         truePeak, inputPeak, truePeakReview;
    std::atomic<float>   glueGrDb { 0.0f };

    // Assistant ---------------------------------------------------------------
    enum class Phase { Idle, WaitTracks, Listening, Verifying, Settling, Done, NoSignal };
    void   startAssistant();
    void   cancelAssistant();
    Phase  getPhase() const           { return phase; }
    float  getPhaseProgress() const;
    juce::String getStatusText() const;
    bool   hasLearned() const         { return lastLearn.valid; }
    const mp::LearnResult& getLastLearn() const { return lastLearn; }
    const mp::MasterCorrection& getLastCorrection() const { return lastCorrection; }
    float  getTargetLufs() const      { return targetLufs; }
    float  getTrackShare() const;     // part réellement confiée aux pistes

    // A/B à volume égal
    void setCompare (bool on);
    bool isComparing() const          { return comparing.load(); }

    // Référence ---------------------------------------------------------------
    void loadReference (const juce::File&, bool applyCorrection = true);
    void clearReference();
    std::shared_ptr<const mp::ReferenceData> getReference() const { return refShared; }
    bool   isLoadingReference() const { return refLoading.load(); }
    float  getReferenceProgress() const { return refProgress.load(); }
    juce::String getReferenceError() const { const juce::SpinLock::ScopedLockType l (pendingLock); return refError; }
    void   setReferenceListening (bool on) { refListen.store (on); }
    bool   isReferenceListening() const    { return refListen.load(); }
    void   setReferenceMatch (bool on)     { refMatch.store (on); }
    bool   isReferenceMatched() const      { return refMatch.load(); }
    void   seekReference (double fraction);
    double getReferencePosition() const;
    bool   referenceIsTarget() const;

    struct TrackInfo { juce::String name; int instrument; float trimDb; int phase; float loudness; bool hasProfile; float duckDb; };
    std::vector<TrackInfo> getTracks() const;

    juce::StringArray getTips() const;
    mp::MixReview buildReview() const;   // révision complète du mix (pistes + global)

    float eqResponseDb (float f) const { return eq.responseDb (f); }
    float getBandFreq (int band) const { return mp::octaveEQFreq (band) * std::exp2 (eqShiftParams[band]->load()); }
    float getBandGainDb (int band) const { return eqGainParams[band]->load(); }
    // valeur proposée par la dernière analyse (0 dB tant que l'Assistant n'a pas tourné)
    float getAnalysisBandGainDb (int band) const { return lastLearn.valid ? lastCorrection.eqDb[(size_t) band] : 0.0f; }
    float getLimiterGr() const { return limiter.gainReductionDb.load(); }
    double getCurrentSampleRate() const { return currentSR; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void applyCorrection (const mp::MasterCorrection&);
    void setParam (const juce::String& id, float value);
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void resolveFromLearned();
    void publishMixCorrection (const mp::Bands&);
    void installReference (std::unique_ptr<mp::ReferenceData>, bool apply);

    double currentSR = 48000.0;
    int    maxBlock  = 512;

    mp::SmoothEQ<11> eq;
    std::atomic<float>* eqGainParams[mp::kBands] {};
    std::atomic<float>* eqShiftParams[mp::kBands] {};
    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2, ap2;
    mp::Compressor compLow, compMid, compHigh;
    juce::AudioBuffer<float> bandLow, bandMid, bandHigh, dryDelayBuf;
    mp::BiquadState sideHp; mp::BiquadCoeffs sideHpCoeffs; float sideHpFreq = -1;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    mp::LookaheadLimiter limiter, safetyLimiter;          // 2e étage : rattrape les micro-dépassements
    mp::TruePeakDetector tpDetector, tpDetector2, tpMeter;
    std::vector<float> levelBuf;
    juce::AudioBuffer<float> meterBuf;
    juce::SmoothedValue<float> pushGain, compareGain, refMix, refGain;
    int dryDelayPos = 0, latency = 0;

    std::atomic<bool>  comparing { false };
    // mesures stéréo de la sortie (moyennes glissantes ~3 s)
    std::atomic<float> stLL { 0 }, stRR { 0 }, stLR { 0 }, stMM { 0 }, stSS { 0 };
    std::atomic<float> glueRefLufs { -18.0f };

    // mesure de largeur stéréo pendant l'écoute
    std::atomic<bool>   msLearning { false };
    std::atomic<double> sumMid { 0.0 }, sumSide { 0.0 };
    float lastSideRatio = -1.0f;

    // référence
    std::shared_ptr<mp::ReferenceData> refShared;      // côté interface
    const mp::ReferenceData* refAudio = nullptr;       // côté audio (protégé par refLock)
    juce::SpinLock refLock;
    std::atomic<int64_t> refPos { 0 };
    std::atomic<bool> refListen { false }, refMatch { true }, refLoading { false }, refCancel { false };
    std::atomic<float> refProgress { 0.0f };
    juce::String refError, refPathToLoad;
    std::unique_ptr<mp::ReferenceData> refPending;
    mutable juce::SpinLock pendingLock;
    bool refApplyOnInstall = true;
    float inPeakHold = -100.0f, truePeakHoldForReview = -100.0f;
    juce::ThreadPool loaderPool { 1 };

    // assistant
    Phase   phase = Phase::Idle;
    int64_t phaseStart = 0, phaseLength = 0;
    mp::LearnResult lastLearn;
    mp::MasterCorrection lastCorrection;
    float   lastInLufs = -70.0f, pushResidual = 0.0f, targetLufs = -14.0f;
    int     settleIter = 0;
    int     lastGenre = -1, lastOutput = -1, lastTarget = -1, lastRefLevel = -1;
    float   lastAmount = -1.0f, lastShare = -1.0f;
    int     lastRefCount = -1, lastProfiledTracks = -1;
    float   limGrAvg = 0.0f;
    mp::Bands trackEfficiency { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };   // part de la demande réellement obtenue par les pistes
    bool    verifyLearning = false;
    uint64_t masterId = mp::makeInstanceId();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};
