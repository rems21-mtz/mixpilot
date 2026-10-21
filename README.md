# MixPilot — mixage & mastering assistés (VST3 / AU)

Deux plugins qui travaillent ensemble :

| Plugin | Où le mettre | Rôle |
|---|---|---|
| **MixPilot Track** | Sur **chaque piste**, en **dernier** dans la chaîne (après l'instrument sur une piste MIDI) | Détecte l'instrument, nettoie, égalise, compresse, place la piste au bon niveau et lui fait de la place |
| **MixPilot Master** | Sur la piste **Master**, en **dernier** | Choix du **style** et de la **sortie**, EQ, glue multibande, image stéréo, chaleur, maximizer true peak, loudness |

Les deux sont **reliés automatiquement** : le Master envoie le style, la sortie et l'ordre d'analyse. Chaque piste lui renvoie son instrument, son niveau et ses corrections, et tient compte des autres pistes.

## Langue

L'interface des deux plugins (boutons, bulles d'aide, guide « ? », conseils, révision du mix) est en **anglais**. Ce README reste en français et cite les boutons sous leur nom anglais.

## Compiler sur Mac (une fois, ~10 min)

```bash
cd ~/Documents/MixPilot
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

Les plugins s'installent tout seuls dans `~/Library/Audio/Plug-Ins/VST3` et `Components` (AU).
Dans Ableton : Réglages → Plug-ins → **Rescan**. Ils apparaissent sous **Rems**.

## Compiler sur Windows (PC)

À installer une fois (gratuit) :
- **Visual Studio 2022 Community** avec la charge de travail « Développement Desktop en C++ »
- **CMake** (cocher « Add to PATH ») et **Git**

Puis, dans « Developer PowerShell for VS 2022 », depuis le dossier MixPilot :

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Copie ensuite les deux dossiers `.vst3` (dans `build\MixPilotMaster_artefacts\Release\VST3` et `build\MixPilotTrack_artefacts\Release\VST3`) dans `C:\Program Files\Common Files\VST3`.
Dans Ableton : Préférences → Plug-ins → activer « Use VST3 Plug-In System Folders » → **Rescan**.
Sur Windows, seul le format VST3 est produit (l'AU n'existe que sur Mac).

### Sans PC : compilation en ligne avec GitHub
Le fichier `.github/workflows/build.yml` compile la version Windows sur les serveurs de GitHub à chaque envoi de code.
Onglet **Actions** du dépôt → dernière exécution → en bas, **Artifacts** → `MixPilot-Windows-VST3` (zip contenant les deux `.vst3`).

## Utilisation en 4 étapes

1. Mets **MixPilot Track** à la fin de chaque piste (kick, basse, voix, synthés…).
2. Mets **MixPilot Master** à la fin du Master. Choisis le **Style** et la **Sortie**.
3. Lance la lecture sur le **passage le plus fort** du morceau et clique **ASSISTANT** (environ 30 à 40 s) :
   1. Toutes les pistes s'écoutent, se reconnaissent et se corrigent.
   2. Le Master écoute le mix obtenu.
   3. Il cale le volume final sur la cible.
4. Ajuste à ton goût : tout reste modifiable. Avec **A/B**, tu compares avant/après à volume égal (sans être trompé par le « plus fort = mieux »).

Changer de **Style** ou de **Sortie** ensuite recalcule tout instantanément, sans nouvelle écoute.

## Nouveau : référence, répartition et aides

### Morceau de référence (Master)
- **Glisse** un fichier (WAV, AIFF, FLAC, MP3, M4A) depuis le Finder sur la forme d'onde du panneau REFERENCE, ou clique sur **LOAD**.
- MixPilot mesure son équilibre spectral, son volume (LUFS), ses crêtes (dBTP), sa dynamique (LRA) et sa largeur stéréo. La courbe violette apparaît sur le spectre.
- **TARGET = Reference** : l'Assistant vise l'équilibre et la largeur de la référence au lieu du style.
- **REF LVL** : vise aussi le volume de la référence (les crêtes restent sous le plafond de la sortie).
- **LISTEN REF** remplace la sortie par la référence. Avec **MATCH**, elle est jouée au même volume que ton mix. Clique sur la forme d'onde pour te placer.
- La référence est retrouvée automatiquement quand tu rouvres le projet (le fichier doit rester au même endroit).

### Répartition Pistes ↔ Master
Quand le mix s'écarte de la cible, le curseur **SPLIT** choisit qui corrige :
- vers **TRACKS** : les pistes qui produisent les fréquences en cause se corrigent elles-mêmes, chacune selon ce qu'elle apporte (barres **violettes** dans leur carte EQ) ;
- vers **MASTER** : l'EQ du Master corrige tout le mix d'un bloc.

Après la correction des pistes, le Master réécoute le mix (6 s) et complète ce qui manque. Le résultat vise donc juste quelle que soit la position du curseur.
Sur chaque piste, **FOLLOW MASTER** permet de refuser ces corrections.

### Aides intégrées
- **Bulles d'aide** : passe la souris sur n'importe quel bouton, réglage ou panneau.
- **?** : guide complet de chaque section, dans le plugin.
- **TIP** : conseils automatiques selon la situation (volume, crêtes, kick/basse, détection incertaine, référence…). Clique sur la barre pour passer au suivant.

## Ce que fait chaque piste (MixPilot Track)

- **Instrument** : détecté automatiquement (kick, caisse claire, charley, basse, voix, lead, nappe, guitare, **Full instrumental**) ou choisi à la main (dont **A cappella**).
- **Gain** : place la piste au bon niveau dans le mix selon l'instrument, le style et la sortie.
- **Nettoyage** : coupe-bas adapté (par exemple 90 Hz sur une voix, 300 Hz sur un charley).
- **EQ** : rapproche le spectre de la forme idéale de l'instrument (courbe verte).
- **EQ éditable** : après l'analyse, chaque bande a un point sur la courbe verte. On le glisse à la souris (haut/bas = gain, gauche/droite = fréquence, Maj = réglage fin). La retouche s'ajoute à la correction calculée et reste en place quand elle se met à jour. Double-clic : retour à l'analyse, clic droit : tout réinitialiser. Même chose sur le spectre de MixPilot Master, où les points pilotent les curseurs de la section EQ.
- **Anti-masquage** : la piste se creuse légèrement là où une piste prioritaire joue (barres orange).
- **Compression** : ratio, attaque et relâchement adaptés à l'instrument et au style.
- **Espace** : la basse baisse ses graves quand le kick frappe (ducking automatique, sans sidechain à câbler).
- **Intensité** : de 0 % (rien) à 100 % (correction complète).

## Mashups : Full instrumental + Acapella

- **Full instrumental** : pour un beat ou la version instrumentale d'un morceau. La piste est traitée comme un mix entier (équilibre du style, compression douce). Détectée automatiquement.
- **A cappella** : pour une voix seule. À choisir dans INSTRUMENT (« A cappella ») ; elle devient prioritaire sur tout le reste.
- Quand les deux sont reliées, l'instru **creuse ses médiums (800 Hz – 5 kHz) uniquement quand la voix chante** (2 à 3,5 dB selon le style, carte SPACE) : la voix passe devant sans monter son volume, et l'instru retrouve tout son corps dans les silences.
- MixPilot gère le son ; le tempo et la tonalité se calent dans Ableton (Warp, Transpose).

## Révision du mix (Master)

Bouton **MIX REVIEW** (panneau Linked tracks), mis à jour en direct :
- **Mix global** : volume final, crêtes, équilibre par rapport à la cible, stéréo et phase, compression, marge.
- **Pistes** : gain, saturation en entrée, détection incertaine, niveau réel vs sa place dans le mix, place laissée aux autres, corrections demandées par le Master.
- **Conflits** : pistes qui se disputent les mêmes fréquences (kick/basse, voix/instru…) et si c'est déjà réglé ; pour un mashup, vérifie que la voix passe au-dessus de l'instru.
- Note sur 100 et code couleur : vert OK, bleu info, orange à vérifier, rouge problème.

## Styles

| Style | Équilibre visé | Particularités |
|---|---|---|
| Électro (House / Techno) | kick et basse puissants, aigus nets | glue et clip marqués, stéréo large |
| Trap | sub-basse 808 très présente, charleys brillants | basse +2 dB, ducking 808/kick appuyé, clip marqué |
| Rap / Hip-hop | voix très en avant et intelligible, kick/caisse claire qui claquent, basses rondes | voix +2,5 dB, chaleur plus présente, image centrée |
| Pop | voix devant, son brillant et équilibré | compression dense, clip modéré |
| Rock / Acoustique | dynamique naturelle, médiums chauds | compression légère, guitares +2 dB |

Les projets existants qui utilisaient « Hip-hop / Trap » passent automatiquement en « Trap ».

## Sorties disponibles

Streaming et YouTube sont fixes : ces plateformes baissent les morceaux plus forts, viser plus haut ne sert à rien.
Les autres sorties suivent les usages du style :

| Sortie | Électro | Trap | Rap / Hip-hop | Pop | Rock | Plafond | Particularité |
|---|---|---|---|---|---|---|---|
| Streaming (Spotify / Apple) | -14 | -14 | -14 | -14 | -14 | -1 dBTP | normalisé par la plateforme |
| YouTube | -13 | -13 | -13 | -13 | -13 | -1 dBTP | normalisé par la plateforme |
| SoundCloud | -8 | -8 | -8,5 | -9 | -10 | -1 dBTP | |
| Club / DJ | -7 | -7 | -7,5 | -8 | -8,5 | -0,3 dBTP | basses en mono < 120 Hz |
| CD | -8 | -8 | -8,5 | -9 | -10 | -0,3 dBTP | |
| Vinyle | -10 | -10 | -10,5 | -11 | -12 | -1 dBTP | basses mono < 150 Hz, aigus extrêmes adoucis |

(valeurs en LUFS intégrés)

### Volume : ce qui est optimisé (v3)
- **CLIP** (nouveau, Maximizer) : écrêtage doux des pointes très brèves, suréchantillonné x4, avant le limiteur. Sur le morceau test : +1,3 dB de volume et deux fois moins de travail du limiteur, donc moins de pompage.
- L'EQ du Master corrige plus franchement l'équilibre (graves en excès, médiums en retrait), ce qui augmente le volume perçu.
- La largeur stéréo vise la largeur typique du style.
- Limiteur true peak à deux étages (détection x8) : marge de 0,3 dB sur les sorties fortes, 0,5 dB sur Streaming, YouTube et Vinyle (où elle ne coûte aucun volume).

## Testé avant livraison

- Mashup réel (Test_Mix en instru + acapella) : instru reconnue « Full instrumental » (99 %), creusement de 2,0 dB quand la voix chante et 0,06 dB dans les silences, volume final sur la cible, révision cohérente.

- 5 styles × 6 sorties (30 combinaisons) : volume à ±0,3 LUFS de la cible, true peak toujours sous le plafond (mesure indépendante x8), instruments reconnus 6/6.
- Morceau réel passé dans les 5 styles (SoundCloud) : chaque style atteint sa cible, crêtes à -1,26 dBTP.

- Morceau réel (Test_Mix, électro-house, SoundCloud) : -11,3 → -8,1 LUFS, true peak -1,3 dBTP, équilibre spectral 2 fois plus proche de la cible.

- Référence : analyse, cible spectrale, cible de volume (-11,5 LUFS visés → -11,52 obtenus), écoute à volume égal (écart 0,2 LU), restauration à la réouverture du projet.
- Répartition testée à 0 %, 50 % et 100 % : même précision finale, volume et crêtes toujours conformes.

- Session simulée de 6 pistes dans 8 combinaisons style/sortie : volume final à ±0,3 LUFS de la cible, true peak sous le plafond (mesure indépendante x8), instruments reconnus 6/6.
- 44,1 et 96 kHz, mono et stéréo, tailles de blocs irrégulières, silence, sauvegarde et rechargement du projet.
- Les vrais fichiers .vst3 chargés dans un hôte : Master et pistes se trouvent et se pilotent bien.

## Limites à connaître

- La détection d'instrument repose sur l'analyse du son. Sur des sons atypiques (basse très percussive, effets), vérifie et choisis à la main si besoin.
- Sur une piste MIDI, le plugin corrige **le son de l'instrument**, pas les notes MIDI.
- C'est un excellent point de départ, pas un remplacement de tes oreilles : écoute, compare en A/B, ajuste.
