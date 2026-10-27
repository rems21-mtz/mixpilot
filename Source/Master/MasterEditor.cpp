#include "MasterEditor.h"

using namespace juce;
using namespace mp::ui;

namespace
{
    String u (const char* s) { return String::fromUTF8 (s); }
    String instrumentName (int i) { return u (mp::instrumentProfile (i).name); }
    const char* eqLabels[] = { "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k", "AIR" };

    std::vector<HelpOverlay::Section> masterHelp()
    {
        return {
            { u ("Quick start"),
              u ("1. Put MixPilot Track at the end of every track, and MixPilot Master at the end of the Master track.\n"
                 "2. Pick the STYLE and the OUTPUT (where the song will be released).\n"
                 "3. Start playback on the loudest part, then click ASSISTANT (30 to 40 s).\n"
                 "4. Listen, compare with A/B, then adjust to taste: everything stays editable.") },
            { u ("Style and Output"),
              u ("STYLE sets the target balance (lows, mids, highs), the compression density and the stereo width.\n"
                 "OUTPUT sets the final loudness (LUFS) and the peak ceiling (dBTP). Streaming (-14) and YouTube (-13) are fixed because these platforms turn loud songs down. "
                 "SoundCloud, Club, CD and Vinyl follow the style's habits (e.g. electro: SoundCloud -8, Club -7, CD -8).\n"
                 "Changing either one after the Assistant recalculates everything instantly, tracks included, without listening again.") },
            { u ("Amount"),
              u ("How much correction the Assistant applies: 0 % = none, 100 % = full correction. 70 to 85 % gives a natural result.") },
            { u ("Assistant"),
              u ("Step 1: every linked track listens (8 s), recognises its instrument and corrects itself.\n"
                 "Step 2: the Master listens to the resulting mix and compares it with the target (style or reference).\n"
                 "Step 3: it sets the final loudness for the chosen output, in several passes.\n"
                 "Click again to stop. Run it again after a big change in the song.") },
            { u ("A/B and Bypass"),
              u ("A/B plays the original sound AT THE SAME LOUDNESS as the processed sound: you judge quality, not volume (louder always seems better).\n"
                 "BYPASS turns the Master off completely.") },
            { u ("Spectrum"),
              u ("Grey: what comes in. Blue: what goes out. Orange dots: the target (style or reference). Purple: the loaded reference. Green: the Master EQ correction.\n"
                 "The display is tilted so that a balanced mix looks roughly flat.\n"
                 "Each EQ band has a point on the green curve: drag it up/down for gain, left/right for frequency (Shift = fine). "
                 "Edited points turn orange. Double-click a point to return it to the Assistant's value, right-click to reset all bands.") },
            { u ("Free EQ bands"),
              u ("Double-click anywhere on the spectrum to add your own EQ band (up to 12), on top of the analysis. Each band has its colour and number.\n"
                 "Drag a band: left/right = frequency, up/down = gain. Mouse wheel over a band = Q (width). Shift = fine. Double-click a band = gain back to 0. "
                 "Alt-click or Delete = remove it. Right-click a band for its type (bell, shelves, low/high cut with slope, notch), channel (Stereo, Mid, Side, Left, Right), "
                 "dynamic EQ, solo listening and more.\n"
                 "Click a band to open its panel: power, note of the frequency, FREQ/GAIN/Q knobs, DYN (dynamic EQ: THRESH, RANGE, RATIO, ATTACK, RELEASE) and SOLO to hear only its zone.\n"
                 "The white curve is the total EQ (analysis + your bands).") },
            { u ("Analyzer"),
              u ("Buttons at the top right of the spectrum: PRE (before processing), POST (after), BOTH, DELTA (what the processing changes, after minus before). "
                 "FREEZE holds the current spectrum. PIANO shows a keyboard under the spectrum, with the note of the selected band lit up.") },
            { u ("Loudness"),
              u ("INTEGRATED LUFS: average perceived loudness since the last RESET (the value Spotify, YouTube… measure).\n"
                 "Short term: loudness of the last 3 seconds. True peak: the real peak, including between samples.\n"
                 "Both dots turn green when loudness and peaks meet the chosen output.") },
            { u ("Reference"),
              u ("Drop a song (WAV, AIFF, FLAC, MP3, M4A) on the REFERENCE panel, or click LOAD.\n"
                 "MixPilot measures its spectral balance, loudness, peaks, dynamics (LRA) and stereo width.\n"
                 "LISTEN REF replaces your output with the reference for comparison. With MATCH, it plays at the same loudness as your mix (a fair comparison).\n"
                 "Click the waveform to jump to a position in the song.") },
            { u ("Target"),
              u ("STYLE: the Assistant aims for the typical balance of the chosen style.\n"
                 "REFERENCE: it aims for the spectral balance and stereo width of your reference.\n"
                 "REF LVL: also aims for the reference's loudness instead of the output's. Peaks stay under the output ceiling.") },
            { u ("Split Tracks ↔ Master"),
              u ("When the mix drifts from the target (for example too much low-mids), there are two ways to correct it:\n"
                 "• TRACKS side: the tracks responsible for those frequencies correct themselves, in proportion to what they contribute (more natural, every track keeps its place);\n"
                 "• MASTER side: the Master EQ corrects the whole mix at once (simpler, more global).\n"
                 "The slider sets each side's share. In the middle it's half and half. With no linked track, the Master corrects everything.\n"
                 "Tracks show their share in purple in their EQ card.") },
            { u ("EQ"),
              u ("10 octave bands set by the Assistant to bring the mix closer to the target, plus AIR (extreme highs above 10 kHz: up for more air, down to soften, useful for vinyl). Double-click: back to 0.\n"
                 "You can also drag the points directly on the spectrum, which moves these sliders and lets you shift each band's frequency.") },
            { u ("Dynamics"),
              u ("GLUE: gentle 3-band compression that holds the mix together. Higher = denser, but less punch. GR = current gain reduction.\n"
                 "WARMTH: light saturation that adds density and roundness.") },
            { u ("Image"),
              u ("WIDTH: widens or narrows the stereo image (100 % = unchanged).\n"
                 "MONO BASS: makes everything below the chosen frequency mono. Essential for clubs and vinyl.") },
            { u ("Maximizer"),
              u ("PUSH: gain sent into the limiter; this is what raises the loudness.\n"
                 "CLIP: soft clipping of very short peaks before the limiter (an electro mastering technique): more loudness without pumping. Too high and the sound gets harsh.\n"
                 "CEILING: maximum allowed peak (true peak, measured between samples).\n"
                 "RELEASE: how fast the limiter recovers. Short = louder but more aggressive.") },
            { u ("Mix review"),
              u ("The MIX REVIEW button (Linked tracks panel) gives a full report, updated live during playback:\n"
                 "• OVERALL MIX: final loudness, peaks, balance vs target, stereo and phase, compression, headroom.\n"
                 "• TRACKS: for each one, its gain, any clipping, its detection, the room it makes for others, the correction requested by the Master.\n"
                 "• CONFLICTS: tracks fighting over the same frequencies (kick/bass, vocal/instrumental…) and whether it's already handled. For a mashup, it also checks that the vocal sits above the instrumental.\n"
                 "Green = OK, blue = info, orange = check, red = problem. The score out of 100 sums it all up.") },
            { u ("Linked tracks"),
              u ("List of the MixPilot Track instances in the project, with their instrument and the gain they apply. Green dot = track analysed and corrected.") },
            { u ("Tips"),
              u ("The TIP bar analyses the situation continuously (loudness, peaks, kick/bass, reference…) and suggests what to do. Click it to see the next tip.") },
            { u ("Good practice"),
              u ("Keep headroom: the mix reaching the Master should not exceed 0 dBFS.\n"
                 "Always judge with A/B at equal loudness. Take breaks: your ears get used to the sound.\n"
                 "Check on several systems (headphones, speakers, phone).") },
        };
    }
}

