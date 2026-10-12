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
            { u ("Démarrage rapide"),
              u ("1. Place MixPilot Track en DERNIER sur la piste (sur une piste MIDI : après l'instrument et ses effets).\n"
                 "2. Mets MixPilot Master sur la piste Master pour relier toutes les pistes.\n"
                 "3. Lance l'ASSISTANT depuis le Master (ou clique ÉCOUTER ici pendant que la piste joue).\n"
                 "4. La piste reconnaît son instrument et se corrige. Ajuste l'INTENSITÉ à ton goût.") },
            { u ("Liaison avec le Master"),
              u ("La pastille en haut indique si un MixPilot Master est présent. S'il l'est, la piste suit son STYLE et sa SORTIE, "
                 "reçoit sa part de correction du mix (voir Suivre Master), et apparaît dans sa liste « Pistes reliées ».\n"
                 "Sans Master, la piste fonctionne seule (style Pop par défaut, ou celui que tu choisis).") },
            { u ("Instrument"),
              u ("AUTO : détecté pendant l'écoute (graves/aigus, attaques, son tonal ou bruité, variations de niveau). La confiance est affichée à droite.\n"
                 "Si la détection se trompe, choisis l'instrument à la main : toutes les corrections s'adaptent immédiatement.") },
            { u ("Mashups : Instrumental complet + Acapella"),
              u ("INSTRUMENTAL COMPLET : pour un beat ou la version instrumentale d'un morceau. La piste est traitée comme un mix entier (équilibre du style choisi, compression douce de « colle »).\n"
                 "ACAPELLA : pour une voix seule. Elle devient prioritaire sur tout le reste.\n"
                 "Quand les deux sont reliées, l'instru creuse automatiquement ses médiums (800 Hz – 5 kHz) dès que la voix chante : la voix passe devant sans qu'on monte son volume. Le réglage se fait dans la carte ESPACE.\n"
                 "MixPilot gère le son ; le tempo et la tonalité se calent dans Ableton (Warp, Transpose).") },
            { u ("Style et Intensité"),
              u ("STYLE : « Suivre le Master » (recommandé) ou un style imposé pour cette piste.\n"
                 "INTENSITÉ : dose de correction (0 % = aucune, 100 % = complète).") },
            { u ("Écouter"),
              u ("Analyse la piste pendant 8 s. Fais-le pendant un passage représentatif où la piste joue franchement. "
                 "L'ASSISTANT du Master déclenche l'écoute de toutes les pistes en même temps.") },
            { u ("Spectre"),
              u ("Gris : le son avant MixPilot. Bleu : après. Pointillés orange : la forme idéale de l'instrument. Vert : la courbe d'EQ appliquée (coupe-bas inclus).") },
            { u ("Gain"),
              u ("Place la piste au bon niveau dans le mix, selon l'instrument, le style et la sortie (ex. voix plus en avant en Pop, kick plus fort en Électro et en Club). "
                 "Le gain est vérifié après EQ et compression pour tomber juste.") },
            { u ("Nettoyage"),
              u ("Coupe-bas adapté à l'instrument : il retire les graves inutiles (bruits, résonances) qui brouillent le mix, surtout quand beaucoup de pistes s'additionnent.") },
            { u ("EQ"),
              u ("VERT : rapproche le spectre de la forme idéale de l'instrument (jamais de boost là où l'instrument n'a pas de contenu).\n"
                 "ORANGE : anti-masquage, la piste se creuse légèrement là où une piste prioritaire joue (ex. un synthé autour de la voix).\n"
                 "VIOLET : correction demandée par le Master pour équilibrer le mix entier, partagée entre les pistes selon ce qu'elles apportent.") },
            { u ("Compression"),
              u ("Ratio, attaque et relâchement adaptés à l'instrument (rapide sur une caisse claire, lent sur une nappe), et plus dense selon le style. GR = réduction en cours.") },
            { u ("Espace"),
              u ("Sur une BASSE : ses graves baissent automatiquement à chaque coup de kick (ducking), sans câbler de sidechain. Il faut un MixPilot Track sur le kick.\n"
                 "Sur un KICK : il pilote ce ducking.\n"
                 "Sur les autres pistes : affiche l'anti-masquage appliqué.") },
            { u ("Suivre Master"),
              u ("Activé : la piste applique sa part de correction du mix demandée par le Master (le curseur RÉPARTITION du Master règle cette part).\n"
                 "Désactivé : la piste ne se corrige que pour elle-même.") },
            { u ("Bypass"),
              u ("Coupe tout le traitement de cette piste (pratique pour comparer).") },
            { u ("Conseils"),
              u ("La barre CONSEIL te dit quoi vérifier : détection incertaine, piste trop faible ou saturée, ducking à activer, correction demandée par le Master… Clique dessus pour passer au suivant.") },
        };
    }
}

