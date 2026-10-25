#include "TrackEditor.h"

using namespace juce;
using namespace mp::ui;

namespace
{
    String u (const char* s) { return String::fromUTF8 (s); }
    String instName (int i)  { return u (mp::instrumentProfile (i).name); }
    String signedDb (float v, int dec = 1) { return (v >= 0 ? "+" : "") + String (v, dec) + " dB"; }

    std::vector<HelpOverlay::Section> trackHelp()
    {
        return {
            { u ("Quick start"),
              u ("1. Put MixPilot Track LAST on the track (on a MIDI track: after the instrument and its effects).\n"
                 "2. Put MixPilot Master on the Master track to link all the tracks.\n"
                 "3. Run the ASSISTANT from the Master (or click LISTEN here while the track plays).\n"
                 "4. The track recognises its instrument and corrects itself. Adjust AMOUNT to taste.") },
            { u ("Link with the Master"),
              u ("The pill at the top shows whether a MixPilot Master is present. If it is, the track follows its STYLE and OUTPUT, "
                 "receives its share of the mix correction (see Follow Master), and shows up in its \"Linked tracks\" list.\n"
                 "Without a Master, the track works on its own (Pop style by default, or the one you choose).") },
            { u ("Instrument"),
              u ("AUTO: detected while listening (lows/highs, attacks, tonal or noisy sound, level changes). The confidence is shown on the right.\n"
                 "If detection gets it wrong, pick the instrument by hand: every correction adapts immediately.") },
            { u ("Mashups: Full instrumental + A cappella"),
              u ("FULL INSTRUMENTAL: for a beat or the instrumental version of a song. The track is treated as a whole mix (balance of the chosen style, gentle \"glue\" compression).\n"
                 "A CAPPELLA: for a solo vocal. It takes priority over everything else.\n"
                 "When both are linked, the instrumental automatically dips its mids (800 Hz – 5 kHz) as soon as the vocal sings: the vocal comes forward without raising its level. It's set in the SPACE card.\n"
                 "MixPilot handles the sound; tempo and key are set in Ableton (Warp, Transpose).") },
            { u ("Style and Amount"),
              u ("STYLE: \"Follow Master\" (recommended) or a style forced for this track.\n"
                 "AMOUNT: how much correction (0 % = none, 100 % = full).") },
            { u ("Listen"),
              u ("Analyses the track for 8 s. Do it during a representative section where the track plays clearly. "
                 "The Master's ASSISTANT triggers listening on all tracks at once.") },
            { u ("Spectrum"),
              u ("Grey: the sound before MixPilot. Blue: after. Orange dots: the instrument's ideal shape. Green: the applied EQ curve (low cut included).\n"
                 "After listening, each EQ band gets a point on the curve: drag it up/down for gain, left/right for frequency. Hold Shift for fine moves. "
                 "Edited points turn orange; your edits stay on top of the analysis, even when it updates. "
                 "Double-click a point to return it to the analysis, right-click to reset all edits.") },
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
            { u ("Gain"),
              u ("Sets the track at the right level in the mix, according to instrument, style and output (e.g. vocal further forward in Pop, louder kick in Electro and Club). "
                 "Gain is checked after EQ and compression to land right.") },
            { u ("Cleanup"),
              u ("Low cut tuned to the instrument: it removes useless lows (noise, resonances) that muddy the mix, especially when many tracks add up.") },
            { u ("EQ"),
              u ("GREEN: brings the spectrum closer to the instrument's ideal shape (never boosts where the instrument has no content).\n"
                 "ORANGE: anti-masking, the track dips slightly where a priority track plays (e.g. a synth around the vocal).\n"
                 "PURPLE: correction requested by the Master to balance the whole mix, shared between tracks according to what they contribute.") },
            { u ("Compression"),
              u ("Ratio, attack and release suited to the instrument (fast on a snare, slow on a pad), and denser depending on the style. GR = current gain reduction.") },
            { u ("Space"),
              u ("On a BASS: its lows automatically drop on every kick hit (ducking), no sidechain routing needed. Requires a MixPilot Track on the kick.\n"
                 "On a KICK: it drives that ducking.\n"
                 "On other tracks: shows the anti-masking applied.") },
            { u ("Follow Master"),
              u ("On: the track applies its share of the mix correction requested by the Master (the Master's SPLIT slider sets this share).\n"
                 "Off: the track only corrects itself.") },
            { u ("Bypass"),
              u ("Turns off all processing on this track (handy for comparing).") },
            { u ("Tips"),
              u ("The TIP bar tells you what to check: uncertain detection, track too quiet or clipping, ducking to turn on, correction requested by the Master… Click it to see the next one.") },
        };
    }
}

