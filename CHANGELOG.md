# Journal des modifications — miniMoc firmware

Toutes les modifications notables de ce projet sont documentées ici.

Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/).  
Le versionnage suit [Semantic Versioning](https://semver.org/lang/fr/).

---

## [0.2] — 2026-06-01

### Ajouté

#### Ports matériels

- **2 entrées MIDI physiques 5-broches TRS** (port A sur Serial2, port B sur Serial1),
  buffers étendus à 1 024 octets chacun (vs 64 par défaut) pour absorber les rafales.
- **6 sorties MIDI physiques 5-broches TRS** (sorties 1–6 sur Serial8/3/7/6 + les deux
  ports du Teensy réassignés en sortie A et B).
- **USB Host** : jusqu'à 6 périphériques MIDI USB simultanés via hub, gérés par
  USBHost_t36 avec 3 instances de hub pour la robustesse en hot-plug.  
  Les devices occupent 6 slots physiques internes (MIDIUSB1–6) et sont exposés en 3 entrées
  logiques (C, D, E) et 3 sorties logiques (7, 8, 9).

#### Routage MIDI

- **Architecture hybride BASIC + FLUX** : deux couches de routage cumulatives.

  - **Matrice BASIC (5 × 9)** : passthrough par port — active ou désactive simplement une
    connexion entre une entrée (A–E) et une sortie (1–9) sur tous les canaux MIDI.
    Représentée par un bitmask `uint16_t` par entrée.
  - **Flux avancés** : jusqu'à 16 règles simultanées, chacune pouvant agréger plusieurs
    entrées et plusieurs sorties, avec filtrage et remappage de canaux MIDI (par bitmask
    16 bits). Un canal d'entrée peut être redirigé vers un canal de sortie différent.
    Chaque flux peut être actif ou inactif.
  - `recompute_route_matrix()` reconstruit la matrice de routage complète après toute
    modification de BASIC ou d'un flux.

- **Smart Mirror** : chaque message MIDI routé vers une sortie physique est
  simultanément mirroré vers le câble USB correspondant (câbles 6–14), permettant à un
  DAW connecté d'enregistrer en temps réel tout le trafic physique sans câblage
  supplémentaire.

- **Passthrough DAW → matériel** : les messages arrivant depuis le DAW sur les câbles
  USB 6–11 sont acheminés directement vers les sorties physiques 1–6 ; les câbles 12–14
  sont acheminés vers les sorties USB Host 7–9 (si un device est connecté sur le slot
  correspondant).

- **Messages routés** : Note On/Off, Control Change, Program Change, Pitch Bend,
  Aftertouch canal.  
  Les messages Real-Time (Clock, Start, Stop, Continue, Active Sensing, System Reset)
  sont gérés séparément via le mécanisme SYNC.

- **8 presets d'usine** non modifiables, chargeable depuis l'interface :
  `VIDE`, `DIAGONALE`, `MERGE`, `DISPATCH`, `TOTAL`, `SPLIT`, `USB>TRS`, `TRS>USB`.

#### SYNC (horloge MIDI)

- **Maître d'horloge configurable** parmi les 5 entrées (A, B, C, D, E) ou `OFF`.
- Le port maître transmet Clock, Start, Stop, Continue, Active Sensing et System Reset
  vers toutes les sorties physiques (1–6) et USB Host (7–9) ; les Real-Time des autres
  ports sont filtrés pour éviter les doublons.
- Le choix du maître est persisté en EEPROM (adresse 34) et rechargé au démarrage.

#### Presets utilisateur (carte SD)

- **32 presets** sur carte SD dans `/PRESETS/PRST01.DAT` … `PRST32.DAT`.
- **Format v3** : chaque fichier contient un en-tête magique, la `route_matrix` dérivée,
  la `basic_matrix` et la liste de flux.
- **Migration automatique des fichiers v1** : les anciens presets (route_matrix seule)
  sont convertis en BASIC au chargement.
- Sauvegarde et chargement silencieux (sans interruption du routage MIDI en cours).
- L'index du preset actif est persisté en EEPROM (adresse 0) et rechargé au démarrage.

#### Enregistrement MIDI sur SD

- **Enregistrement automatique** de tous les événements MIDI routés, démarrant dès que
  la SD est disponible à l'initialisation.
- **Format binaire 10 octets** par événement :
  `[timestamp 32 bits µs] [sortie 1–9] [source] [type MIDI] [canal] [data1] [data2]`.
  Sources tracées : PC/DAW (0), Entrée A (1), B (2), USB Host C (3), D (4), E (5).
- **Architecture ring buffer** : 4 096 octets en RAM ; `rec_push()` ≈ 50 ns, non
  bloquant. Écriture SD différée par blocs de 280 octets minimum, sync tous les ~4 Ko.
- **Préallocation 20 Mo** au démarrage pour éviter la fragmentation et les pauses SD.
- **Sessions numérotées** de `REC001.DAT` à `REC099.DAT` avec wrap ; compteur en EEPROM
  (adresse 1).
- **Statistiques exposées** dans le sous-menu INFO : kilo-octets écrits, latence max
  d'écriture en µs, nombre de spikes (>3 ms), nombre d'overflows ring buffer.

> **Limitation connue** : `sd_close()` est défini dans le code mais n'est jamais appelé.
> Le fichier d'enregistrement n'est donc jamais fermé proprement en cours d'utilisation ;
> en particulier, les 20 Mo préalloués ne sont pas libérés. Les données sont cependant
> préservées grâce au `sync()` périodique. Il n'existe pas non plus de commande dans
> l'interface pour démarrer ou arrêter manuellement l'enregistrement.

#### Accès SD depuis le PC (MTP)

- La carte SD est exposée en tant que volume `MiniMoc SD` via le protocole MTP
  (MTP_Teensy), permettant de récupérer les fichiers `REC*.DAT` et `PRST*.DAT` depuis
  le PC sans retirer la carte.
- Un adaptateur `SdFsAdapter` fait le pont entre SdFat (utilisé pour l'enregistrement)
  et l'interface `FS` requise par MTP_Teensy, sans réinitialiser le contrôleur SDIO.

#### Configuration USB Host

- **Mode AUTO** (défaut) : les devices USB MIDI sont assignés aux ports logiques
  (C/D/E/7/8/9) dans l'ordre de connexion (FIFO).
- **Mode MANUEL (LOCK)** : l'assignation port logique → slot physique est fixée par
  l'utilisateur et mémorisée par VID:PID. À la reconnexion, `hc_poll()` retrouve chaque
  device par son empreinte VID:PID, quel que soit son slot d'arrivée.
- Le mode et l'assignation sont persistés en EEPROM (adresses 2–33).

#### Interface utilisateur — OLED + encodeur + boutons

- **Écran OLED SSD1306 128 × 64 px** en I2C (400 kHz). L'oscillateur SSD1306 est réglé
  au maximum (registre `0xD5 = 0xF0`) pour la stabilité.
- **Encodeur rotatif** (pins 39/40) avec anti-rebond à deux niveaux : fenêtre de 120 ms
  + délai d'application de 60 ms. Un clic simultané annule la rotation en attente
  (protection contre les artefacts mécaniques).
- **Bouton SELECT** (pin 41, Bounce2, 5 ms) et **bouton BACK** (pin 38, 150 ms).
- **Appui long BACK (1 s)** : bascule entre le logo de veille et l'interface interactive.
  Un mécanisme de relâchement empêche un réveil immédiat après retour au logo.
- **Logo de veille** : affiche le logo Verveine 64 × 64 px et la version du firmware.
  Tout appui ou rotation de l'encodeur réveille l'interface.

**Structure de navigation** :

- **Carousel principal** : 5 sections — PRESET / SYNC / ROUTAGE / MONITOR / SYSTEM —
  avec animation de glissement horizontal (36 px par step, 16 px/frame) et affichage de
  l'icône 32 × 32 de la section active.  
  > Note : le code pour afficher les icônes 16 × 16 des sections adjacentes est présent
  > mais commenté ; seul l'icône central est actuellement rendu.

- **Sous-menu PRESET** : sélection du numéro de preset (1–32, indicateur `*` si le
  preset existe sur SD), sauvegarde, chargement, application d'un preset d'usine.
  Messages de confirmation centrés à l'écran pendant 1,2 s. Indicateur `SD indisponible`
  si la carte est absente.

- **Sous-menu SYNC** : grille 2 × 3 de grandes cases (A / B / C / D / E / OFF) ;
  la case active est affichée en inversé. Validation immédiate et persistance EEPROM.

- **Sous-menu ROUTAGE** : deux modes d'édition accessibles depuis un menu principal :
  - **MATRICE** : grille interactive 5 × 9 (entrées A–E en lignes, sorties 1–9 en
    colonnes). Un appui SELECT bascule chaque case. Mise à jour instantanée du routage.
  - **FLUX** : liste scrollable des flux existants (4 lignes visibles) avec résumé
    compact (`F1 - A → 1 4`). Création d'un nouveau flux, ou modification/suppression
    d'un flux existant via menu contextuel.  
    Création en deux étapes : sélection des ports d'entrée (A–E, appui court = toggle
    tout-canal, appui long = ouvre un picker de canaux 1–16), puis des ports de sortie
    (1–9, idem). Validation finale sur `✓ VALIDER`.

- **Sous-menu MONITOR** : VU-mètre temps réel — 5 barres d'entrée (A/B/C/D/E) en haut,
  séparateur horizontal, 9 barres de sortie (1–9) en bas. Décroissance en 500 ms.
  Rafraîchissement continu (indépendant des entrées utilisateur).

- **Sous-menu SYSTEM** : 5 entrées :
  - **INFO** (2 pages) : page 1 = preset actif + statistiques enregistrement SD ;
    page 2 = liste des 6 slots USB Host avec nom de produit ou VID:PID.
  - **HOST CONFIG** : bascule AUTO/MANUEL, assignation manuelle des 6 ports logiques
    vers les slots physiques D1–D6 via un popup scrollable.
  - **USB LINK** : polling intensif du bus USB pendant 2,5 s pour forcer l'énumération
    de devices non reconnus, sans reboot. Affiche le nombre de devices détectés.
  - **USB RESET** : redémarrage complet du Teensy (`SCB_AIRCR = SYSRESETREQ`) — à
    utiliser si USB LINK échoue.
  - **CONTRASTE** : réglage du contraste OLED par pas de 16 (valeurs 16–255) avec barre
    de progression et valeur en %. OK sauvegarde en EEPROM ; BACK annule.

#### Éditeur web — protocole SysEx

- Communication via câble USB 0, manufacturer ID `0x7D`.
- **Commandes reçues depuis le PC** :

  | Code | Action |
  |------|--------|
  | `0x01` | Mise à jour directe d'une cellule `route_matrix` (maintenu pour compatibilité éditeur legacy) |
  | `0x02` | Demande de dump complet vers le PC |
  | `0x05` | Charger le preset N depuis SD |
  | `0x06` | Sauvegarder dans le preset N sur SD |
  | `0x08` | Ping / heartbeat → réponse pong |
  | `0x09` | Mise à jour d'une case `basic_matrix` |
  | `0x0A` | Ajout ou mise à jour d'un flux |
  | `0x0B` | Suppression d'un flux |
  | `0x0C` | Définir le maître SYNC |
  | `0x0D` | Définir la configuration USB Host |
  | `0x0E` | Quick poll (dump allégé : preset, sync, basic_matrix, flux, host config) |
  | `0x10` | Reboot en mode bootloader HalfKay |

- **Dump complet** (`0x02`) : envoie `route_matrix`, preset actif, `basic_matrix`,
  liste de flux, maître SYNC, configuration USB Host, et noms de produits (ou VID:PID)
  des devices USB Host connectés.
- Les valeurs 16 bits sont encodées sur 3 octets SysEx 7 bits pour rester dans les
  limites du standard MIDI.

#### Mise à jour firmware OTA (via USB CDC)

- Mise à jour du firmware sans bootloader HalfKay, depuis un PC connecté en USB.
- **Protocole** :
  1. PC → Teensy : `START_UPDATE:{taille}\n`
  2. Teensy → PC : `ACK\n`
  3. Transfert par blocs de 256 octets, chaque bloc acquitté par `ACK\n`
  4. PC → Teensy : `END_UPDATE:{crc32hex}\n`
  5. Vérification CRC32 (IEEE 802.3) → `OK\n` ou `ERR:{raison}\n`
- **Zone de staging** : le nouveau firmware est écrit dans la moitié haute de la flash
  QSPI (offset 4 MiB, 4 MiB disponibles) sans risquer le firmware actif.
- **Application en ITCM** (`FASTRUN`) : la copie staging → partition active s'exécute
  entièrement depuis la SRAM (ITCM), IRQ désactivées, sans dépendre du code en flash.
- `ota_tick()` est non bloquant dans la boucle principale ; il bloque uniquement pendant
  la durée effective d'un transfert.

#### Identité USB

- Le Teensy s'énumère sous le nom `MINIMOC` (descriptor USB défini dans `name.c`).

---

*Changelog généré à partir du code source du firmware — version interne `v2.0.2`.*