TrackEditor::TrackEditor (TrackProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    auto& vts = proc.apvts;

    for (auto* z : { &linkZone, &statusZone, &detectZone }) addAndMakeVisible (z);
    for (auto& z : cardZones) addAndMakeVisible (z);
    linkZone.setTooltip (u ("Vert : un MixPilot Master est présent, cette piste suit son style, sa sortie et ses corrections. Gris : la piste fonctionne seule."));
    statusZone.setTooltip (u ("État de l'analyse de la piste et réglages qu'elle suit."));
    detectZone.setTooltip (u ("Instrument reconnu et son caractère : répartition graves/médiums/aigus, côté percussif (attaques rapides) ou tenu."));
    cardZones[0].setTooltip (u ("GAIN : place la piste au bon niveau dans le mix (niveau mesuré → niveau visé)."));
    cardZones[1].setTooltip (u ("NETTOYAGE : coupe-bas qui retire les graves inutiles de cette piste."));
    cardZones[2].setTooltip (u ("EQ : vert = vers la forme idéale, orange = place laissée aux autres pistes, violet = demandé par le Master pour équilibrer le mix."));
    cardZones[3].setTooltip (u ("COMPRESSION : stabilise la dynamique de la piste. GR = réduction de gain en cours."));
    cardZones[4].setTooltip (u ("ESPACE : ducking de la basse sous le kick, creusement de l'instru complète sous la voix, ou anti-masquage entre pistes."));

    instrumentBox.addItem ("Auto", 1);
    for (int i = 0; i < mp::NumInstruments; ++i) instrumentBox.addItem (instName (i), i + 2);
    genreBox.addItem ("Suivre le Master", 1);
    for (int i = 0; i < mp::NumGenres; ++i) genreBox.addItem (u (mp::genreProfile (i).name), i + 2);
    instrumentBox.setTooltip (u ("Instrument de la piste. AUTO = détecté pendant l'écoute. Choisis-le à la main si la détection se trompe."));
    genreBox.setTooltip (u ("Style visé. « Suivre le Master » (recommandé) reprend le style choisi sur MixPilot Master."));
    addAndMakeVisible (instrumentBox); addAndMakeVisible (genreBox);
    instAtt  = std::make_unique<CA> (vts, "instrument", instrumentBox);
    genreAtt = std::make_unique<CA> (vts, "genre", genreBox);

    amountSlider.setSliderStyle (Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (Slider::TextBoxRight, false, 44, 18);
    amountSlider.setColour (Slider::trackColourId, col::accent2);
    amountSlider.setColour (Slider::backgroundColourId, col::border);
    amountSlider.setColour (Slider::thumbColourId, col::text);
    amountSlider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    amountSlider.setTooltip (u ("Dose de correction : 0 % = aucune, 100 % = complète."));
    addAndMakeVisible (amountSlider);
    amountAtt = std::make_unique<SA> (vts, "amount", amountSlider);

    listenBtn.setButtonText (u ("ÉCOUTER"));
    listenBtn.setColour (TextButton::buttonColourId, col::accent2.darker (0.35f));
    listenBtn.setTooltip (u ("Analyse cette piste pendant 8 s (elle doit jouer). L'ASSISTANT du Master le fait pour toutes les pistes d'un coup."));
    listenBtn.onClick = [this] { proc.startListening (8); };
    addAndMakeVisible (listenBtn);

    styleHelpButton (helpBtn);
    helpBtn.onClick = [this] { help->setVisible (true); help->toFront (true); };
    addAndMakeVisible (helpBtn);

    stylePower (bypassBtn);
    bypassBtn.setColour (TextButton::buttonOnColourId, col::bad);
    bypassBtn.setTooltip (u ("Coupe tout le traitement de cette piste."));
    addAndMakeVisible (bypassBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "bypass", bypassBtn));

    followBtn.setButtonText ("SUIVRE MASTER");
    stylePower (followBtn);
    followBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.35f));
    followBtn.setTooltip (u ("Applique la part de correction du mix demandée par le Master (barres violettes). Désactivé : la piste ne se corrige que pour elle-même."));
    addAndMakeVisible (followBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "followMaster", followBtn));

    struct P { TextButton* b; const char* id; const char* tip; };
    P powers[] = {
        { &gainOn, "gainOn", "Active/désactive le gain de placement." },
        { &eqOn,   "eqOn",   "Active/désactive l'EQ et le coupe-bas (y compris la part demandée par le Master)." },
        { &compOn, "compOn", "Active/désactive la compression." },
        { &duckOn, "duckOn", "Active/désactive le ducking (basse sous le kick, instru complète sous la voix)." } };
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
    spectrum.caption = u ("AVANT (gris) · APRÈS (bleu) · FORME IDÉALE (pointillés) · CORRECTION (vert)");
    spectrum.setTooltip (u ("Gris = avant MixPilot, bleu = après, pointillés = forme idéale de l'instrument, vert = courbe d'EQ appliquée."));
    addAndMakeVisible (spectrum);

    help = std::make_unique<HelpOverlay> (u ("Guide de MixPilot Track"), trackHelp());
    addChildComponent (*help);

    setSize (1000, 610);
    startTimerHz (30);
}