//==============================================================================
void RefWaveform::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panelHi); g.fillRoundedRectangle (r, 4.0f);
    auto ref = proc.getReference();

    if (proc.isLoadingReference())
    {
        g.setColour (col::purple.withAlpha (0.25f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * proc.getReferenceProgress()), 4.0f);
        g.setColour (col::text); g.setFont (font (11.0f, true));
        g.drawText (u ("Analysing reference… ") + String (roundToInt (proc.getReferenceProgress() * 100)) + " %", r, Justification::centred);
    }
    else if (ref != nullptr && ! ref->overview.empty())
    {
        const float mid = r.getCentreY();
        const int n = (int) ref->overview.size();
        g.setColour (col::purple.withAlpha (0.75f));
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) n;
            const float h = ref->overview[(size_t) i] * r.getHeight() * 0.45f;
            g.drawVerticalLine ((int) x, mid - h, mid + h);
        }
        const float px = r.getX() + r.getWidth() * (float) proc.getReferencePosition();
        g.setColour (proc.isReferenceListening() ? col::text : col::textDim);
        g.fillRect (px - 1.0f, r.getY(), 2.0f, r.getHeight());
    }
    else
    {
        g.setColour (col::textDim); g.setFont (font (11.5f));
        g.drawText (u ("Drop a reference song here (WAV, AIFF, FLAC, MP3, M4A)"), r, Justification::centred);
    }

    if (dragHover)
    {
        g.setColour (col::purple); g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }
}