TrackEditor::TrackEditor (TrackProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    auto& vts = proc.apvts;

    for (auto* z : { &linkZone, &statusZone, &detectZone }) addAndMakeVisible (z);
    for (auto& z : cardZones) addAndMakeVisible (z);
    linkZone.setTooltip (u ("Green: a MixPilot Master is present, this track follows its style, output and corrections. Grey: the track works on its own."));
    statusZone.setTooltip (u ("Track analysis status and the settings it follows."));
    detectZone.setTooltip (u ("Recognised instrument and its character: lows/mids/highs balance, percussive (fast attacks) or sustained."));
    cardZones[0].setTooltip (u ("GAIN: sets the track at the right level in the mix (measured level → target level)."));
    cardZones[1].setTooltip (u ("CLEANUP: low cut that removes useless lows from this track."));
    cardZones[2].setTooltip (u ("EQ: green = towards the ideal shape, orange = room made for other tracks, purple = requested by the Master to balance the mix."));
    cardZones[3].setTooltip (u ("COMPRESSION: controls the track's dynamics. GR = current gain reduction."));
    cardZones[4].setTooltip (u ("SPACE: bass ducking under the kick, full instrumental dipping under the vocal, or anti-masking between tracks."));

    instrumentBox.addItem ("Auto", 1);
    for (int i = 0; i < mp::NumInstruments; ++i) instrumentBox.addItem (instName (i), i + 2);
    genreBox.addItem ("Follow Master", 1);
    for (int i = 0; i < mp::NumGenres; ++i) genreBox.addItem (u (mp::genreProfile (i).name), i + 2);
    instrumentBox.setTooltip (u ("Track instrument. AUTO = detected while listening. Pick it by hand if detection gets it wrong."));
    genreBox.setTooltip (u ("Target style. \"Follow Master\" (recommended) uses the style chosen on MixPilot Master."));
    addAndMakeVisible (instrumentBox); addAndMakeVisible (genreBox);
    instAtt  = std::make_unique<CA> (vts, "instrument", instrumentBox);
    genreAtt = std::make_unique<CA> (vts, "genre", genreBox);

    amountSlider.setSliderStyle (Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (Slider::TextBoxRight, false, 44, 18);
    amountSlider.setColour (Slider::trackColourId, col::accent2);
    amountSlider.setColour (Slider::backgroundColourId, col::border);
    amountSlider.setColour (Slider::thumbColourId, col::text);
    amountSlider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    amountSlider.setTooltip (u ("How much correction: 0 % = none, 100 % = full."));
    addAndMakeVisible (amountSlider);
    amountAtt = std::make_unique<SA> (vts, "amount", amountSlider);

    listenBtn.setButtonText ("LISTEN");
    listenBtn.setColour (TextButton::buttonColourId, col::accent2.darker (0.35f));
    listenBtn.setTooltip (u ("Analyses this track for 8 s (it must be playing). The Master's ASSISTANT does it for all tracks at once."));
    listenBtn.onClick = [this] { proc.startListening (8); };
    addAndMakeVisible (listenBtn);

    styleHelpButton (helpBtn);
    helpBtn.onClick = [this] { help->setVisible (true); help->toFront (true); };
    addAndMakeVisible (helpBtn);

    stylePower (bypassBtn);
    bypassBtn.setColour (TextButton::buttonOnColourId, col::bad);
    bypassBtn.setTooltip (u ("Turns off all processing on this track."));
    addAndMakeVisible (bypassBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "bypass", bypassBtn));

    followBtn.setButtonText ("FOLLOW MASTER");
    stylePower (followBtn);
    followBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.35f));
    followBtn.setTooltip (u ("Applies the share of mix correction requested by the Master (purple bars). Off: the track only corrects itself."));
    addAndMakeVisible (followBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "followMaster", followBtn));

    struct P { TextButton* b; const char* id; const char* tip; };
    P powers[] = {
        { &gainOn, "gainOn", "Turns the placement gain on/off." },
        { &eqOn,   "eqOn",   "Turns EQ and low cut on/off (including the share requested by the Master)." },
        { &compOn, "compOn", "Turns compression on/off." },
        { &duckOn, "duckOn", "Turns ducking on/off (bass under the kick, full instrumental under the vocal)." } };
    for (auto& pw : powers)
    {
        stylePower (*pw.b);
        pw.b->setColour (TextButton::buttonOnColourId, col::accent2.darker (0.25f));
        pw.b->setTooltip (u (pw.tip));
        addAndMakeVisible (*pw.b);
        buttonAtt.push_back (std::make_unique<BA> (vts, pw.id, *pw.b));
    }

    addAndMakeVisible (tipsBar);

    spectrum.fftSize = mp::SpectrumAnalyzer::fftSize;
    spectrum.eqCurve = [this] (float f) { return proc.eqResponseDb (f); };
    spectrum.caption = u ("BEFORE (grey) · AFTER (blue) · IDEAL SHAPE (dotted) · CORRECTION (green)");
    spectrum.setTooltip (u ("Grey = before MixPilot, blue = after, dotted = the instrument's ideal shape, green = applied EQ curve. "
                            "After listening, drag the EQ points to adjust them (Shift = fine, double-click = back to the analysis). "
                            "Double-click an empty spot to add your own EQ band, right-click for more."));

    // Points d'EQ éditables après l'analyse : la retouche s'ajoute à la correction calculée
    spectrum.eqPoints = [this]
    {
        std::vector<SpectrumView::EqPoint> pts;
        if (! proc.isEqEditable()) return pts;
        for (int b = 0; b < mp::kBands; ++b)
            pts.push_back ({ proc.getBandFreq (b), proc.getBandGainDb (b), proc.isBandEdited (b) });
        return pts;
    };
    auto setParam = [this] (const String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    spectrum.onPointMoved = [this, setParam] (int b, float freq, float gainDb)
    {
        setParam ("eqAdj" + String (b), gainDb - proc.getAnalysisBandGainDb (b));
        setParam ("eqShift" + String (b), jlimit (-mp::kMaxBandShiftOct, mp::kMaxBandShiftOct, std::log2 (freq / mp::octaveEQFreq (b))));
    };
    spectrum.onPointGesture = [this] (int b, bool starting)
    {
        for (auto id : { "eqAdj" + String (b), "eqShift" + String (b) })
            if (auto* p = proc.apvts.getParameter (id)) { if (starting) p->beginChangeGesture(); else p->endChangeGesture(); }
    };
    spectrum.onPointReset = [setParam] (int b)
    {
        setParam ("eqAdj" + String (b), 0.0f);
        setParam ("eqShift" + String (b), 0.0f);
    };
    spectrum.onResetAll = [setParam]
    {
        for (int b = 0; b < mp::kBands; ++b) { setParam ("eqAdj" + String (b), 0.0f); setParam ("eqShift" + String (b), 0.0f); }
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

    help = std::make_unique<HelpOverlay> (u ("MixPilot Track guide"), trackHelp());
    addChildComponent (*help);

    setSize (1000, 710);
    startTimerHz (30);
}

TrackEditor::~TrackEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TrackEditor::timerCallback()
{
    spectrum.setSpectra (proc.inAnalyzer.getDisplayDb(), proc.outAnalyzer.getDisplayDb());
    spectrum.sampleRate = proc.getCurrentSampleRate();
    spectrum.showTarget = proc.hasProfile();
    spectrum.target = mp::instrumentProfile (proc.getResolvedInstrument()).shape;
    spectrum.repaint();

    outShort = proc.outMeter.getShortTerm();
    compGr = jmax (proc.getCompGr(), compGr * 0.85f);
    duckGr = jmax (proc.getDuckGr(), duckGr * 0.85f);
    vDuckGr = jmax (proc.getVoiceDuckGr(), vDuckGr * 0.85f);
    tipsBar.setTips (proc.getTips());

    const bool listening = proc.getPhase() == TrackProcessor::Phase::Listening;
    listenBtn.setButtonText (listening ? u ("LISTENING…") : u ("LISTEN"));
    listenBtn.setEnabled (! listening);
    followBtn.setEnabled (proc.isLinked());
    repaint (0, 0, getWidth(), spectrum.getY());
    repaint (detectArea);
    for (auto& c : cards) repaint (c);
}

//==============================================================================
void TrackEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);

    g.setColour (col::panel); g.fillRect (0, 0, getWidth(), 56);
    g.setColour (col::border); g.drawHorizontalLine (56, 0, (float) getWidth());
    g.setFont (font (20.0f, true));
    g.setColour (col::text);   g.drawText ("MIXPILOT", 18, 12, 110, 32, Justification::centredLeft);
    g.setColour (col::accent2); g.drawText ("TRACK", 122, 12, 80, 32, Justification::centredLeft);

    {
        auto r = linkArea.toFloat();
        const bool linked = proc.isLinked();
        g.setColour (col::panelHi); g.fillRoundedRectangle (r, 6);
        g.setColour (linked ? col::good : col::textDim);
        g.fillEllipse (r.getX() + 10, r.getCentreY() - 4, 8, 8);
        g.setFont (font (11.0f, true)); g.setColour (col::text);
        g.drawText (linked ? u ("LINKED TO MASTER") : u ("STANDALONE MODE"), r.withTrimmedLeft (26).withHeight (r.getHeight() / 2 + 2).translated (0, 3), Justification::centredLeft);
        g.setFont (font (10.5f)); g.setColour (col::textDim);
        const String sub = linked ? u (mp::genreProfile (proc.getEffectiveGenre()).name).upToFirstOccurrenceOf (" ", false, false)
                                    + " / " + u (mp::outputProfile (proc.getEffectiveOutput()).name).upToFirstOccurrenceOf (" ", false, false)
                                  : u ("Master not detected");
        g.drawText (sub, r.withTrimmedLeft (26).withTrimmedTop (r.getHeight() / 2 - 1), Justification::centredLeft);
    }

    g.setFont (font (10.0f, true)); g.setColour (col::textDim);
    g.drawText ("INSTRUMENT", instrumentBox.getX(), 4, 120, 12, Justification::left);
    g.drawText ("STYLE",      genreBox.getX(),      4, 120, 12, Justification::left);
    g.drawText ("AMOUNT", amountSlider.getX(), 4, 120, 12, Justification::left);

    {
        auto r = statusArea;
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 4);
        const float prog = proc.getPhaseProgress();
        if (prog > 0 && prog < 1)
        {
            g.setColour (col::accent2.withAlpha (0.18f));
            g.fillRoundedRectangle (r.toFloat().withWidth ((float) r.getWidth() * prog), 4);
        }
        const auto ph = proc.getPhase();
        g.setColour (ph == TrackProcessor::Phase::Locked ? col::good : (ph == TrackProcessor::Phase::NoSignal ? col::warn : col::text));
        g.setFont (font (12.5f, true));
        g.drawText (proc.getStatusText(), r.reduced (12, 0).withTrimmedRight (220), Justification::centredLeft);
    }

    drawDetection (g, detectArea);
    for (int i = 0; i < 5; ++i) drawCard (g, i);
}