TrackEditor::~TrackEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TrackEditor::timerCallback()
{
    spectrum.specIn  = proc.inAnalyzer.getDisplayDb();
    spectrum.specOut = proc.outAnalyzer.getDisplayDb();
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
    listenBtn.setButtonText (listening ? u ("ÉCOUTE…") : u ("ÉCOUTER"));
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
        g.drawText (linked ? u ("RELIÉ AU MASTER") : u ("MODE AUTONOME"), r.withTrimmedLeft (26).withHeight (r.getHeight() / 2 + 2).translated (0, 3), Justification::centredLeft);
        g.setFont (font (10.5f)); g.setColour (col::textDim);
        const String sub = linked ? u (mp::genreProfile (proc.getEffectiveGenre()).name).upToFirstOccurrenceOf (" ", false, false)
                                    + " / " + u (mp::outputProfile (proc.getEffectiveOutput()).name).upToFirstOccurrenceOf (" ", false, false)
                                  : u ("Master non détecté");
        g.drawText (sub, r.withTrimmedLeft (26).withTrimmedTop (r.getHeight() / 2 - 1), Justification::centredLeft);
    }

    g.setFont (font (10.0f, true)); g.setColour (col::textDim);
    g.drawText ("INSTRUMENT", instrumentBox.getX(), 4, 120, 12, Justification::left);
    g.drawText ("STYLE",      genreBox.getX(),      4, 120, 12, Justification::left);
    g.drawText (u ("INTENSITÉ"), amountSlider.getX(), 4, 120, 12, Justification::left);

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
    if (manual)                 sub = u ("Choisi manuellement") + (proc.hasProfile() ? u (" (détecté : ") + instName (det.instrument) + ")" : String());
    else if (proc.hasProfile()) sub = u ("Détecté automatiquement · confiance ") + String (roundToInt (det.confidence * 100)) + " %";
    else                        sub = u ("Pas encore analysé");
    g.drawText (sub, r.removeFromTop (18), Justification::centredLeft);
    r.removeFromTop (10);

    if (! proc.hasProfile()) return;
    const auto& L = proc.getLearned();
    const float perc = mp::percussiveness (L);
    std::pair<String, float> feats[] = {
        { "Graves",    L.low },  { u ("Bas-médiums"), L.lowmid }, { u ("Médiums"), L.mid }, { "Aigus", L.high },
        { "Percussif", perc },   { "Tenu", L.sustain } };
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
            small (has ? String (proc.getLearnedLufs(), 1) + u (" → ") + String (c.targetLufs, 1) + u (" LUFS (cible dans le mix)") : u ("Place la piste au bon niveau"));
            g.setFont (font (11.0f)); g.setColour (col::text);
            g.drawText (u ("Sortie : ") + (outShort > -69 ? String (outShort, 1) + " LUFS" : String ("--")), r.removeFromTop (18), Justification::centredLeft);
            break;

        case 1:
            g.setFont (font (11.0f, true)); g.setColour (col::text);
            g.drawText ("NETTOYAGE", area.reduced (12, 10).removeFromTop (22), Justification::centredLeft);
            big (String (roundToInt (c.hpfHz)) + " Hz");
            small (u ("Coupe-bas : retire les graves inutiles qui encombrent le mix"));
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
            legend (col::accent2, u ("vers la forme idéale"));
            legend (col::warn,    u ("place laissée aux autres"));
            legend (col::purple,  u ("demandé par le Master"));
            break;
        }

        case 3:
            big (has ? String (c.compRatio, 1) + ":1" : String());
            small (has ? u ("Attaque ") + String (roundToInt (c.compAttackMs)) + u (" ms · relâchement ") + String (roundToInt (c.compReleaseMs)) + " ms" : u ("Stabilise la dynamique"));
            grBar (compGr, "GR");
            break;

        case 4:
        {
            const int inst = proc.getResolvedInstrument();
            if (inst == mp::Instrumental && c.voiceDuckDb > 0.05f)
            {
                big ("-" + String (c.voiceDuckDb, 1) + " dB", col::warn);
                small (u ("Les médiums de l'instru (800 Hz – 5 kHz) se creusent quand la voix chante"));
                grBar (vDuckGr, "VOIX");
            }
            else if (inst == mp::Instrumental)
            {
                big (u ("En attente"), col::textDim);
                small (u ("Mets MixPilot Track (Acapella ou Voix) sur la voix : l'instru lui fera de la place"));
            }
            else if (mp::isVoice (inst))
            {
                big (u ("Prioritaire"), col::good);
                small (u ("Les autres pistes (et l'instru complète) se creusent pour laisser passer la voix"));
            }
            else if (inst == mp::Bass && c.duckDepthDb > 0.05f)
            {
                big ("-" + String (c.duckDepthDb, 1) + " dB", col::warn);
                small (u ("La basse s'efface sous le kick (graves uniquement)"));
                grBar (duckGr, "DUCK");
            }
            else if (inst == mp::Kick)
            {
                big (u ("Prioritaire"), col::good);
                small (u ("Ce kick pilote le ducking des basses reliées"));
            }
            else
            {
                int worst = -1; float w = 0;
                for (int b = 0; b < mp::kBands; ++b) if (c.maskDb[(size_t) b] < w) { w = c.maskDb[(size_t) b]; worst = b; }
                if (worst >= 0)
                {
                    const float f = mp::kBandFreqs[(size_t) worst];
                    big (String (w, 1) + " dB", col::warn);
                    small (u ("Anti-masquage autour de ") + (f >= 1000 ? String (f / 1000, 0) + " kHz" : String ((int) f) + " Hz") + u (" : laisse passer les pistes prioritaires"));
                }
                else
                {
                    big (u ("Libre"), col::good);
                    small (u ("Aucun conflit avec les autres pistes reliées"));
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

    const int top = 138, h = 236;
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