//==============================================================================
MasterView::MasterView (MasterProcessor& p) : proc (p)
{
    setLookAndFeel (&look);
    auto& vts = proc.apvts;

    for (auto* z : { &statusZone, &loudZone, &tracksZone, &refInfoZone, &corrZone }) addAndMakeVisible (z);
    statusZone.setTooltip (u ("Assistant status and the chosen output's target (loudness in LUFS and peak ceiling in dBTP)."));
    loudZone.setTooltip (u ("Loudness readings of your output. Integrated LUFS = average perceived loudness since the last RESET. Green dots mean the output is compliant."));
    tracksZone.setTooltip (u ("Tracks with MixPilot Track on them. Green dot = analysed and corrected. Right: the gain applied to the track."));
    refInfoZone.setTooltip (u ("Reference info: integrated loudness (LUFS), peak (dBTP), loudness range (LRA) and duration."));
    corrZone.setTooltip (u ("Choose what the Assistant aims for (style or reference) and who corrects the mix (the tracks, the Master, or both)."));

    // --- En-tête -------------------------------------------------------------
    for (int i = 0; i < mp::NumGenres; ++i)  genreBox.addItem  (u (mp::genreProfile (i).name), i + 1);
    for (int i = 0; i < mp::NumOutputs; ++i) outputBox.addItem (u (mp::outputProfile (i).name), i + 1);
    addAndMakeVisible (genreBox); addAndMakeVisible (outputBox);
    genreAtt  = std::make_unique<CA> (vts, "genre",  genreBox);
    outputAtt = std::make_unique<CA> (vts, "output", outputBox);
    genreBox.setTooltip (u ("Music style: sets the target balance, compression density and width. Linked tracks follow this choice."));
    outputBox.setTooltip (u ("Where the song will be released: sets the final loudness (LUFS) and the peak ceiling. E.g. Spotify -14 LUFS, club -8 LUFS."));

    amountSlider.setSliderStyle (Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (Slider::TextBoxRight, false, 44, 18);
    amountSlider.setColour (Slider::trackColourId, col::accent);
    amountSlider.setColour (Slider::backgroundColourId, col::border);
    amountSlider.setColour (Slider::thumbColourId, col::text);
    amountSlider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    amountSlider.setTooltip (u ("How much the Assistant corrects: 0 % = none, 100 % = full. 70 to 85 % sounds natural."));
    addAndMakeVisible (amountSlider);
    sliderAtt.push_back (std::make_unique<SA> (vts, "amount", amountSlider));

    assistantBtn.setColour (TextButton::buttonColourId, col::accent);
    assistantBtn.setTooltip (u ("Runs the full analysis (30 to 40 s) during the loudest part: the tracks correct themselves, then the Master sets the balance and loudness."));
    assistantBtn.onClick = [this]
    {
        const auto ph = proc.getPhase();
        if (ph == MasterProcessor::Phase::WaitTracks || ph == MasterProcessor::Phase::Listening || ph == MasterProcessor::Phase::Verifying) proc.cancelAssistant();
        else proc.startAssistant();
    };
    addAndMakeVisible (assistantBtn);

    stylePower (compareBtn);
    compareBtn.setColour (TextButton::buttonOnColourId, col::warn);
    compareBtn.setTooltip (u ("A/B: hear the original sound at the same loudness as the processed sound, for a fair judgement."));
    compareBtn.onClick = [this] { proc.setCompare (compareBtn.getToggleState()); };
    addAndMakeVisible (compareBtn);

    stylePower (bypassBtn);
    bypassBtn.setColour (TextButton::buttonOnColourId, col::bad);
    bypassBtn.setTooltip (u ("Turns off all Master processing."));
    addAndMakeVisible (bypassBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "bypass", bypassBtn));

    styleHelpButton (helpBtn);
    helpBtn.onClick = [this] { help->setVisible (true); help->toFront (true); };
    addAndMakeVisible (helpBtn);

    resetLufsBtn.setTooltip (u ("Resets the integrated loudness reading (to measure a new section)."));
    resetLufsBtn.onClick = [this] { proc.outMeter.resetIntegrated(); };
    addAndMakeVisible (resetLufsBtn);

    addAndMakeVisible (tipsBar);

    // --- Référence -----------------------------------------------------------
    loadRefBtn.setButtonText ("LOAD");
    loadRefBtn.setTooltip (u ("Pick a reference song (WAV, AIFF, FLAC, MP3, M4A). You can also drop it on the waveform."));
    loadRefBtn.onClick = [this] { chooseReferenceFile(); };
    clearRefBtn.setTooltip (u ("Removes the reference."));
    clearRefBtn.onClick = [this] { proc.clearReference(); };
    listenRefBtn.setButtonText (u ("LISTEN REF"));
    stylePower (listenRefBtn);
    listenRefBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.3f));
    listenRefBtn.setTooltip (u ("Replaces your output with the reference for comparison. Click again to return to your mix."));
    listenRefBtn.onClick = [this] { proc.setReferenceListening (listenRefBtn.getToggleState()); };
    matchRefBtn.setButtonText (u ("MATCH"));
    stylePower (matchRefBtn);
    matchRefBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.3f));
    matchRefBtn.setToggleState (proc.isReferenceMatched(), dontSendNotification);
    matchRefBtn.setTooltip (u ("Plays the reference at the same loudness as your mix: you compare the sound, not the volume."));
    matchRefBtn.onClick = [this] { proc.setReferenceMatch (matchRefBtn.getToggleState()); };
    waveform.setTooltip (u ("Reference waveform. Click to jump to a position. Drop a file here to load another one."));
    for (auto* c : std::initializer_list<Component*> { &loadRefBtn, &clearRefBtn, &listenRefBtn, &matchRefBtn, &waveform }) addAndMakeVisible (c);

    // --- Cible et répartition -------------------------------------------------
    targetBox.addItem ("Style", 1);
    targetBox.addItem ("Reference", 2);
    targetBox.setTooltip (u ("What the Assistant aims for: the typical balance of the STYLE, or that of your REFERENCE (spectrum and width)."));
    addAndMakeVisible (targetBox);
    targetAtt = std::make_unique<CA> (vts, "target", targetBox);

    refLevelBtn.setButtonText (u ("REF LVL"));
    stylePower (refLevelBtn);
    refLevelBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.3f));
    refLevelBtn.setTooltip (u ("Also aims for the reference's loudness (instead of the output's). Peaks stay under the output ceiling."));
    addAndMakeVisible (refLevelBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "refLevel", refLevelBtn));

    shareSlider.setSliderStyle (Slider::LinearHorizontal);
    shareSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    shareSlider.setColour (Slider::trackColourId, col::purple.withAlpha (0.6f));
    shareSlider.setColour (Slider::backgroundColourId, col::accent.withAlpha (0.6f));
    shareSlider.setColour (Slider::thumbColourId, col::text);
    shareSlider.setPopupDisplayEnabled (true, true, this);
    shareSlider.setTooltip (u ("SPLIT: who corrects the mix balance. Left: the responsible tracks correct themselves (more natural). Right: the Master EQ corrects everything at once. Middle: half and half."));
    addAndMakeVisible (shareSlider);
    sliderAtt.push_back (std::make_unique<SA> (vts, "masterShare", shareSlider));
    shareSlider.setDoubleClickReturnValue (true, 0.5);

    // --- Modules -------------------------------------------------------------
    struct P { TextButton* b; const char* id; const char* tip; };
    P powers[] = {
        { &eqOn,    "eqOn",    "Turns the Master EQ on/off. The sliders are set by the Assistant and stay editable." },
        { &dynOn,   "dynOn",   "Turns multiband compression (Glue) and saturation (Warmth) on/off." },
        { &imageOn, "imageOn", "Turns stereo processing (Width and Mono bass) on/off." },
        { &maxOn,   "maxOn",   "Turns the maximizer (Push + true peak limiter) on/off. When off, loudness is no longer matched to the output." } };
    for (auto& pw : powers)
    {
        stylePower (*pw.b);
        pw.b->setTooltip (u (pw.tip));
        addAndMakeVisible (*pw.b);
        buttonAtt.push_back (std::make_unique<BA> (vts, pw.id, *pw.b));
    }

    for (int i = 0; i <= mp::kBands; ++i)
    {
        auto& s = eqSliders[i];
        s.setSliderStyle (Slider::LinearVertical);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::trackColourId, i == mp::kBands ? col::warn : col::accent2);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTooltip (i == mp::kBands ? u ("AIR: shelf on the extreme highs (above 10 kHz). Up = more air and sparkle, down = softer (useful for vinyl or a harsh mix).")
                                      : u ("Band ") + eqLabels[i] + u (" Hz. Boost (+) or cut (−) this frequency range on the whole mix. Double-click: 0 dB."));
        addAndMakeVisible (s);
        sliderAtt.push_back (std::make_unique<SA> (vts, i < mp::kBands ? "eq" + String (i) : String ("highTame"), s));
        s.setDoubleClickReturnValue (true, 0.0);
    }

    addKnob (glue,     "glue",     u ("GLUE: gentle 3-band compression that holds the mix together. Higher = denser, but less punch."));
    addKnob (warmth,   "warmth",   u ("WARMTH: light saturation that adds density and roundness. Too high and the sound can get dull."), col::warn);
    addKnob (width,    "width",    u ("WIDTH: widens (> 100 %) or narrows (< 100 %) the stereo image. The low end stays stable."), col::accent2);
    addKnob (monoBass, "monoBass", u ("MONO BASS: makes everything below this frequency mono. Recommended for club (120 Hz) and vinyl (150 Hz)."), col::accent2);
    addKnob (push,     "push",     u ("PUSH: gain sent into the limiter. This is what raises the final loudness (set automatically by the Assistant)."));
    addKnob (clipKnob, "clip",     u ("CLIP: rounds off very short peaks (kick and snare attacks) before the limiter. More loudness without pumping; too high and the sound can get harsh. Electro: 60 to 80 %."), col::warn);
    addKnob (ceiling,  "ceiling",  u ("CEILING: maximum peak (true peak). -1 dBTP for streaming, -0.3 dBTP for club/CD."), col::bad);
    addKnob (release,  "release",  u ("RELEASE: how fast the limiter recovers. Short = louder but more aggressive; long = smoother."));

    // --- Spectre -------------------------------------------------------------
    spectrum.fftSize = mp::SpectrumAnalyzer::fftSize;
    spectrum.eqCurve = [this] (float f) { return proc.eqResponseDb (f); };
    spectrum.showTarget = true;
    spectrum.caption = u ("INPUT (grey) · OUTPUT (blue) · TARGET (dotted) · REFERENCE (purple) · CORRECTION (green)");
    spectrum.setTooltip (u ("Mix spectrum. Grey = input, blue = output, dotted = target, purple = reference, green = Master EQ correction. "
                            "Drag the EQ points to adjust them (Shift = fine, double-click = back to the Assistant). "
                            "Double-click an empty spot to add your own EQ band, right-click for more."));

    // Points d'EQ éditables : ce sont les mêmes paramètres que les curseurs de la section EQ
    spectrum.pointMinDb = -6.0f;
    spectrum.pointMaxDb = 6.0f;
    spectrum.eqPoints = [this]
    {
        std::vector<SpectrumView::EqPoint> pts;
        if (proc.apvts.getRawParameterValue ("eqOn")->load() < 0.5f || proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f) return pts;
        for (int b = 0; b < mp::kBands; ++b)
        {
            const float gain = proc.getBandGainDb (b);
            const bool edited = std::abs (gain - proc.getAnalysisBandGainDb (b)) > 0.05f
                             || std::abs (proc.apvts.getRawParameterValue ("eqShift" + String (b))->load()) > 0.005f;
            pts.push_back ({ proc.getBandFreq (b), gain, edited });
        }
        return pts;
    };
    auto setParam = [this] (const String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    spectrum.onPointMoved = [setParam] (int b, float freq, float gainDb)
    {
        setParam ("eq" + String (b), gainDb);
        setParam ("eqShift" + String (b), jlimit (-mp::kMaxBandShiftOct, mp::kMaxBandShiftOct, std::log2 (freq / mp::octaveEQFreq (b))));
    };
    spectrum.onPointGesture = [this] (int b, bool starting)
    {
        for (auto id : { "eq" + String (b), "eqShift" + String (b) })
            if (auto* p = proc.apvts.getParameter (id)) { if (starting) p->beginChangeGesture(); else p->endChangeGesture(); }
    };
    spectrum.onPointReset = [this, setParam] (int b)
    {
        setParam ("eq" + String (b), proc.getAnalysisBandGainDb (b));
        setParam ("eqShift" + String (b), 0.0f);
    };
    spectrum.onResetAll = [this, setParam]
    {
        for (int b = 0; b < mp::kBands; ++b) { setParam ("eq" + String (b), proc.getAnalysisBandGainDb (b)); setParam ("eqShift" + String (b), 0.0f); }
    };
    // EQ libre : bandes ajoutées par double-clic, panneau de la bande sélectionnée
    spectrum.pro = &proc.proModel;
    bandPanel = std::make_unique<BandPanel> (proc.apvts, proc.proModel);
    spectrum.addChildComponent (*bandPanel);
    spectrum.bandPanel = bandPanel.get();
    spectrum.onSelectBand = [this] (int k) { bandPanel->showBand (k); };
    bandPanel->onSelect   = [this] (int k) { spectrum.select (k); };
    spectrum.anaMode = (int) proc.apvts.state.getProperty ("anaMode", (int) SpectrumView::AnaBoth);
    spectrum.piano   = (bool) proc.apvts.state.getProperty ("piano", false);
    spectrum.onViewChanged = [this]
    {
        proc.apvts.state.setProperty ("anaMode", spectrum.anaMode, nullptr);
        proc.apvts.state.setProperty ("piano", spectrum.piano, nullptr);
    };
    addAndMakeVisible (spectrum);

    help = std::make_unique<HelpOverlay> (u ("MixPilot Master guide"), masterHelp());
    addChildComponent (*help);

    reviewBtn.setButtonText (u ("MIX REVIEW"));
    reviewBtn.setColour (TextButton::buttonColourId, col::accent.withAlpha (0.35f));
    reviewBtn.setTooltip (u ("Full mix report: every track (level, clipping, detection), conflicts between tracks, and the overall mix (loudness, peaks, balance, stereo, compression), with a score and what to do."));
    reviewBtn.onClick = [this] { review->setReview (proc.buildReview()); review->setVisible (true); review->toFront (true); };
    addAndMakeVisible (reviewBtn);
    review = std::make_unique<ReviewOverlay>();
    addChildComponent (*review);

    setSize (1040, 864);
    startTimerHz (30);
}