void TrackEditor::drawDetection (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (14, 10);
    drawSectionTitle (g, r.removeFromTop (18), "INSTRUMENT");

    const bool manual = (int) proc.apvts.getRawParameterValue ("instrument")->load() > 0;
    const auto det = proc.getDetection();
    const bool known = proc.hasProfile() || manual;

    const String nm = known ? instName (proc.getResolvedInstrument()) : String ("?");
    g.setFont (font (nm.length() > 14 ? 22.0f : 28.0f, true));
    g.setColour (known ? col::text : col::textDim);
    g.drawFittedText (nm, r.removeFromTop (40), Justification::centredLeft, 1, 0.8f);

    g.setFont (font (11.5f)); g.setColour (det.confidence < 0.6f && ! manual && proc.hasProfile() ? col::warn : col::textDim);
    String sub;
    if (manual)                 sub = u ("Picked by hand") + (proc.hasProfile() ? u (" (detected: ") + instName (det.instrument) + ")" : String());
    else if (proc.hasProfile()) sub = u ("Auto-detected · confidence ") + String (roundToInt (det.confidence * 100)) + " %";
    else                        sub = u ("Not analysed yet");
    g.drawText (sub, r.removeFromTop (18), Justification::centredLeft);
    r.removeFromTop (10);

    if (! proc.hasProfile()) return;
    const auto& L = proc.getLearned();
    const float perc = mp::percussiveness (L);
    std::pair<String, float> feats[] = {
        { "Lows",    L.low },  { "Low mids", L.lowmid }, { "Mids", L.mid }, { "Highs", L.high },
        { "Percussive", perc },   { "Sustained", L.sustain } };
    for (auto& [name, v] : feats)
    {
        auto line = r.removeFromTop (19);
        g.setFont (font (11.0f)); g.setColour (col::textDim);
        g.drawText (name, line.removeFromLeft (84), Justification::centredLeft);
        auto bar = line.reduced (0, 6).toFloat();
        g.setColour (col::panelHi); g.fillRoundedRectangle (bar, 2);
        g.setColour (col::accent2.withAlpha (0.85f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, v)), 2);
    }
}

