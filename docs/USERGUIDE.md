# Mode d'emploi — miniMoc

> Version 0.5.2

---

## Sommaire

1. [Présentation](#1-présentation)
2. [Matériel — ce qu'il y a dans la boîte](#2-matériel)
3. [Connexions MIDI](#3-connexions-midi)
4. [Interface physique — écran et encodeur](#4-interface-physique)
5. [Smart Mirror — connexion à un DAW](#5-smart-mirror)
6. [Routage basique (MATRICE)](#6-routage-basique-matrice)
7. [Routage avancé (FLUX)](#7-routage-avancé-flux)
8. [Synchronisation MIDI Clock (SYNC)](#8-synchronisation-midi-clock-sync)
9. [Gestion des presets](#9-gestion-des-presets)
10. [Appareils USB (HOST CONFIG)](#10-appareils-usb-host-config)
11. [Enregistrement MIDI sur carte SD](#11-enregistrement-midi-sur-carte-sd)
12. [Éditeur web (weblink)](#12-éditeur-web-weblink)
13. [Mise à jour du firmware](#13-mise-à-jour-du-firmware)
14. [Référence des menus OLED](#14-référence-des-menus-oled)
15. [Dépannage](#15-dépannage)

---

## 1. Présentation

Le **miniMoc** est un routeur MIDI hardware fait main, conçu pour fonctionner **sans ordinateur**. Il permet de relier des synthétiseurs, boîtes à rythmes et autres instruments MIDI entre eux avec une grande flexibilité.

**Fonctionnalités principales :**

- 2 entrées MIDI physiques TRS type A (a, b) + 3 entrées USB Host (c, d, e)
- 6 sorties MIDI physiques TRS (1–6) + 3 sorties USB Host (7–9)
- Routage basique par matrice 5×9 (un bouton = une connexion)
- Routage avancé par **Flux** : filtrage par canal, remapping de canal
- **Smart Mirror** : les ports physiques apparaissent automatiquement dans votre DAW quand vous branchez un PC — sans driver, sans configuration
- Synchronisation MIDI Clock : choix d'un maître parmi 10 sources externes (TRS/USB Host physique ou câble USB/PC, au choix) **ou l'horloge interne du miniMoc lui-même**, diffusion sur toutes les sorties
- Menu **TRANSPORT** : réglage du tempo de l'horloge interne, Play/Pause/Stop, et pilotage d'un enregistreur externe (MMC Record/Avance/Rewind)
- **Transformateurs de Flux** : transposition, harmonisation, réharmonisation sur une progression d'accords
- **Générateurs** internes : LFO, séquenceur euclidien, motifs de batterie pré-enregistrés
- 32 presets utilisateur + 8 presets d'usine, stockés sur carte SD
- Mise à jour firmware OTA via l'éditeur web

> L'enregistrement MIDI continu sur carte SD (capture automatique des jams) fait partie
> du projet mais est **désactivé par défaut et reporté après la V1** — voir
> [§11](#11-enregistrement-midi-sur-carte-sd).

---

## 2. Matériel

| Composant | Description |
|-----------|-------------|
| Teensy 4.1 | Microcontrôleur principal (ARM Cortex-M7 @ 600 MHz) |
| Écran OLED | 128×64 px, interface I²C |
| Encodeur rotatif | Navigation dans les menus |
| Bouton SELECT | Valider une sélection |
| Bouton BACK | Revenir en arrière / annuler |
| Carte SD | Presets et enregistrements (FAT32) |
| Ports TRS | Connecteurs 3,5 mm Type A (compatibles MIDI TRS et adaptateurs 5-broches DIN) |

---

## 3. Connexions MIDI

### Entrées

| Port | Type | Description |
|------|------|-------------|
| **A** | TRS 3,5 mm | Entrée physique — câble TRS ou adaptateur DIN |
| **B** | TRS 3,5 mm | Entrée physique — câble TRS ou adaptateur DIN |
| **C** | USB Host | Appareils USB MIDI branchés directement sur le miniMoc |
| **D** | USB Host | (idem) |
| **E** | USB Host | (idem) |

### Sorties

| Port | Type | Description |
|------|------|-------------|
| **1–6** | TRS 3,5 mm | Sorties physiques — câble TRS ou adaptateur DIN |
| **7–9** | USB Host | Sorties vers les appareils USB MIDI connectés |

### Connexion PC / DAW

Branchez le miniMoc sur votre ordinateur via USB. Il apparaît immédiatement comme un **périphérique MIDI USB multi-port** (15 câbles virtuels). Aucun driver n'est nécessaire.

> **Astuce** : débranchez le PC, le miniMoc continue de fonctionner seul. Rebranchez, et les mêmes ports réapparaissent dans votre DAW.

---

## 4. Interface physique

L'interface est composée de :

- **Encodeur rotatif** : tourner pour naviguer dans les listes / changer une valeur
- **Bouton SELECT** (appuyer sur l'encodeur) : valider / entrer dans un sous-menu
- **Bouton BACK** : revenir au niveau précédent

### Navigation dans le carousel principal

Au démarrage, l'écran affiche un **carousel** de 6 sections :

```
◀ PRESET   SYNC   ROUTAGE   MONITOR   SYSTEME   TRANSPORT ▶
```

Tournez l'encodeur pour changer de section. Appuyez sur SELECT pour entrer.

---

## 5. Smart Mirror

Quand le miniMoc est branché à un ordinateur :

- Chaque message reçu sur un port physique est **simultanément transmis** sur le câble USB correspondant → votre DAW peut enregistrer directement.
- Chaque message envoyé vers une sortie physique est aussi **reflété** sur le câble USB correspondant → monitoring complet depuis le DAW.

**Câbles virtuels USB (numérotation interne) :**

| Câble | Correspondance |
|-------|---------------|
| 1–2 | Entrées physiques A, B |
| 3–5 | Entrées USB Host C, D, E |
| 6–11 | Sorties physiques 1–6 |
| 12–14 | Sorties USB Host 7–9 |
| 0 | Éditeur web (SysEx) |

---

## 6. Routage basique (MATRICE)

La **MATRICE** est une grille 5×9 : chaque cellule active ou désactive la connexion entre une entrée et une sortie.

### Via l'écran OLED

1. Section **ROUTAGE** → **MATRICE**
2. L'écran affiche une grille. La ligne = entrée (A–E), la colonne = sortie (1–9).
3. Naviguez avec l'encodeur, appuyez sur SELECT pour activer/désactiver une cellule.

### Via l'éditeur web

1. Onglet **MATRICE**
2. Cliquez sur une cellule pour l'activer (colorée) ou la désactiver.
3. Les changements sont envoyés en temps réel.

### Exemple : relier le synthé en entrée A aux sorties 1 et 2

Activez les cellules [A, 1] et [A, 2] dans la matrice. Toutes les notes et messages du synthé sont maintenant envoyés simultanément aux deux destinations, **sur tous les canaux MIDI**.

> Pour un routage par canal ou avec remapping, utilisez les **Flux** (section 7).

### Presets d'usine

| Preset | Description |
|--------|-------------|
| VIDE | Aucune connexion |
| DIAGONALE | A→1, B→2, C→3, D→4, E→5 |
| MERGE | Toutes les entrées → toutes les sorties |
| DISPATCH | Chaque entrée → sa sortie + une sortie commune |
| TOTAL | Interconnexion maximale |
| SPLIT | Séparation stéréo / double mono |
| USB>TRS | Entrées USB C/D/E → sorties TRS 1–6 |
| TRS>USB | Entrées TRS A/B → sorties USB 7–9 |

---

## 7. Routage avancé (FLUX)

Les **Flux** permettent un routage précis : vous choisissez **quels canaux MIDI** entrent et **sur quel canal** ils ressortent.

Jusqu'à **16 flux simultanés** sont possibles.

### Créer un flux (éditeur web)

1. Onglet **FLUX** → bouton **+ Nouveau flux**
2. **Étape 1 – Entrées**
   - Cochez les ports d'entrée (A, B, C, D, E)
   - Pour chaque port, cliquez sur l'indicateur de canal :
     - **ALL** : tous les canaux MIDI passent
     - **ch×N** : cliquez pour ouvrir la grille et sélectionner les canaux 1–16 individuellement
3. **Étape 2 – Sorties**
   - Cochez les ports de sortie (1–9)
   - Pour chaque port, cliquez sur l'indicateur de canal :
     - **ALL** : le canal d'entrée est conservé
     - **ch×N** : forcer un canal de sortie fixe (remapping)
4. Cliquez **OK**

### Exemple : boîte à rythmes (entrée B, canal 10) → expandeur (sortie 3, canal 1)

- Entrée : B, canal 10 uniquement
- Sortie : 3, canal 1 (remapping)
- Résultat : seules les données MIDI canal 10 de B arrivent sur la sortie 3, converties en canal 1.

### Gérer les flux existants

- Chaque flux affiché montre un résumé : `F1 - A:1 → 1:5`
- Bouton **Éditer** : modifier le flux
- Bouton **✕** : supprimer le flux

> **Important** : les Flux et la Matrice sont **cumulatifs**. Un message peut être routé à la fois par la matrice (tous canaux) et par un flux (canal spécifique). Veillez à ne pas doubler le signal non voulu.

---

## 8. Synchronisation MIDI Clock (SYNC)

Le miniMoc peut prendre **une seule source** comme maître d'horloge et diffuser son Clock, Start, Stop, Continue sur **toutes les sorties actives** (physiques et USB Host), avec un miroir vers le PC (smartmirror) sur les câbles correspondants.

### Pourquoi chaque lettre a deux variantes

Chaque port A-E existe sous **deux formes distinctes** comme source d'horloge :

- **TRS / USB Host** : le signal reçu sur l'entrée physique elle-même (câble TRS pour A/B, appareil USB branché pour C/D/E).
- **USB/PC** : le signal envoyé par votre ordinateur (via le smartmirror) sur le câble USB miroir de ce même port.

Ce sont deux sources potentiellement différentes qui partagent la même lettre. Sans cette distinction, choisir « A » écouterait les deux en même temps — si un vrai clock TRS et le smartmirror envoient tous les deux du Clock sur le port A, les ticks se mélangent et le tempo devient instable. En choisissant explicitement **A (TRS)** ou **A (USB/PC)**, une seule des deux sources est réellement écoutée.

### Les 11 sources disponibles

| Source | Ce qu'elle écoute |
|--------|-------------------|
| **A (TRS)** / **B (TRS)** | Entrée physique TRS |
| **C (USB Host)** / **D (USB Host)** / **E (USB Host)** | Appareil USB branché directement sur le miniMoc |
| **A (USB/PC)** … **E (USB/PC)** | Câble USB miroir correspondant, piloté depuis votre ordinateur (smartmirror) |
| **MINIMOC** | Horloge interne du miniMoc — voir ci-dessous |
| **OFF** | Aucune horloge propagée |

### Configurer le maître

**Via l'écran OLED — section SYNC** :
- Tournez l'encodeur pour faire défiler la liste (11 sources + OFF)
- SELECT pour valider — le choix est sauvegardé immédiatement

**Via l'éditeur web — onglet SYNC** :
- Cliquez sur l'une des cases de la grille
- La source active est marquée d'un `*`
- Le réglage est envoyé et sauvegardé immédiatement

### Ce qui est transmis depuis le maître

- MIDI Clock (F8)
- Start (FA), Stop (FC), Continue (FB)
- Active Sensing (FE)
- System Reset (FF)

Les messages Real-Time des sources non sélectionnées sont **ignorés**, pour éviter les conflits d'horloge. Réglez **OFF** si vous ne souhaitez propager aucun clock.

### Horloge interne (MINIMOC)

Le miniMoc peut être son propre maître d'horloge, sans dépendre d'aucun appareil externe : sélectionnez **MINIMOC** dans la liste SYNC. Le tempo est généré en continu par une horloge matérielle dédiée du Teensy — il reste précis même quand l'écran est très sollicité (VU-mètre, navigation dans les menus). Réglez le tempo et pilotez la lecture depuis le menu **TRANSPORT** (voir ci-dessous) ou depuis l'onglet SYNC de l'éditeur web.

### Menu TRANSPORT

Accessible depuis le carousel principal, cet écran n'affiche que le **BPM en grand** et un indicateur **PLAY / PAUSE / STOP**.

| Geste | Action |
|-------|--------|
| Tourner l'encodeur | Régler le tempo (±1 BPM ; ±5 si vous tournez vite) |
| Clic encodeur | Play / Pause — premier départ = Start, reprise après pause = Continue (la lecture reprend, elle ne redémarre pas) |
| Clic simple sur BACK, en pause | STOP — arrête réellement l'horloge ; le Play suivant redémarrera depuis le début |
| Clic simple sur BACK, en lecture ou déjà stoppé | Retour au menu précédent |
| Maintenir BACK + clic encodeur | RECORD — commande un enregistreur/DAW externe (MMC) |
| Maintenir BACK + tourner à droite | AVANCE (MMC Fast Forward) |
| Maintenir BACK + tourner à gauche | REWIND (MMC Rewind) |

> Record/Avance/Rewind utilisent le protocole **MMC** (MIDI Machine Control), pensé pour piloter un enregistreur ou un DAW externe — c'est un protocole différent du Play/Pause/Stop "temps réel" qui pilote les appareils synchronisés sur le clock. Les deux peuvent être utilisés indépendamment.

Depuis l'éditeur web, l'onglet SYNC propose les mêmes contrôles : deux boutons **−/+** pour le tempo (appui maintenu = défilement continu) et une rangée de boutons ▶/⏸ ⏹ ⏺ ⏪ ⏩.

### Visualiser le BPM en temps réel

Depuis le carousel, entrez dans **MONITOR** puis tournez l'encodeur vers la droite pour passer à l'écran **BPM**.

- La source maître est rappelée en haut de l'écran.
- Le BPM du maître est affiché en grands chiffres, arrondi à l'entier le plus proche.
- La LED embarquée du miniMoc clignote à chaque temps (toutes les 24 ticks de Clock).
- Si aucun signal n'est reçu depuis plus de 2 secondes, l'écran affiche **NO CLOCK**.
- Tournez l'encodeur vers la gauche pour revenir à l'écran VU-mètre.

La fréquence d'actualisation de cette page se règle dans **SYSTEME → BPM REFRESH** (de 250ms à 5s, ou **MANUEL** pour ne jamais rafraîchir automatiquement). Quel que soit le réglage, un **clic sur l'encodeur** pendant que cette page est affichée force toujours une actualisation immédiate — pratique en mode MANUEL, ou pour vérifier le tempo à l'instant précis où vous le voulez.

> **Important** : l'écran MONITOR se rafraîchit en permanence (chaque rafraîchissement de l'affichage OLED bloque le bus I²C ~20 ms). Pour éviter tout jitter sur les sorties MIDI à forte densité de données, **revenez à l'écran d'accueil (la feuille du carousel) dès que vous n'avez plus besoin du moniteur**. Ceci ne concerne pas l'horloge MINIMOC : générée par un timer matériel dédié, elle reste précise même écran actif.

---

## 9. Gestion des presets

Le miniMoc dispose de **32 emplacements utilisateur** et **8 presets d'usine**.

Chaque preset contient :
- La matrice de routage basique
- Tous les flux définis
- Le réglage SYNC

### Charger un preset

**Via l'éditeur web** :
1. Cliquez **Charger** dans la barre du haut
2. Sélectionnez un preset d'usine ou un numéro 1–32
3. Cliquez **Charger preset N**

**Via l'écran OLED — section PRESET** :
1. Tournez l'encodeur pour sélectionner le numéro
2. SELECT pour charger

### Sauvegarder un preset

**Via l'éditeur web** :
1. Configurez votre routage
2. Cliquez **Sauver** dans la barre du haut
3. Choisissez un emplacement (1–32)
4. Cliquez **Sauver sur preset N**

**Via l'écran OLED — section PRESET** :
1. Allez sur **SAUVER**
2. Sélectionnez le numéro de destination
3. SELECT pour confirmer

### Indicateur de modification

Dans l'éditeur web, un astérisque `*` apparaît à côté du numéro de preset si des modifications non sauvegardées sont en cours.

### Persistance

- Les presets sont stockés sur la **carte SD** (`/PRESETS/PRST01.DAT` à `PRST32.DAT`).
- Le **preset actif** est mémorisé en EEPROM et rechargé automatiquement au démarrage.

---

## 10. Appareils USB (HOST CONFIG)

Le miniMoc peut connecter jusqu'à **3 appareils USB MIDI simultanément** en entrée (C, D, E) et 3 en sortie (7, 8, 9), via un hub USB interne.

### Mode AUTO (défaut)

- Les appareils sont assignés dans l'ordre de connexion.
- Premier branché = slot C/7, deuxième = D/8, troisième = E/9.
- À chaque déconnexion/reconnexion, l'ordre peut changer.
- Idéal pour des tests rapides.

### Mode MANUEL

- Chaque appareil est identifié par son VID:PID (identifiant fabricant/produit).
- L'appareil retrouve toujours le même port logique, quel que soit l'ordre de branchement.
- Idéal pour un live setup stable.

### Passer en mode MANUEL

**Via l'éditeur web — onglet HOST CONFIG** :
1. Cliquez le bouton **MANUEL**
2. Les listes déroulantes deviennent actives
3. Assignez chaque slot physique (D1–D6) à un port logique (C, D, E, 7, 8, 9)

**Via l'écran OLED — SYSTEME → HOST CONFIG** :
- Sélectionnez AUTO ou MANUEL
- Assignez les slots si en mode MANUEL

### En cas de problème de reconnaissance

Depuis **SYSTEME** :
- **USB LINK** : force une nouvelle détection des appareils (2,5 secondes de polling)
- **USB RESET** : redémarre complètement le Teensy

---

## 11. Enregistrement MIDI sur carte SD

> ⚠️ **Reporté après la V1.** Le moteur d'enregistrement continu (ring buffer → écriture
> SD, format `.DAT`) existe dans le firmware mais est **désactivé par défaut**
> (`SD_RECORDER_ENABLED`) : il n'y a pas encore de commande pour démarrer/arrêter un
> enregistrement à la volée, ni de fermeture propre du fichier en cours d'usage. En
> conséquence, l'onglet **DAT → MIDI** de l'éditeur web est également masqué tant que
> cette fonctionnalité n'est pas retravaillée. Le reste de cette section décrit le
> fonctionnement prévu, pour référence.

Une fois réactivé, le miniMoc enregistre automatiquement tous les messages MIDI routés sur la carte SD, dans `/REC/REC001.DAT` à `/REC099.DAT`.

- L'enregistrement est en **anneau** (ring buffer) : REC099 plein → REC001 écrasé
- Format propriétaire `.DAT` (10 octets/événement : horodatage, source, destination, type, canal, données)
- La conversion en fichier MIDI standard se fera via un onglet **DAT → MIDI** de l'éditeur web, réactivé en même temps que l'enregistrement.

---

## 12. Éditeur web (weblink)

L'éditeur web est une application accessible depuis un **navigateur Chrome ou Edge**.

### Prérequis

- Navigateur : **Google Chrome** ou **Microsoft Edge** (Chromium)
- Connexion : USB entre le miniMoc et l'ordinateur
- Protocole : le fichier doit être ouvert en HTTPS ou en `localhost` (Web MIDI API)

### Ouvrir l'éditeur

Ouvrez le fichier `weblink/index.html` depuis votre navigateur, ou accédez à l'URL hébergée si disponible.

### Se connecter

1. Branchez le miniMoc en USB
2. Dans la barre du haut, cliquez sur le menu déroulant **— Port MIDI —**
3. Sélectionnez **miniMoc** (ou le nom qui lui correspond)
4. Le point de statut devient **vert** ("Connecté")

> Un badge affiche la version du firmware connecté à côté du logo. Si ce firmware est trop ancien pour cette version de l'éditeur web, un bandeau d'avertissement apparaît avec un lien vers une version compatible du configurateur — voir [Dépannage](#15-dépannage).

### Onglets disponibles

| Onglet | Fonction |
|--------|----------|
| **SYNC** | Choisir la source d'horloge maître (11 sources + OFF), régler le tempo de l'horloge interne (boutons −/+, appui maintenu = défilement continu) et piloter le transport (▶/⏸ ⏹ ⏺ ⏪ ⏩) |
| **MATRICE** | Routage basique (grille 5×9) |
| **FLUX** | Routage avancé (filtrage, remapping de canaux, transformateurs) |
| **GENERATEURS** | Configuration des générateurs MIDI internes (LFO, séquenceur euclidien, motifs) |
| **HOST CONFIG** | Gestion des appareils USB |
| **FIRMWARE** | Mise à jour du firmware |

---

## 13. Mise à jour du firmware

### Via l'éditeur web (recommandé)

1. Dans l'éditeur web, onglet **FIRMWARE**
2. Choisissez la source du fichier :
   - **Firmwares du serveur** *(onglet par défaut)* : sélectionnez une version dans la liste — le fichier `.hex` est téléchargé automatiquement depuis le serveur
   - **Fichier local** : compilez d'abord dans l'Arduino IDE (**Croquis → Exporter le binaire compilé**), puis glissez le `.hex` dans la zone ou cliquez pour le sélectionner
3. Une fois le fichier chargé, cliquez **⬆ Mettre à jour le firmware**
4. Sélectionnez le port série du miniMoc dans le sélecteur du navigateur
5. Attendez la barre de progression et le message de confirmation
6. Le miniMoc redémarre automatiquement

### Première installation (Arduino IDE)

Si le miniMoc n'a jamais de firmware, ou en cas de problème grave :

1. **Installer Arduino IDE** (dernière version)
2. **Installer Teensyduino** : [pjrc.com/teensy/teensyduino.html](https://www.pjrc.com/teensy/teensyduino.html)
3. **Installer les bibliothèques** (Croquis → Gérer les bibliothèques) :
   - U8g2
   - Arduino MIDI Library (v5+)
   - MTP_Teensy
   - SdFat
   - Encoder
   - Bounce2
4. **Configurer Arduino IDE** :
   - Outils → Carte → **Teensy 4.1**
   - Outils → Type USB → **Serial + MIDI**
   - Outils → Vitesse CPU → **600 MHz**
   - Outils → Optimisation → **Faster**
5. Ouvrir `firmware/firmware.ino`
6. **Croquis → Téléverser**

---

## 14. Référence des menus OLED

```
CAROUSEL PRINCIPAL
├── PRESET
│   ├── Sélectionner preset actif (1–32)
│   ├── Charger depuis SD
│   └── Sauver sur SD
│
├── ROUTAGE
│   ├── MATRICE   ← grille 5×9
│   └── FLUX      ← liste des flux / créer / éditer / supprimer
│
├── SYNC   ← liste défilante, 11 sources + OFF
│   ├── A (TRS) / B (TRS)
│   ├── C (USB Host) / D (USB Host) / E (USB Host)
│   ├── A (USB/PC) … E (USB/PC)
│   ├── MINIMOC   ← horloge interne
│   └── OFF
│
├── MONITOR
│   ├── Page 1 — VU-mètres temps réel (5 entrées + 9 sorties)
│   └── Page 2 — BPM du maître SYNC en grand (tournez l'encodeur pour naviguer)
│
├── SYSTEME
│   ├── INFO           ← numéro preset, stats SD, noms appareils USB
│   ├── HOST CONFIG    ← mode AUTO/MANUEL, assignation des slots
│   ├── USB LINK       ← forcer détection appareils (2,5 s)
│   ├── USB RESET      ← redémarrage complet Teensy
│   ├── CONTRASTE      ← luminosité OLED (16–255)
│   └── BPM REFRESH    ← fréquence d'actualisation de la page BPM du MONITOR (250ms–5s ou MANUEL)
│
└── TRANSPORT   ← tempo + transport de l'horloge interne MINIMOC
    ├── Tourner l'encodeur     : ±1 BPM (±5 en rotation rapide)
    ├── Clic encodeur          : Play / Pause
    ├── Clic simple BACK       : STOP (si en pause) ou retour au menu précédent
    └── BACK maintenu + encodeur : Record / Avance / Rewind (MMC)
```

---

## 15. Dépannage

| Problème | Cause possible | Solution |
|----------|---------------|----------|
| L'éditeur web ne se connecte pas | Mauvais navigateur | Utilisez Chrome ou Edge uniquement |
| L'éditeur web ne se connecte pas | USB non reconnu | Essayez un autre port USB ou câble |
| Pas de son en sortie | Matrice vide | Vérifiez onglet MATRICE — activez les cellules |
| Doublement du signal MIDI | Flux + Matrice actifs simultanément | Désactivez la cellule Matrice correspondant au Flux |
| Clock irrégulier | Plusieurs sources de clock actives sur la même lettre | Choisissez explicitement **TRS/Host** ou **USB/PC** dans SYNC — les deux ne sont plus mélangées automatiquement |
| Le tempo affiché ne bouge pas quand je change le BPM | Réglage BPM REFRESH sur un intervalle long ou MANUEL | Cliquez l'encodeur sur la page BPM du MONITOR pour forcer une actualisation immédiate |
| L'éditeur web affiche un bandeau "firmware plus ancien" | Le firmware connecté ne supporte pas certaines fonctions de cette version de l'éditeur | Utilisez le lien du bandeau (ou le lien "configurateur classique" en bas de la barre latérale) vers la version compatible |
| Appareils USB non reconnus | Ordre de connexion | SYSTEME → USB LINK, ou USB RESET |
| Appareils USB changent de port | Mode AUTO actif | HOST CONFIG → passer en mode MANUEL |
| Carte SD non détectée | Carte absente ou non formatée | Insérez une carte SD formatée FAT32 |
| Firmware ne se met pas à jour | Fichier `.hex` incorrect | Recompilez dans Arduino IDE avec les bons paramètres |
| Écran OLED très sombre | Contraste bas | SYSTEME → CONTRASTE → augmentez la valeur |

---

*miniMoc — firmware PolyForm NC 1.0 / hardware CC BY-NC 4.0*