MasterView::~MasterView()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MasterView::addKnob (Slider& s, const char* id, const String& tip, Colour c)
{
    setupKnob (s, c);
    s.setTooltip (tip);
    addAndMakeVisible (s);
    sliderAtt.push_back (std::make_unique<SA> (proc.apvts, id, s));
}

void MasterView::chooseReferenceFile()
{
    chooser = std::make_unique<FileChooser> (u ("Choose a reference song"), File::getSpecialLocation (File::userMusicDirectory),
                                             "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) proc.loadReference (f);
                          });
}

bool MasterView::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;ogg")) return true;
    return false;
}

void MasterView::filesDropped (const StringArray& files, int, int)
{
    waveform.dragHover = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;ogg")) { proc.loadReference (File (f)); break; }
    repaint();
}

//==============================================================================
void MasterView::timerCallback()
{
    auto ref = proc.getReference();
    spectrum.setSpectra (proc.inAnalyzer.getDisplayDb(), proc.outAnalyzer.getDisplayDb());
    spectrum.sampleRate = proc.getCurrentSampleRate();
    spectrum.showRef = ref != nullptr;
    if (ref != nullptr) spectrum.refBands = ref->learn.bandsDb;
    spectrum.target = proc.referenceIsTarget() ? ref->learn.bandsDb
                                               : mp::genreProfile ((int) proc.apvts.getRawParameterValue ("genre")->load()).masterCurve;
    spectrum.repaint();

    shortTerm  = proc.outMeter.getShortTerm();
    integrated = proc.outMeter.getIntegrated();
    const float tp = proc.truePeak.takeDb();
    truePeakDb = tp;
    tpHold = tp > tpHold ? tp : tpHold - 0.05f;
    glueGr = proc.glueGrDb.load();
    limGr  = jmax (proc.getLimiterGr(), limGr * 0.85f);
    tracks = proc.getTracks();
    tipsBar.setTips (proc.getTips());
    if (review->isVisible() && ++reviewTick % 15 == 0) review->setReview (proc.buildReview());   // mise à jour 2x/s

    const auto ph = proc.getPhase();
    const bool busy = ph == MasterProcessor::Phase::WaitTracks || ph == MasterProcessor::Phase::Listening || ph == MasterProcessor::Phase::Verifying || ph == MasterProcessor::Phase::Settling;
    assistantBtn.setButtonText (busy ? "STOP" : "ASSISTANT");
    if (compareBtn.getToggleState() != proc.isComparing()) compareBtn.setToggleState (proc.isComparing(), dontSendNotification);
    if (listenRefBtn.getToggleState() != proc.isReferenceListening()) listenRefBtn.setToggleState (proc.isReferenceListening(), dontSendNotification);

    const bool hasRef = ref != nullptr;
    listenRefBtn.setEnabled (hasRef); matchRefBtn.setEnabled (hasRef); clearRefBtn.setEnabled (hasRef || proc.isLoadingReference());
    refLevelBtn.setEnabled (hasRef);

    repaint (statusArea); repaint (loudArea); repaint (tracksArea); repaint (dynArea); repaint (maxArea);
    repaint (refArea); repaint (corrArea);
    waveform.repaint();
}