void TrackEditor::drawCard (Graphics& g, int i)
{
    auto area = cards[i];
    drawPanel (g, area);
    auto r = area.reduced (12, 10);
    r.removeFromTop (30);

    const bool has = proc.hasProfile();
    const auto c = proc.getCorrection();
    auto big = [&] (const String& t, Colour col_ = col::text)
    {
        g.setFont (font (24.0f, true)); g.setColour (has ? col_ : col::textDim);
        g.drawText (has ? t : String ("--"), r.removeFromTop (34), Justification::centredLeft);
    };
    auto small = [&] (const String& t)
    {
        g.setFont (font (11.0f)); g.setColour (col::textDim);
        g.drawFittedText (t, r.removeFromTop (30), Justification::topLeft, 2);
    };
    auto grBar = [&] (float gr, const String& label)
    {
        auto line = r.removeFromBottom (14).toFloat();
        g.setFont (font (9.5f, true)); g.setColour (col::textDim);
        g.drawText (label, line.removeFromLeft (30), Justification::centredLeft);
        g.setColour (col::panelHi); g.fillRoundedRectangle (line, 2);
        g.setColour (col::warn); g.fillRoundedRectangle (line.withWidth (line.getWidth() * jlimit (0.0f, 1.0f, gr / 10.0f)), 2);
        g.setColour (col::text);
        g.drawText (String (gr, 1) + " dB", line.reduced (4, 0), Justification::centredRight);
    };

    switch (i)
    {
        case 0:
            big (signedDb (proc.getAppliedTrimDb()));
            small (has ? String (proc.getLearnedLufs(), 1) + u (" → ") + String (c.targetLufs, 1) + u (" LUFS (target in the mix)") : u ("Sets the track at the right level"));
            g.setFont (font (11.0f)); g.setColour (col::text);
            g.drawText (u ("Output: ") + (outShort > -69 ? String (outShort, 1) + " LUFS" : String ("--")), r.removeFromTop (18), Justification::centredLeft);
            break;

        case 1:
            g.setFont (font (11.0f, true)); g.setColour (col::text);
            g.drawText ("CLEANUP", area.reduced (12, 10).removeFromTop (22), Justification::centredLeft);
            big (String (roundToInt (c.hpfHz)) + " Hz");
            small (u ("Low cut: removes lows that clutter the mix"));
            break;

        case 2:
        {
            auto graph = r.removeFromTop (70).toFloat();
            const float mid = graph.getCentreY();
            const float bw = graph.getWidth() / mp::kBands;
            g.setColour (col::border); g.drawHorizontalLine ((int) mid, graph.getX(), graph.getRight());
            auto barFor = [&] (float v, float x0, float w)
            {
                const float h = jlimit (-9.0f, 9.0f, v) / 9.0f * (graph.getHeight() * 0.5f);
                return Rectangle<float> (x0, h > 0 ? mid - h : mid, w, std::abs (h));
            };
            for (int b = 0; b < mp::kBands; ++b)
            {
                const float own = has ? c.eqDb[(size_t) b] : 0.0f;
                const float mix = has ? c.mixDb[(size_t) b] : 0.0f;
                const float x0 = graph.getX() + b * bw + 2;
                const float w  = bw - 4;
                g.setColour (std::abs (c.maskDb[(size_t) b]) > 0.05f ? col::warn : col::accent2);
                g.fillRect (barFor (own, x0, std::abs (mix) > 0.05f ? w * 0.55f : w));
                if (std::abs (mix) > 0.05f)
                {
                    g.setColour (col::purple);
                    g.fillRect (barFor (mix, x0 + w * 0.55f, w * 0.45f));
                }
                if (has && proc.isBandEdited (b))
                {
                    // gain final après retouche manuelle (contour)
                    g.setColour (col::text.withAlpha (0.85f));
                    g.drawRect (barFor (proc.getBandGainDb (b), x0, w).expanded (0.0f, 0.5f), 1.0f);
                }
            }
            g.setFont (font (9.0f)); g.setColour (col::textDim);
            g.drawText ("31", graph.withHeight (10).translated (0, graph.getHeight() + 1), Justification::left);
            g.drawText ("16k", graph.withHeight (10).translated (0, graph.getHeight() + 1), Justification::right);
            r.removeFromTop (14);
            g.setFont (font (10.0f));
            auto legend = [&] (Colour cc, const String& t)
            {
                auto line = r.removeFromTop (13);
                g.setColour (cc); g.fillRect ((float) line.getX(), line.getCentreY() - 3.0f, 8.0f, 6.0f);
                g.setColour (col::textDim); g.drawText (t, line.withTrimmedLeft (12), Justification::centredLeft);
            };
            legend (col::accent2, u ("towards ideal shape"));
            legend (col::warn,    u ("room for other tracks"));
            legend (col::purple,  u ("requested by Master"));
            if (has)
            {
                bool edited = false;
                for (int b = 0; b < mp::kBands; ++b) edited = edited || proc.isBandEdited (b);
                if (edited)
                {
                    auto line = r.removeFromTop (13);
                    g.setColour (col::text.withAlpha (0.85f));
                    g.drawRect (Rectangle<float> ((float) line.getX(), line.getCentreY() - 3.0f, 8.0f, 6.0f), 1.0f);
                    g.setColour (col::textDim); g.drawText (u ("your edits (drag the points)"), line.withTrimmedLeft (12), Justification::centredLeft);
                }
            }
            break;
        }

        case 3:
            big (has ? String (c.compRatio, 1) + ":1" : String());
            small (has ? u ("Attack ") + String (roundToInt (c.compAttackMs)) + u (" ms · release ") + String (roundToInt (c.compReleaseMs)) + " ms" : u ("Controls the dynamics"));
            grBar (compGr, "GR");
            break;

        case 4:
        {
            const int inst = proc.getResolvedInstrument();
            if (inst == mp::Instrumental && c.voiceDuckDb > 0.05f)
            {
                big ("-" + String (c.voiceDuckDb, 1) + " dB", col::warn);
                small (u ("The instrumental's mids (800 Hz – 5 kHz) dip when the vocal sings"));
                grBar (vDuckGr, "VOX");
            }
            else if (inst == mp::Instrumental)
            {
                big (u ("Waiting"), col::textDim);
                small (u ("Put MixPilot Track (A cappella or Vocal) on the vocal: the instrumental will make room for it"));
            }
            else if (mp::isVoice (inst))
            {
                big (u ("Priority"), col::good);
                small (u ("Other tracks (and the full instrumental) dip to let the vocal through"));
            }
            else if (inst == mp::Bass && c.duckDepthDb > 0.05f)
            {
                big ("-" + String (c.duckDepthDb, 1) + " dB", col::warn);
                small (u ("The bass ducks under the kick (lows only)"));
                grBar (duckGr, "DUCK");
            }
            else if (inst == mp::Kick)
            {
                big (u ("Priority"), col::good);
                small (u ("This kick drives the ducking of linked basses"));
            }
            else
            {
                int worst = -1; float w = 0;
                for (int b = 0; b < mp::kBands; ++b) if (c.maskDb[(size_t) b] < w) { w = c.maskDb[(size_t) b]; worst = b; }
                if (worst >= 0)
                {
                    const float f = mp::kBandFreqs[(size_t) worst];
                    big (String (w, 1) + " dB", col::warn);
                    small (u ("Anti-masking around ") + (f >= 1000 ? String (f / 1000, 0) + " kHz" : String ((int) f) + " Hz") + u (": makes way for priority tracks"));
                }
                else
                {
                    big (u ("Clear"), col::good);
                    small (u ("No conflict with the other linked tracks"));
                }
            }
            break;
        }
        default: break;
    }
}

