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
            { u ("Démarrage rapide"),
              u ("1. Mets MixPilot Track à la fin de chaque piste, et MixPilot Master à la fin de la piste Master.\n"
                 "2. Choisis le STYLE et la SORTIE (où le morceau sera diffusé).\n"
                 "3. Lance la lecture sur le passage le plus fort, puis clique sur ASSISTANT (30 à 40 s).\n"
                 "4. Écoute, compare avec A/B, puis ajuste à ton goût : tout reste modifiable.") },
            { u ("Style et Sortie"),
              u ("STYLE donne l'équilibre visé (graves, médiums, aigus), la densité de compression et la largeur stéréo.\n"
                 "SORTIE fixe le volume final (LUFS) et le plafond de crête (dBTP). Streaming (-14) et YouTube (-13) sont fixes car ces plateformes baissent les morceaux trop forts. "
                 "SoundCloud, Club, CD et Vinyle suivent les usages du style (ex. électro : SoundCloud -8, Club -7, CD -8).\n"
                 "Changer l'un ou l'autre après l'Assistant recalcule tout instantanément, pistes comprises, sans nouvelle écoute.") },
            { u ("Intensité"),
              u ("Dose de correction appliquée par l'Assistant : 0 % = aucune, 100 % = correction complète. 70 à 85 % donne un résultat naturel.") },
            { u ("Assistant"),
              u ("Étape 1 : toutes les pistes reliées s'écoutent (8 s), reconnaissent leur instrument et se corrigent.\n"
                 "Étape 2 : le Master écoute le mix obtenu et le compare à la cible (style ou référence).\n"
                 "Étape 3 : il cale le volume final sur la sortie choisie, en plusieurs passes.\n"
                 "Clique de nouveau pour arrêter. Relance-le après un gros changement dans le morceau.") },
            { u ("A/B et Bypass"),
              u ("A/B fait entendre le son d'origine AU MÊME VOLUME que le son traité : tu juges la qualité, pas le volume (un son plus fort paraît toujours meilleur).\n"
                 "BYPASS coupe complètement le Master.") },
            { u ("Spectre"),
              u ("Gris : ce qui entre. Bleu : ce qui sort. Pointillés orange : la cible (style ou référence). Violet : la référence chargée. Vert : la correction d'EQ du Master.\n"
                 "L'affichage est incliné pour qu'un mix équilibré paraisse à peu près plat.") },
            { u ("Loudness"),
              u ("LUFS INTÉGRÉ : volume moyen perçu depuis le dernier RESET (c'est la valeur que mesurent Spotify, YouTube…).\n"
                 "Court terme : volume des 3 dernières secondes. True peak : la crête réelle, y compris entre les échantillons.\n"
                 "Les deux pastilles deviennent vertes quand le volume et les crêtes respectent la sortie choisie.") },
            { u ("Référence"),
              u ("Glisse un morceau (WAV, AIFF, FLAC, MP3, M4A) sur le panneau RÉFÉRENCE, ou clique sur CHARGER.\n"
                 "MixPilot mesure son équilibre spectral, son volume, ses crêtes, sa dynamique (LRA) et sa largeur stéréo.\n"
                 "ÉCOUTER RÉF. remplace ta sortie par la référence pour comparer. Avec VOL. ÉGAL, elle est jouée au même volume que ton mix (comparaison honnête).\n"
                 "Clique sur la forme d'onde pour te placer dans le morceau.") },
            { u ("Cible"),
              u ("STYLE : l'Assistant vise l'équilibre type du style choisi.\n"
                 "RÉFÉRENCE : il vise l'équilibre spectral et la largeur stéréo de ta référence.\n"
                 "VOL. RÉF. : vise aussi le volume de la référence au lieu de celui de la sortie. Les crêtes restent sous le plafond de la sortie.") },
            { u ("Répartition Pistes ↔ Master"),
              u ("Quand le mix s'écarte de la cible (par exemple trop de bas-médiums), il y a deux façons de corriger :\n"
                 "• côté PISTES : les pistes responsables de ces fréquences se corrigent elles-mêmes, en proportion de ce qu'elles apportent (son plus naturel, chaque piste garde sa place) ;\n"
                 "• côté MASTER : l'EQ du Master corrige le mix entier d'un bloc (plus simple, plus global).\n"
                 "Le curseur fixe la part de chacun. Au milieu, c'est moitié-moitié. Sans piste reliée, le Master corrige tout.\n"
                 "Les pistes affichent leur part en violet dans leur carte EQ.") },
            { u ("EQ"),
              u ("10 bandes d'octave réglées par l'Assistant pour rapprocher le mix de la cible, plus AIR (aigus extrêmes, utile pour le vinyle). Double-clic : retour à 0.") },
            { u ("Dynamique"),
              u ("GLUE : compression douce en 3 bandes qui soude le mix. Plus haut = plus dense, mais moins de punch. GR = réduction en cours.\n"
                 "CHALEUR : saturation légère qui ajoute de la densité et de la rondeur.") },
            { u ("Image"),
              u ("LARGEUR : élargit ou resserre la stéréo (100 % = inchangé).\n"
                 "BASSES MONO : met en mono tout ce qui est sous la fréquence choisie. Indispensable en club et pour le vinyle.") },
            { u ("Maximizer"),
              u ("PUSH : gain envoyé dans le limiteur, c'est lui qui fait monter le volume.\n"
                 "CLIP : écrêtage doux des pointes très brèves avant le limiteur (technique des masters électro) : plus de volume sans pompage. Trop haut, le son durcit.\n"
                 "PLAFOND : crête maximale autorisée (true peak, mesurée entre les échantillons).\n"
                 "RELÂCHEMENT : vitesse de récupération du limiteur. Court = plus fort mais plus agressif.") },
            { u ("Révision du mix"),
              u ("Le bouton RÉVISION DU MIX (panneau Pistes reliées) fait le bilan complet, mis à jour en direct pendant la lecture :\n"
                 "• MIX GLOBAL : volume final, crêtes, équilibre par rapport à la cible, stéréo et phase, compression, marge.\n"
                 "• PISTES : pour chacune, son gain, sa saturation éventuelle, sa détection, la place qu'elle laisse aux autres, la correction demandée par le Master.\n"
                 "• CONFLITS : les pistes qui se disputent les mêmes fréquences (kick/basse, voix/instru…) et si c'est déjà réglé. Pour un mashup, vérifie aussi que la voix passe au-dessus de l'instru.\n"
                 "Vert = OK, bleu = info, orange = à vérifier, rouge = problème. La note sur 100 résume le tout.") },
            { u ("Pistes reliées"),
              u ("Liste des MixPilot Track présents dans le projet, avec leur instrument et le gain qu'ils appliquent. Pastille verte = piste analysée et corrigée.") },
            { u ("Conseils"),
              u ("La barre CONSEIL analyse la situation en continu (volume, crêtes, kick/basse, référence…) et propose quoi faire. Clique dessus pour passer au conseil suivant.") },
            { u ("Bonnes pratiques"),
              u ("Garde de la marge : le mix qui arrive au Master ne doit pas dépasser 0 dBFS.\n"
                 "Juge toujours avec A/B à volume égal. Fais des pauses : l'oreille s'habitue.\n"
                 "Vérifie sur plusieurs systèmes (casque, enceintes, téléphone).") },
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
        g.drawText (u ("Analyse de la référence… ") + String (roundToInt (proc.getReferenceProgress() * 100)) + " %", r, Justification::centred);
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
        g.drawText (u ("Glisse un morceau de référence ici (WAV, AIFF, FLAC, MP3, M4A)"), r, Justification::centred);
    }

    if (dragHover)
    {
        g.setColour (col::purple); g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }
}