//==============================================================================
void MasterView::paint (Graphics& g)
{
    g.fillAll (col::bg);

    // En-tête
    g.setColour (col::panel);
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (col::border);
    g.drawHorizontalLine (56, 0, (float) getWidth());
    g.setFont (font (20.0f, true));
    g.setColour (col::text);
    g.drawText ("MIXPILOT", 18, 12, 110, 32, Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("MASTER", 122, 12, 90, 32, Justification::centredLeft);

    g.setFont (font (10.0f, true)); g.setColour (col::textDim);
    g.drawText ("STYLE",   genreBox.getX(),     4, 100, 12, Justification::left);
    g.drawText ("OUTPUT",  outputBox.getX(),    4, 100, 12, Justification::left);
    g.drawText ("AMOUNT", amountSlider.getX(), 4, 100, 12, Justification::left);

    // Barre d'état
    {
        auto r = statusArea;
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 4);
        const auto ph = proc.getPhase();
        const float prog = proc.getPhaseProgress();
        if (prog > 0 && prog < 1)
        {
            g.setColour (col::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (r.toFloat().withWidth ((float) r.getWidth() * prog), 4);
        }
        const auto& op = mp::outputProfile ((int) proc.apvts.getRawParameterValue ("output")->load());
        g.setColour (ph == MasterProcessor::Phase::NoSignal ? col::warn : (ph == MasterProcessor::Phase::Done ? col::good : col::text));
        g.setFont (font (12.5f, true));
        g.drawText (proc.getStatusText(), r.reduced (12, 0), Justification::centredLeft);
        g.setColour (col::textDim); g.setFont (font (12.0f));
        const float defT = mp::outputLufs ((int) proc.apvts.getRawParameterValue ("output")->load(), (int) proc.apvts.getRawParameterValue ("genre")->load());
        g.drawText (u ("Target: ") + String (proc.hasLearned() ? proc.getTargetLufs() : defT, 1) + " LUFS  |  " + String (op.ceilingDb, 1) + " dBTP"
                    + (op.monoBassHz > 0 ? u ("  |  mono bass < ") + String ((int) op.monoBassHz) + " Hz" : String()),
                    r.reduced (12, 0), Justification::centredRight);
    }

    for (auto r : { eqArea, dynArea, imgArea, maxArea }) drawPanel (g, r);

    g.setFont (font (9.5f, true));
    for (int i = 0; i <= mp::kBands; ++i)
    {
        g.setColour (i == mp::kBands ? col::warn : col::textDim);
        auto sb = eqSliders[i].getBounds();
        g.drawText (eqLabels[i], sb.getX() - 6, sb.getBottom() + 2, sb.getWidth() + 12, 12, Justification::centred);
    }

    drawKnobTitle (g, glue, "GLUE");
    drawKnobTitle (g, warmth, "WARMTH");
    drawKnobTitle (g, width, "WIDTH");
    drawKnobTitle (g, monoBass, "MONO BASS");
    drawKnobTitle (g, push, "PUSH");
    drawKnobTitle (g, clipKnob, "CLIP");
    drawKnobTitle (g, ceiling, "CEILING");
    drawKnobTitle (g, release, "RELEASE");

    auto grBar = [&] (Rectangle<int> area, float gr, const String& label)
    {
        auto r = area.toFloat();
        g.setColour (col::textDim); g.setFont (font (9.5f, true));
        g.drawText (label, r.removeFromLeft (26), Justification::centredLeft);
        g.setColour (col::panelHi); g.fillRoundedRectangle (r, 2);
        g.setColour (col::warn);
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * jlimit (0.0f, 1.0f, gr / 12.0f)), 2);
        g.setColour (col::text); g.setFont (font (9.5f));
        g.drawText (String (gr, 1) + " dB", r.reduced (4, 0), Justification::centredRight);
    };
    grBar ({ dynArea.getX() + 10, dynArea.getBottom() - 22, dynArea.getWidth() - 20, 12 }, glueGr, "GR");
    grBar ({ maxArea.getX() + 10, maxArea.getBottom() - 22, maxArea.getWidth() - 20, 12 }, limGr, "GR");

    drawLoudnessPanel (g, loudArea);
    drawTrackPanel (g, tracksArea);
    drawReferencePanel (g, refArea);
    drawCorrectionPanel (g, corrArea);
}