//==============================================================================
void TrackEditor::resized()
{
    linkArea = { 200, 11, 176, 34 };
    linkZone.setBounds (linkArea);
    instrumentBox.setBounds (388, 18, 166, 28);
    genreBox.setBounds      (562, 18, 166, 28);
    amountSlider.setBounds  (736, 18, 112, 28);
    listenBtn.setBounds     (856, 14, 92, 32);
    helpBtn.setBounds       (954, 14, 32, 32);

    statusArea = { 16, 66, getWidth() - 32, 28 };
    statusZone.setBounds (statusArea);
    bypassBtn.setBounds (statusArea.getRight() - 76, statusArea.getY() + 3, 70, 22);
    followBtn.setBounds (statusArea.getRight() - 206, statusArea.getY() + 3, 124, 22);

    tipsBar.setBounds (16, 100, getWidth() - 32, 30);

    const int top = 138, h = 336;
    spectrum.setBounds (16, top, 680, h);
    detectArea = { 706, top, getWidth() - 706 - 16, h };
    detectZone.setBounds (detectArea);

    const int cy = top + h + 10, ch = getHeight() - cy - 14;
    const int cw = (getWidth() - 32 - 4 * 8) / 5;
    for (int i = 0; i < 5; ++i) { cards[i] = { 16 + i * (cw + 8), cy, cw, ch }; cardZones[i].setBounds (cards[i]); }

    auto placePower = [] (TextButton& b, Rectangle<int> a) { b.setBounds (a.getX() + 10, a.getY() + 10, a.getWidth() - 20, 22); };
    placePower (gainOn, cards[0]); placePower (eqOn, cards[2]); placePower (compOn, cards[3]); placePower (duckOn, cards[4]);

    help->setBounds (getLocalBounds());
}