//==============================================================================
MasterEditor::MasterEditor (MasterProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    auto& vts = proc.apvts;

    for (auto* z : { &statusZone, &loudZone, &tracksZone, &refInfoZone, &corrZone }) addAndMakeVisible (z);
    statusZone.setTooltip (u ("État de l'Assistant et cible de la sortie choisie (volume en LUFS et plafond de crête en dBTP)."));
    loudZone.setTooltip (u ("Mesures de volume de ta sortie. LUFS intégré = volume moyen perçu depuis le dernier RESET. Les pastilles vertes indiquent que la sortie est conforme."));
    tracksZone.setTooltip (u ("Les pistes équipées de MixPilot Track. Pastille verte = analysée et corrigée. À droite : le gain appliqué à la piste."));
    refInfoZone.setTooltip (u ("Infos de la référence : volume intégré (LUFS), crête (dBTP), plage dynamique (LRA) et durée."));
    corrZone.setTooltip (u ("Choisis ce que vise l'Assistant (style ou référence) et qui corrige le mix (les pistes, le Master, ou les deux)."));

    // --- En-tête -------------------------------------------------------------
    for (int i = 0; i < mp::NumGenres; ++i)  genreBox.addItem  (u (mp::genreProfile (i).name), i + 1);
    for (int i = 0; i < mp::NumOutputs; ++i) outputBox.addItem (u (mp::outputProfile (i).name), i + 1);
    addAndMakeVisible (genreBox); addAndMakeVisible (outputBox);
    genreAtt  = std::make_unique<CA> (vts, "genre",  genreBox);
    outputAtt = std::make_unique<CA> (vts, "output", outputBox);
    genreBox.setTooltip (u ("Style musical : définit l'équilibre visé, la densité de compression et la largeur. Les pistes reliées suivent ce choix."));
    outputBox.setTooltip (u ("Où sera diffusé le morceau : fixe le volume final (LUFS) et le plafond de crête. Ex. Spotify -14 LUFS, club -8 LUFS."));

    amountSlider.setSliderStyle (Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (Slider::TextBoxRight, false, 44, 18);
    amountSlider.setColour (Slider::trackColourId, col::accent);
    amountSlider.setColour (Slider::backgroundColourId, col::border);
    amountSlider.setColour (Slider::thumbColourId, col::text);
    amountSlider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    amountSlider.setTooltip (u ("Dose de correction de l'Assistant : 0 % = aucune, 100 % = complète. 70 à 85 % sonne naturel."));
    addAndMakeVisible (amountSlider);
    sliderAtt.push_back (std::make_unique<SA> (vts, "amount", amountSlider));

    assistantBtn.setColour (TextButton::buttonColourId, col::accent);
    assistantBtn.setTooltip (u ("Lance l'analyse complète (30 à 40 s) pendant le passage le plus fort : les pistes se corrigent, puis le Master cale l'équilibre et le volume."));
    assistantBtn.onClick = [this]
    {
        const auto ph = proc.getPhase();
        if (ph == MasterProcessor::Phase::WaitTracks || ph == MasterProcessor::Phase::Listening || ph == MasterProcessor::Phase::Verifying) proc.cancelAssistant();
        else proc.startAssistant();
    };
    addAndMakeVisible (assistantBtn);

    stylePower (compareBtn);
    compareBtn.setColour (TextButton::buttonOnColourId, col::warn);
    compareBtn.setTooltip (u ("A/B : écoute le son d'origine au même volume que le son traité, pour juger honnêtement."));
    compareBtn.onClick = [this] { proc.setCompare (compareBtn.getToggleState()); };
    addAndMakeVisible (compareBtn);

    stylePower (bypassBtn);
    bypassBtn.setColour (TextButton::buttonOnColourId, col::bad);
    bypassBtn.setTooltip (u ("Coupe tout le traitement du Master."));
    addAndMakeVisible (bypassBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "bypass", bypassBtn));

    styleHelpButton (helpBtn);
    helpBtn.onClick = [this] { help->setVisible (true); help->toFront (true); };
    addAndMakeVisible (helpBtn);

    resetLufsBtn.setTooltip (u ("Remet à zéro la mesure du volume intégré (pour mesurer un nouveau passage)."));
    resetLufsBtn.onClick = [this] { proc.outMeter.resetIntegrated(); };
    addAndMakeVisible (resetLufsBtn);

    addAndMakeVisible (tipsBar);

    // --- Référence -----------------------------------------------------------
    loadRefBtn.setButtonText ("CHARGER");
    loadRefBtn.setTooltip (u ("Choisis un morceau de référence (WAV, AIFF, FLAC, MP3, M4A). Tu peux aussi le glisser sur la forme d'onde."));
    loadRefBtn.onClick = [this] { chooseReferenceFile(); };
    clearRefBtn.setTooltip (u ("Retire la référence."));
    clearRefBtn.onClick = [this] { proc.clearReference(); };
    listenRefBtn.setButtonText (u ("ÉCOUTER RÉF."));
    stylePower (listenRefBtn);
    listenRefBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.3f));
    listenRefBtn.setTooltip (u ("Remplace ta sortie par la référence pour comparer. Reclique pour revenir à ton mix."));
    listenRefBtn.onClick = [this] { proc.setReferenceListening (listenRefBtn.getToggleState()); };
    matchRefBtn.setButtonText (u ("VOL. ÉGAL"));
    stylePower (matchRefBtn);
    matchRefBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.3f));
    matchRefBtn.setToggleState (proc.isReferenceMatched(), dontSendNotification);
    matchRefBtn.setTooltip (u ("Joue la référence au même volume que ton mix : tu compares le son, pas le volume."));
    matchRefBtn.onClick = [this] { proc.setReferenceMatch (matchRefBtn.getToggleState()); };
    waveform.setTooltip (u ("Forme d'onde de la référence. Clique pour te placer. Glisse un fichier ici pour en charger un autre."));
    for (auto* c : std::initializer_list<Component*> { &loadRefBtn, &clearRefBtn, &listenRefBtn, &matchRefBtn, &waveform }) addAndMakeVisible (c);

    // --- Cible et répartition -------------------------------------------------
    targetBox.addItem ("Style", 1);
    targetBox.addItem (u ("Référence"), 2);
    targetBox.setTooltip (u ("Ce que vise l'Assistant : l'équilibre type du STYLE, ou celui de ta RÉFÉRENCE (spectre et largeur)."));
    addAndMakeVisible (targetBox);
    targetAtt = std::make_unique<CA> (vts, "target", targetBox);

    refLevelBtn.setButtonText (u ("VOL. RÉF."));
    stylePower (refLevelBtn);
    refLevelBtn.setColour (TextButton::buttonOnColourId, col::purple.darker (0.3f));
    refLevelBtn.setTooltip (u ("Vise aussi le volume de la référence (au lieu de celui de la sortie). Les crêtes restent sous le plafond de la sortie."));
    addAndMakeVisible (refLevelBtn);
    buttonAtt.push_back (std::make_unique<BA> (vts, "refLevel", refLevelBtn));

    shareSlider.setSliderStyle (Slider::LinearHorizontal);
    shareSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    shareSlider.setColour (Slider::trackColourId, col::purple.withAlpha (0.6f));
    shareSlider.setColour (Slider::backgroundColourId, col::accent.withAlpha (0.6f));
    shareSlider.setColour (Slider::thumbColourId, col::text);
    shareSlider.setPopupDisplayEnabled (true, true, this);
    shareSlider.setTooltip (u ("RÉPARTITION : qui corrige l'équilibre du mix. À gauche, les pistes responsables se corrigent elles-mêmes (plus naturel). À droite, l'EQ du Master corrige tout d'un bloc. Au milieu : moitié-moitié."));
    addAndMakeVisible (shareSlider);
    sliderAtt.push_back (std::make_unique<SA> (vts, "masterShare", shareSlider));
    shareSlider.setDoubleClickReturnValue (true, 0.5);

    // --- Modules -------------------------------------------------------------
    struct P { TextButton* b; const char* id; const char* tip; };
    P powers[] = {
        { &eqOn,    "eqOn",    "Active/désactive l'EQ du Master. Les curseurs sont réglés par l'Assistant et restent modifiables." },
        { &dynOn,   "dynOn",   "Active/désactive la compression multibande (Glue) et la saturation (Chaleur)." },
        { &imageOn, "imageOn", "Active/désactive le traitement stéréo (Largeur et Basses mono)." },
        { &maxOn,   "maxOn",   "Active/désactive le maximizer (Push + limiteur true peak). Désactivé, le volume n'est plus calé sur la sortie." } };
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
        s.setTooltip (i == mp::kBands ? u ("AIR : adoucit les aigus extrêmes (au-dessus de 12 kHz). Utile pour le vinyle ou un mix agressif.")
                                      : u ("Bande ") + eqLabels[i] + u (" Hz. Monte (+) ou baisse (−) cette zone de fréquences sur tout le mix. Double-clic : 0 dB."));
        addAndMakeVisible (s);
        sliderAtt.push_back (std::make_unique<SA> (vts, i < mp::kBands ? "eq" + String (i) : String ("highTame"), s));
        s.setDoubleClickReturnValue (true, 0.0);
    }

    addKnob (glue,     "glue",     u ("GLUE : compression douce sur 3 bandes qui soude le mix. Plus haut = plus dense, mais moins de punch."));
    addKnob (warmth,   "warmth",   u ("CHALEUR : saturation légère qui ajoute densité et rondeur. Trop haut, le son peut devenir terne."), col::warn);
    addKnob (width,    "width",    u ("LARGEUR : élargit (> 100 %) ou resserre (< 100 %) la stéréo. Les graves restent stables."), col::accent2);
    addKnob (monoBass, "monoBass", u ("BASSES MONO : met en mono tout ce qui est sous cette fréquence. Recommandé pour le club (120 Hz) et le vinyle (150 Hz)."), col::accent2);
    addKnob (push,     "push",     u ("PUSH : gain envoyé dans le limiteur. C'est lui qui fait monter le volume final (réglé automatiquement par l'Assistant)."));
    addKnob (clipKnob, "clip",     u ("CLIP : arrondit les pointes très brèves (attaques de kick, caisse claire) avant le limiteur. Plus de volume sans pompage ; trop haut, le son peut durcir. Électro : 60 à 80 %."), col::warn);
    addKnob (ceiling,  "ceiling",  u ("PLAFOND : crête maximale (true peak). -1 dBTP pour le streaming, -0,3 dBTP pour club/CD."), col::bad);
    addKnob (release,  "release",  u ("RELÂCHEMENT : vitesse de récupération du limiteur. Court = plus fort mais plus agressif ; long = plus doux."));

    // --- Spectre -------------------------------------------------------------
    spectrum.fftSize = mp::SpectrumAnalyzer::fftSize;
    spectrum.eqCurve = [this] (float f) { return proc.eqResponseDb (f); };
    spectrum.showTarget = true;
    spectrum.caption = u ("ENTRÉE (gris) · SORTIE (bleu) · CIBLE (pointillés) · RÉFÉRENCE (violet) · CORRECTION (vert)");
    spectrum.setTooltip (u ("Spectre du mix. Gris = entrée, bleu = sortie, pointillés = cible visée, violet = référence, vert = correction d'EQ du Master."));
    addAndMakeVisible (spectrum);

    help = std::make_unique<HelpOverlay> (u ("Guide de MixPilot Master"), masterHelp());
    addChildComponent (*help);

    reviewBtn.setButtonText (u ("RÉVISION DU MIX"));
    reviewBtn.setColour (TextButton::buttonColourId, col::accent.withAlpha (0.35f));
    reviewBtn.setTooltip (u ("Bilan complet du mix : chaque piste (niveau, saturation, détection), les conflits entre pistes, et le mix global (volume, crêtes, équilibre, stéréo, compression), avec une note et quoi faire."));
    reviewBtn.onClick = [this] { review->setReview (proc.buildReview()); review->setVisible (true); review->toFront (true); };
    addAndMakeVisible (reviewBtn);
    review = std::make_unique<ReviewOverlay>();
    addChildComponent (*review);

    setSize (1040, 764);
    startTimerHz (30);
}