void MasterView::drawKnobTitle (Graphics& g, const Slider& s, const String& t)
{
    g.setColour (col::textDim); g.setFont (font (10.0f, true));
    g.drawText (t, s.getX() - 10, s.getY() - 14, s.getWidth() + 20, 12, Justification::centred);
}

void MasterView::drawReferencePanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (12, 8);
    auto info = r.removeFromLeft (190);
    drawSectionTitle (g, info.removeFromTop (16), "REFERENCE");

    auto ref = proc.getReference();
    g.setFont (font (12.5f, true));
    if (ref != nullptr)
    {
        g.setColour (col::purple);
        g.drawText (ref->name, info.removeFromTop (20), Justification::centredLeft, true);
        g.setFont (font (11.0f)); g.setColour (col::textDim);
        const int mins = (int) (ref->durationSec / 60), secs = (int) ref->durationSec % 60;
        g.drawText (String (ref->lufs, 1) + " LUFS · " + String (ref->truePeakDb, 1) + " dBTP", info.removeFromTop (15), Justification::centredLeft);
        g.drawText ("LRA " + String (ref->lra, 1) + " LU · " + String (mins) + ":" + String (secs).paddedLeft ('0', 2), info.removeFromTop (15), Justification::centredLeft);
    }
    else
    {
        const auto err = proc.getReferenceError();
        g.setColour (err.isNotEmpty() ? col::warn : col::textDim);
        g.setFont (font (11.0f));
        g.drawFittedText (err.isNotEmpty() ? err : u ("None. Load a song that sounds the way you want."), info.removeFromTop (34), Justification::topLeft, 2);
    }
}