MasterEditor::~MasterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MasterEditor::addKnob (Slider& s, const char* id, const String& tip, Colour c)
{
    setupKnob (s, c);
    s.setTooltip (tip);
    addAndMakeVisible (s);
    sliderAtt.push_back (std::make_unique<SA> (proc.apvts, id, s));
}

void MasterEditor::chooseReferenceFile()
{
    chooser = std::make_unique<FileChooser> (u ("Choisis un morceau de référence"), File::getSpecialLocation (File::userMusicDirectory),
                                             "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) proc.loadReference (f);
                          });
}

bool MasterEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;ogg")) return true;
    return false;
}

void MasterEditor::filesDropped (const StringArray& files, int, int)
{
    waveform.dragHover = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;ogg")) { proc.loadReference (File (f)); break; }
    repaint();
}

//==============================================================================
void MasterEditor::timerCallback()
{
    auto ref = proc.getReference();
    spectrum.specIn  = proc.inAnalyzer.getDisplayDb();
    spectrum.specOut = proc.outAnalyzer.getDisplayDb();
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
void MasterEditor::paint (Graphics& g)
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
    g.drawText ("SORTIE",  outputBox.getX(),    4, 100, 12, Justification::left);
    g.drawText (u ("INTENSITÉ"), amountSlider.getX(), 4, 100, 12, Justification::left);

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
        g.drawText (u ("Cible : ") + String (proc.hasLearned() ? proc.getTargetLufs() : defT, 1) + " LUFS  |  " + String (op.ceilingDb, 1) + " dBTP"
                    + (op.monoBassHz > 0 ? u ("  |  basses mono < ") + String ((int) op.monoBassHz) + " Hz" : String()),
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
    drawKnobTitle (g, warmth, "CHALEUR");
    drawKnobTitle (g, width, "LARGEUR");
    drawKnobTitle (g, monoBass, "BASSES MONO");
    drawKnobTitle (g, push, "PUSH");
    drawKnobTitle (g, clipKnob, "CLIP");
    drawKnobTitle (g, ceiling, "PLAFOND");
    drawKnobTitle (g, release, u ("RELÂCHEMENT"));

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

void MasterEditor::drawKnobTitle (Graphics& g, const Slider& s, const String& t)
{
    g.setColour (col::textDim); g.setFont (font (10.0f, true));
    g.drawText (t, s.getX() - 10, s.getY() - 14, s.getWidth() + 20, 12, Justification::centred);
}

void MasterEditor::drawReferencePanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (12, 8);
    auto info = r.removeFromLeft (190);
    drawSectionTitle (g, info.removeFromTop (16), u ("RÉFÉRENCE"));

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
        g.drawFittedText (err.isNotEmpty() ? err : u ("Aucune. Charge un morceau qui sonne comme tu veux."), info.removeFromTop (34), Justification::topLeft, 2);
    }
}

void MasterEditor::drawCorrectionPanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (12, 8);
    auto row1 = r.removeFromTop (24);
    g.setColour (col::textDim); g.setFont (font (10.5f, true));
    g.drawText ("CIBLE", row1.removeFromLeft (44), Justification::centredLeft);

    r.removeFromTop (6);
    auto lab = r.removeFromTop (14);
    const bool masterOnly = tracks.empty() || proc.getTrackShare() <= 0.0f;
    g.setFont (font (10.0f, true));
    g.setColour (col::purple);  g.drawText ("PISTES", lab, Justification::centredLeft);
    g.setColour (col::accent);  g.drawText ("MASTER", lab, Justification::centredRight);
    g.setColour (col::textDim);
    g.setFont (font (masterOnly ? 9.0f : 10.0f, ! masterOnly));
    g.drawText (masterOnly ? (tracks.empty() ? u ("sans piste : Master seul") : u ("Master seul")) : u ("RÉPARTITION"), lab, Justification::centred);
}

void MasterEditor::drawLoudnessPanel (Graphics& g, Rectangle<int> area)
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
    g.drawText (u ("LUFS INTÉGRÉ"), big.removeFromTop (28), Justification::bottomLeft);
    g.drawText (hasInt ? (diff >= 0 ? "+" : "") + String (diff, 1) + " vs cible" : u ("en attente…"), big, Justification::topLeft);

    r.removeFromTop (2);
    auto row = [&] (const String& k, const String& v, Colour c)
    {
        auto line = r.removeFromTop (20);
        g.setFont (font (12.0f)); g.setColour (col::textDim);
        g.drawText (k, line, Justification::centredLeft);
        g.setFont (font (12.5f, true)); g.setColour (c);
        g.drawText (v, line, Justification::centredRight);
    };
    row ("Court terme (3 s)", shortTerm > -69 ? String (shortTerm, 1) + " LUFS" : String ("--"), col::text);
    row ("True peak (max)",   tpHold > -99 ? String (tpHold, 1) + " dBTP" : String ("--"), okTP ? col::text : col::bad);
    if (auto ref = proc.getReference())
        row (u ("Référence"), String (ref->lufs, 1) + " LUFS", col::purple);
    else
        row (u ("Réduction glue"), String (glueGr, 1) + " dB", col::text);

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
    chk (okL,  "Volume conforme (" + String (target, 1) + " LUFS)");
    chk (okTP && tpHold > -99, u ("Pas de saturation (≤ ") + String (op.ceilingDb, 1) + " dBTP)");
}