void MasterView::drawCorrectionPanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (12, 8);
    auto row1 = r.removeFromTop (24);
    g.setColour (col::textDim); g.setFont (font (10.5f, true));
    g.drawText ("TARGET", row1.removeFromLeft (50), Justification::centredLeft);

    r.removeFromTop (6);
    auto lab = r.removeFromTop (14);
    const bool masterOnly = tracks.empty() || proc.getTrackShare() <= 0.0f;
    g.setFont (font (10.0f, true));
    g.setColour (col::purple);  g.drawText ("TRACKS", lab, Justification::centredLeft);
    g.setColour (col::accent);  g.drawText ("MASTER", lab, Justification::centredRight);
    g.setColour (col::textDim);
    g.setFont (font (masterOnly ? 9.0f : 10.0f, ! masterOnly));
    g.drawText (masterOnly ? (tracks.empty() ? u ("no tracks: Master only") : u ("Master only")) : u ("SPLIT"), lab, Justification::centred);
}

void MasterView::drawLoudnessPanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (14, 10);
    drawSectionTitle (g, r.removeFromTop (18), "LOUDNESS");

    const auto& op = mp::outputProfile ((int) proc.apvts.getRawParameterValue ("output")->load());
    const float target = proc.hasLearned() ? proc.getTargetLufs()
                                           : mp::outputLufs ((int) proc.apvts.getRawParameterValue ("output")->load(), (int) proc.apvts.getRawParameterValue ("genre")->load());
    const bool hasInt = integrated > -69.0f;
    const float diff = integrated - target;
    const bool okL = hasInt && std::abs (diff) <= 1.0f;
    const bool okTP = tpHold <= op.ceilingDb + 0.05f;

    auto big = r.removeFromTop (56);
    g.setFont (font (38.0f, true));
    g.setColour (hasInt ? (okL ? col::good : col::text) : col::textDim);
    g.drawText (hasInt ? String (integrated, 1) : String ("--.-"), big.removeFromLeft (124), Justification::centredLeft);
    g.setFont (font (11.0f, true)); g.setColour (col::textDim);
    g.drawText (u ("INTEGRATED LUFS"), big.removeFromTop (28), Justification::bottomLeft);
    g.drawText (hasInt ? (diff >= 0 ? "+" : "") + String (diff, 1) + " vs target" : u ("waiting…"), big, Justification::topLeft);

    r.removeFromTop (2);
    auto row = [&] (const String& k, const String& v, Colour c)
    {
        auto line = r.removeFromTop (20);
        g.setFont (font (12.0f)); g.setColour (col::textDim);
        g.drawText (k, line, Justification::centredLeft);
        g.setFont (font (12.5f, true)); g.setColour (c);
        g.drawText (v, line, Justification::centredRight);
    };
    row ("Short term (3 s)", shortTerm > -69 ? String (shortTerm, 1) + " LUFS" : String ("--"), col::text);
    row ("True peak (max)",   tpHold > -99 ? String (tpHold, 1) + " dBTP" : String ("--"), okTP ? col::text : col::bad);
    if (auto ref = proc.getReference())
        row ("Reference", String (ref->lufs, 1) + " LUFS", col::purple);
    else
        row ("Glue reduction", String (glueGr, 1) + " dB", col::text);

    r.removeFromTop (8);
    auto bar = r.removeFromTop (14).toFloat();
    g.setColour (col::panelHi); g.fillRoundedRectangle (bar, 3);
    auto xOf = [&] (float l) { return bar.getX() + bar.getWidth() * jlimit (0.0f, 1.0f, (l + 30.0f) / 30.0f); };
    if (shortTerm > -69) { g.setColour (col::accent); g.fillRoundedRectangle (bar.withRight (xOf (shortTerm)), 3); }
    g.setColour (col::warn);
    g.fillRect (xOf (target) - 1.0f, bar.getY() - 3, 2.0f, bar.getHeight() + 6);
    g.setFont (font (9.5f)); g.setColour (col::textDim);
    auto lab = r.removeFromTop (13);
    g.drawText ("-30", lab, Justification::centredLeft);
    g.drawText ("0 LUFS", lab, Justification::centredRight);

    r.removeFromTop (4);
    auto chk = [&] (bool ok, const String& t)
    {
        auto line = r.removeFromTop (19);
        g.setColour (ok ? col::good : col::textDim);
        g.fillEllipse (line.getX() + 1.0f, line.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setFont (font (12.0f)); g.setColour (ok ? col::text : col::textDim);
        g.drawText (t, line.withTrimmedLeft (16), Justification::centredLeft);
    };
    chk (okL,  "Loudness on target (" + String (target, 1) + " LUFS)");
    chk (okTP && tpHold > -99, u ("No clipping (≤ ") + String (op.ceilingDb, 1) + " dBTP)");
}

void MasterView::drawTrackPanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (14, 10);
    auto head = r.removeFromTop (18);
    drawSectionTitle (g, head, u ("LINKED TRACKS (") + String ((int) tracks.size()) + ")");
    r.removeFromTop (6);

    if (tracks.empty())
    {
        g.setColour (col::textDim); g.setFont (font (12.0f));
        g.drawFittedText (u ("No tracks.\nAdd MixPilot Track on every track (after the instrument or effects): they will show up here."),
                          r.removeFromTop (80), Justification::topLeft, 5);
        return;
    }

    static const char* phaseTxt[] = { "-", "listening", "setting", "OK" };
    const int maxRows = r.getHeight() / 22;
    for (int i = 0; i < (int) tracks.size() && i < maxRows; ++i)
    {
        const auto& t = tracks[(size_t) i];
        auto line = r.removeFromTop (22);
        if (i % 2 == 0) { g.setColour (col::panelHi.withAlpha (0.6f)); g.fillRect (line); }
        line.reduce (6, 0);
        const int ph = jlimit (0, 3, t.phase);
        g.setColour (ph == 3 ? col::good : (ph == 0 ? col::textDim : col::warn));
        g.fillEllipse ((float) line.getX(), line.getCentreY() - 3.0f, 6.0f, 6.0f);
        line.removeFromLeft (12);
        g.setFont (font (11.5f, true)); g.setColour (col::text);
        g.drawText (t.name.isEmpty() ? "Track" : t.name, line.removeFromLeft (78), Justification::centredLeft, true);
        g.setFont (font (11.0f)); g.setColour (col::textDim);
        g.drawText (t.hasProfile ? instrumentName (t.instrument) : String ("?"), line.removeFromLeft (80), Justification::centredLeft, true);
        g.setColour (ph == 3 ? col::text : col::textDim);
        g.drawText (ph == 3 ? (t.trimDb >= 0 ? "+" : "") + String (t.trimDb, 1) + " dB" : u (phaseTxt[ph]), line, Justification::centredRight);
    }
}

//==============================================================================
void MasterView::resized()
{
    genreBox.setBounds  (226, 18, 180, 28);
    outputBox.setBounds (414, 18, 200, 28);
    amountSlider.setBounds (622, 18, 124, 28);
    assistantBtn.setBounds (754, 14, 104, 32);
    compareBtn.setBounds   (864, 14, 46, 32);
    bypassBtn.setBounds    (916, 14, 64, 32);
    helpBtn.setBounds      (986, 14, 38, 32);

    statusArea = { 16, 66, getWidth() - 32, 28 };
    tipsArea   = { 16, 100, getWidth() - 32, 30 };
    tipsBar.setBounds (tipsArea);
    statusZone.setBounds (statusArea);

    const int top = 138, specH = 330;
    spectrum.setBounds (16, top, 744, specH);
    loudArea = { 776, top, 248, specH };
    loudZone.setBounds (loudArea);
    resetLufsBtn.setBounds (loudArea.getRight() - 64, loudArea.getY() + 8, 52, 20);

    // Ligne référence / correction
    const int ry = top + specH + 8, rh = 74;
    refArea  = { 16, ry, 744, rh };
    corrArea = { 776, ry, 248, rh };
    refInfoZone.setBounds (refArea.withWidth (200));
    corrZone.setBounds (corrArea);
    {
        auto r = refArea.reduced (12, 8);
        r.removeFromLeft (196);
        auto btns = r.removeFromRight (196);
        auto b1 = btns.removeFromTop (27); btns.removeFromTop (4);
        loadRefBtn.setBounds (b1.removeFromLeft (150)); b1.removeFromLeft (6); clearRefBtn.setBounds (b1);
        listenRefBtn.setBounds (btns.removeFromLeft (110)); btns.removeFromLeft (6); matchRefBtn.setBounds (btns);
        r.removeFromRight (10);
        waveform.setBounds (r);
    }
    {
        auto r = corrArea.reduced (12, 8);
        auto row1 = r.removeFromTop (24);
        row1.removeFromLeft (50);
        refLevelBtn.setBounds (row1.removeFromRight (72));
        row1.removeFromRight (6);
        targetBox.setBounds (row1);
        r.removeFromTop (20);
        shareSlider.setBounds (r.removeFromTop (18));
    }

    const int my = ry + rh + 8, mh = getHeight() - my - 14;
    tracksArea = { 776, my, 248, mh };
    tracksZone.setBounds (tracksArea);
    eqArea  = { 16,  my, 300, mh };
    dynArea = { 320, my, 140, mh };
    imgArea = { 464, my, 140, mh };
    maxArea = { 608, my, 152, mh };

    auto placePower = [] (TextButton& b, Rectangle<int> a) { b.setBounds (a.getX() + 10, a.getY() + 10, a.getWidth() - 20, 22); };
    placePower (eqOn, eqArea); placePower (dynOn, dynArea); placePower (imageOn, imgArea); placePower (maxOn, maxArea);

    const int sx = eqArea.getX() + 14, sw = (eqArea.getWidth() - 28) / (mp::kBands + 1);
    for (int i = 0; i <= mp::kBands; ++i)
        eqSliders[i].setBounds (sx + i * sw, eqArea.getY() + 46, sw, mh - 46 - 26);

    auto knobAt = [] (Slider& s, Rectangle<int> a, int row, int rows)
    {
        const int avail = a.getHeight() - 40 - 26;
        const int h = avail / rows;
        s.setBounds (a.getX() + (a.getWidth() - 80) / 2, a.getY() + 52 + row * h, 80, jmin (86, h - 14));
    };
    knobAt (glue, dynArea, 0, 2);   knobAt (warmth, dynArea, 1, 2);
    knobAt (width, imgArea, 0, 2);  knobAt (monoBass, imgArea, 1, 2);

    push.setBounds     (maxArea.getX() + 6,  maxArea.getY() + 54, 70, 80);
    clipKnob.setBounds (maxArea.getX() + 76, maxArea.getY() + 54, 70, 80);
    ceiling.setBounds  (maxArea.getX() + 6,  maxArea.getY() + 166, 70, 80);
    release.setBounds  (maxArea.getX() + 76, maxArea.getY() + 166, 70, 80);

    help->setBounds (getLocalBounds());
    review->setBounds (getLocalBounds());
    reviewBtn.setBounds (tracksArea.getRight() - 122, tracksArea.getY() + 8, 110, 22);
}