void MasterEditor::drawTrackPanel (Graphics& g, Rectangle<int> area)
{
    drawPanel (g, area);
    auto r = area.reduced (14, 10);
    auto head = r.removeFromTop (18);
    drawSectionTitle (g, head, u ("PISTES RELIÉES (") + String ((int) tracks.size()) + ")");
    r.removeFromTop (6);

    if (tracks.empty())
    {
        g.setColour (col::textDim); g.setFont (font (12.0f));
        g.drawFittedText (u ("Aucune piste.\nAjoute MixPilot Track sur chaque piste (après l'instrument ou les effets) : elles apparaîtront ici."),
                          r.removeFromTop (80), Justification::topLeft, 5);
        return;
    }

    static const char* phaseTxt[] = { "-", "écoute", "réglage", "OK" };
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
        g.drawText (t.name.isEmpty() ? "Piste" : t.name, line.removeFromLeft (78), Justification::centredLeft, true);
        g.setFont (font (11.0f)); g.setColour (col::textDim);
        g.drawText (t.hasProfile ? instrumentName (t.instrument) : String ("?"), line.removeFromLeft (80), Justification::centredLeft, true);
        g.setColour (ph == 3 ? col::text : col::textDim);
        g.drawText (ph == 3 ? (t.trimDb >= 0 ? "+" : "") + String (t.trimDb, 1) + " dB" : u (phaseTxt[ph]), line, Justification::centredRight);
    }
}

//==============================================================================
void MasterEditor::resized()
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

    const int top = 138, specH = 230;
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
        row1.removeFromLeft (44);
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
    reviewBtn.setBounds (tracksArea.getRight() - 140, tracksArea.getY() + 8, 128, 22);
}
