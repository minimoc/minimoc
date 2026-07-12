# miniMoc — Spécifications Fonctionnelles

## 1. Vue d'ensemble

Le **miniMoc** est un routeur MIDI hardware intelligent combinant :
- Un firmware embarqué (C++/Arduino) sur **Teensy 4.1**
- Une interface homme-machine sur **OLED 128×64 (SSD1306 I2C)**
- Un **encodeur rotatif** (navigation + sélection) et un bouton BACK

---

## 2. Ports MIDI

### Entrées (5 ports)
| Port | Type | Description |
|------|------|-------------|
| A | DIN MIDI (Serial2) | Entrée physique TRS TYPE A |
| B | DIN MIDI (Serial1) | Entrée physique TRS TYPE A |
| C | USB Host | 1er device USB branché |
| D | USB Host | 2e device USB branché |
| E | USB Host | 3e device USB branché |

### Sorties (9 ports)
| Port | Type | Description |
|------|------|-------------|
| 1–6 | DIN MIDI (Serial3/6/7/8) | Sorties physiques TRS |
| 7–9 | USB Host | Sorties vers devices USB |

### USB Device (vers DAW)
- Port 0 : SysEx/éditeur
- Ports 1–2 : miroir brut des entrées A/B
- Ports 3–12 : miroir routé des sorties 1–9 + miroir brut des entrées C/D/E
- **Smart Mirror** : tout signal routé est simultanément mirroré vers le DAW pour enregistrement

### USB Host (6 slots physiques)
- 6 objets `MIDIDevice_BigBuffer` (MIDIUSB1–6), assignation FIFO
- 3 instances `USBHub` pour robustesse hub multi-TT / reconnexion
- Pool : 12 pipes + 48 transfers extra

---

## 3. Routage MIDI — Architecture Hybride

Le routage combine deux couches cumulatives calculées dans `route_matrix[5][16][9]` :

### 3.1 Couche BASIC (matrice 5×9)
- Grille booléenne Port d'entrée → Port de sortie
- Quand un nœud est actif : **passthrough canal** (ch1→ch1, ch2→ch2, etc.)
- Stockée dans `basic_matrix[5]` (bitmask uint16_t par port d'entrée)
- Navigation : encodeur (1 case/tick), VALID = toggle

### 3.2 Couche FLUX (règles avancées cumulatives)
Chaque **flux** définit :
- **Entrées** : liste de (port, canal_mask) — ex. {A, ch1} + {B, ch2}
- **Sorties** : liste de (port, canal_mask) — ex. {sortie1, ch5} + {sortie4, ch2}
- Si canal_mask = 0 → tous les canaux (entrée) ou passthrough (sortie)
- Multi-sélection indépendante pour entrées ET sorties
- MAX 16 flux simultanés

**Cumul** : route_matrix = BASIC + tous FLUX actifs. Les bits sont OR-és : un signal peut arriver sur plusieurs destinations via BASIC et FLUX simultanément.

### 3.3 Filtrage canal 10
Supprimé — le canal 10 est traité comme n'importe quel autre canal.

---

## 4. Synchronisation MIDI (SYNC)

Sélection d'une **unique source maître** (`sync_master`, `uint8_t`) pour Clock/Start/Stop/Continue/SystemReset, diffusée via `_sync_forward()` (`_midi.h`) vers **toutes les sorties** (1–6 physiques, 7–9 USB Host) et leur miroir PC (smartmirror, `SYNC_MIRROR_CABLES[]`).

### 4.1 Encodage `sync_master`

Chaque lettre A-E a deux sources physiques distinctes qui déclenchaient toutes les deux le même maître avant cette distinction : l'entrée TRS/USB Host physique d'un côté, le câble USB miroir venant du PC (smartmirror) de l'autre. `SYNC_LABELS[]` (`logic.h`) :

| Valeur | Source | Valeur | Source |
|--------|--------|--------|--------|
| 0 | A (TRS) | 5 | A (USB/PC) |
| 1 | B (TRS) | 6 | B (USB/PC) |
| 2 | C (USB Host) | 7 | C (USB/PC) |
| 3 | D (USB Host) | 8 | D (USB/PC) |
| 4 | E (USB Host) | 9 | E (USB/PC) |
| 10 | MINIMOC (horloge interne) | 0xFF (RAM) / 0x7F (EEPROM, SysEx) | OFF |

- Persisté en EEPROM (adresse 34, 1 octet, valeurs 0-10 ou 0x7F=OFF)
- Les USB Host inputs (C/D/E) supportent la réception d'horloge via callbacks `setHandleRealTimeSystem` (`_usb_rt()`, valeurs 2-4 uniquement — inchangé par la distinction TRS/USB ci-dessus)
- Rétrocompatibilité : les valeurs 0-4 et 0xFF gardent exactement le sens qu'elles avaient avant l'introduction des variantes USB/PC (5-9) et MINIMOC (10)

### 4.2 Horloge interne (MINIMOC)

Quand `sync_master == SYNC_MASTER_INTERNAL` (10), le miniMoc génère lui-même le Clock via `internal_clock_isr()` (`_midi.h`), déclenché par un `IntervalTimer` matériel — indépendant de `loop()` et de ses appels bloquants (`u8g2.sendBuffer()` ~8ms, `sd_tick()`/`MTP.loop()`, `myusb.Task()`).

- **Sorties DIN (MIDI1-6)** : envoyées directement depuis l'ISR. Sûr par construction : si le buffer TX UART est plein, l'IRQ UART (priorité 64) préempte l'`IntervalTimer` (priorité par défaut 128, donc moins prioritaire) pour le drainer — mécanisme prévu par le core Teensy pour ce cas précis (`HardwareSerial.cpp::write9bit()`).
- **Sorties USB Host (7-9) et miroir PC** : jamais appelées depuis l'ISR — `MIDIDevice_BigBuffer::sendRealTime()` (USBHost_t36) et `usbMIDI` ne sont pas garantis réentrants (`write_packed()` peut boucler un temps non borné en cas de buffers pleins). L'ISR pose un flag (`internal_clock_usb_pending`), vidé en best-effort par `internal_clock_flush_usb()` appelée sans condition dans `loop()`.
- Tempo : `internal_clock_bpm` (`uint16_t`, 20-300, défaut 100), persisté en EEPROM (37-38). `internal_clock_apply()` démarre/arrête/reconfigure le timer (`period_us = 60000000UL / (24UL * internal_clock_bpm)`) — appelée après toute affectation de `sync_master` ou de `internal_clock_bpm`.
- Pas de Start/Stop/Continue automatique généré par le Clock lui-même — géré séparément par le menu TRANSPORT (4.3).

### 4.3 Transport (Play/Pause/Stop/MMC)

`transport_submenu.h` expose des actions réutilisées à la fois par les gestes physiques (encodeur + BACK) et par la commande SysEx `0x11` (weblink) :

| Fonction | Effet |
|----------|-------|
| `transport_do_play_pause()` | Bascule Play/Pause. Play : `Start` (premier départ) ou `Continue` (reprise après pause) selon `transport_resumable`. Pause : `Stop` temps réel, `transport_resumable=true` |
| `transport_do_stop()` | `Stop` temps réel + MMC Stop, `transport_resumable=false` (le Play suivant redémarre avec `Start`, pas `Continue`) |
| `transport_do_record()` | MMC Record Strobe |
| `transport_do_fastforward()` | MMC Fast Forward |
| `transport_do_rewind()` | MMC Rewind |

État partagé : `transport_playing` (bool), `transport_resumable` (bool) — lus par `transport_draw()` pour l'affichage PLAY/PAUSE/STOP.

**MMC (MIDI Machine Control)** : SysEx `F0 7F 7F 06 <commande> F7` (device-id 0x7F = broadcast), diffusé vers les mêmes 9 sorties + miroir PC via `send_mmc()` (`_midi.h`). Commandes utilisées : Stop 0x01, Fast Forward 0x04, Rewind 0x05, Record Strobe 0x06. Protocole indépendant du Start/Continue/Stop temps réel (`_sync_forward()`).

**Gestes physiques** (écran TRANSPORT) :
- Encodeur (BACK relâché) : ±1 BPM, ±5 si deux crans s'enchaînent en moins de `TRANSPORT_BPM_ACCEL_MS` (180ms)
- Clic encodeur (BACK relâché) : `transport_do_play_pause()`
- Clic simple BACK : `transport_do_stop()` si en pause (reste sur l'écran), sinon retour au menu précédent
- BACK tenu + clic encodeur / tourner à droite / tourner à gauche : `transport_do_record()` / `transport_do_fastforward()` / `transport_do_rewind()`
- Le long-appui global BACK→écran logo (`firmware.ino::loop()`) est désactivé tant que `ui_screen == UI_TRANSPORT`, pour ne pas interrompre les gestes chordés ci-dessus
- `btn_back` distingue un **front** (`backButton.fell()`, un seul événement par appui — utilisé par tous les autres écrans pour "remonter d'un niveau") d'un **niveau continu** (`btn_back_held`, utilisé uniquement par TRANSPORT pour les gestes chordés)

### 4.4 Protocole SysEx éditeur (PCEditor.h)

Toutes les commandes `F0 7D <cmd> ... F7` (`MANUFACTURER_ID = 0x7D`) :

| Cmd | Sens | Description |
|-----|------|-------------|
| 0x01 | PC→device | Update routing direct (legacy) |
| 0x02 | PC→device | Request full dump (`sendFullDumpToPC()`) |
| 0x03 | device→PC | Version firmware (ASCII, ex. "v0.3.0") — envoyée en tête du dump complet et à chaque QUICK POLL |
| 0x04 | bidirectionnel | BPM horloge interne (14 bits, 2×7bit) |
| 0x05 / 0x06 | PC→device | Charger / sauver preset N |
| 0x07 | device→PC | Preset actif |
| 0x08 | bidirectionnel | Ping / Pong (heartbeat) |
| 0x09 | bidirectionnel | Cellule matrice BASIC |
| 0x0A / 0x0B | bidirectionnel | Flux (données / count) |
| 0x0C | bidirectionnel | SYNC master |
| 0x0D | bidirectionnel | HOST CONFIG |
| 0x0E | PC→device | Quick poll (déclenche l'envoi groupé de 0x03/0x07/0x0C/0x04/0x09/0x0B/0x0A/0x0D/0x0F) |
| 0x0F | device→PC | Nom device USB (par slot) |
| 0x10 | PC→device | Reboot HalfKay bootloader |
| 0x11 | PC→device | Action transport — 0=Play/Pause 1=Stop 2=Record 3=Fast Forward 4=Rewind (voir 4.3) |

---

## 5. USB Host — Configuration (HOST CONFIG)

### 5.1 Assignation port logique ↔ slot physique
Table `port_slot[6]` : pour chaque port logique (IN C/D/E + OUT 7/8/9), quel slot physique (MIDIUSB1-6) le gère.

Défaut : slot 0 → C+7, slot 1 → D+8, slot 2 → E+9.

### 5.2 Mode AUTO
- Assignation chronologique FIFO (premier device branché = premier slot)
- Pas de persistance des assignations
- Toute reconnexion repart dans l'ordre d'arrivée

### 5.3 Mode MANUEL
- L'utilisateur assigne manuellement chaque port logique à un slot physique
- Capture des empreintes **VID:PID** de chaque port au moment de l'activation
- À la reconnexion : scan par VID:PID → le device retrouve son port logique quel que soit l'ordre de branchement
- Persisté en EEPROM (adresse 2, format v3)

---

## 6. Presets

- Stockage sur **carte SD** (SDIO), répertoire `/PRESETS/PRSTnn.DAT`
- Maximum **32 presets**
- Format v3 : `route_matrix` + `basic_matrix` + flux_list (FLUX)
- Migration automatique des presets v1 (legacy route_matrix → basic_matrix déduite)
- Sauvegarde/chargement via menu PRESET
- Preset actif mémorisé en EEPROM (adresse 0)

---

## 7. Interface Homme-Machine

### 7.1 Contrôles physiques
- **Encodeur rotatif** : navigation dans les listes/grilles
- **Bouton VALID** (encodeur click) : sélectionner, valider, entrer dans un menu
- **Bouton BACK** (bouton séparé) : retour arrière dans la navigation
- **Appui long BACK** (1s) : basculer vers/depuis l'écran logo (mode live)
- **Depuis le logo** : tout clic ou rotation → retour au carousel

### 7.2 Carousel principal (6 items)
```
PRESET → SYNC → ROUTAGE → MONITOR → SYSTEM → TRANSPORT
```
Ordre d'enregistrement : `firmware.ino::setup()` (`carousel_register()`).

### 7.3 Menu PRESET
- Affichage du numéro de preset courant (indicateur SD si sauvegardé)
- Actions : changer de numéro / Sauvegarder / Charger

### 7.4 Menu SYNC
- Liste défilante (`sync_submenu.h`) : 11 sources + OFF (voir §4.1), curseur en encart inversé, flèches `^`/`v` quand du contenu est masqué
- La source active est marquée d'un `*`, indépendamment de la position du curseur
- VALID = sélectionner + sauvegarder EEPROM + `internal_clock_apply()` (démarre/arrête l'horloge interne selon le nouveau choix)

### 7.5 Menu ROUTAGE
```
ROUTAGE
├── MATRICE  → grille 5×9 interactive (BASIC)
└── FLUX     → liste des flux avancés
     ├── [F1 - A:1 → 1:5]  (clic = Modifier / Supprimer)
     ├── [F2 - B → 1 4]
     └── [+] Nouveau flux
           Step 1 : FLUX → ENTREES (ports A-E, long press = canaux)
           Step 2 : FLUX → SORTIES (ports 1-9, long press = canaux)
           Picker canaux : grille 4×4 (ch1-16), sélection multi indépendante
```

### 7.6 Menu MONITOR
- VU-mètre temps réel : 5 entrées (A/B/C/D/E) + 9 sorties (1-9)
- Actualisation permanente (animation fondu 500ms)
- Les barres d'entrée s'allument dès réception, indépendamment du routage
- Page 2 (BPM) : tempo du maître SYNC en grand ; fréquence de rafraîchissement pilotée par le réglage **BPM REFRESH** (§7.7) ; un clic sur l'encodeur force toujours une actualisation immédiate quel que soit ce réglage ; la LED embarquée (`LED_BUILTIN`) clignote à chaque temps (`bpm_push_clock()`, toutes les 24 ticks), indépendamment de l'affichage

### 7.7 Menu SYSTEM
```
SYSTEM
├── INFO         → stats SD + liste devices USB physiques (D1..D6)
├── HOST CONFIG  → assignation ports logiques / Mode AUTO-MANUEL / LOCK VID:PID
├── USB LINK     → polling USB 2,5s pour forcer l'énumération (sans reboot)
├── USB RESET    → reboot Cortex-M7 complet (équivalent power-cycle)
├── CONTRASTE    → luminosité OLED (16–255)
└── BPM REFRESH  → fréquence d'actualisation de la page BPM du MONITOR
                   (250/500/1000/2000/5000ms ou MANUEL — cf. BPM_REFRESH_OPTIONS, logic.h)
```
Liste défilante (même pattern que SYNC), 6 items.

### 7.8 Menu TRANSPORT
Écran minimal : BPM en grand (`u8g2_font_logisoso42_tn`) + indicateur PLAY/PAUSE/STOP en dessous (`transport_submenu.h`). Pilote le tempo et le transport de l'horloge interne MINIMOC (§4.2, §4.3) — voir §4.3 pour le détail des gestes. `UI_TRANSPORT` est ajouté à `need_anim` dans `controls.h::ui_tick()` : l'écran tourne en continu (pas seulement sur activité détectée), nécessaire pour observer de façon fiable le front de relâchement de BACK.

---

## 8. Performances et stabilité

- **Latence MIDI** : presque nulle — `sendBuffer()` OLED suspendu si pas d'input utilisateur
- **USB Host** : `myusb.Task()` appelé 2× par itération de loop pour réduire le gap pendant SD/MTP
- **Démarrage** : logo affiché, preset et config restaurés depuis SD/EEPROM ; USB LINK disponible si devices non reconnus
- **Récupération** : USB RESET depuis SYSTEM → reboot propre, re-énumération complète

---

## 9. Stockage EEPROM (Teensy 4.1 — 1080 octets)

| Adresse | Contenu |
|---------|---------|
| 0 | Preset actif (index 1-indexed) |
| 1 | Compteur de session SD |
| 2–33 | Config USB Host (magic + mode + port_slot + VID:PID) |
| 34 | Maître SYNC (0-10, cf. `SYNC_LABELS` §4.1 ; 0x7F=OFF) |
| 35 | Contraste OLED (`screen_contrast`, 1 octet) |
| 36 | Intervalle de rafraîchissement BPM REFRESH (index dans `BPM_REFRESH_OPTIONS`, 1 octet) |
| 37–38 | BPM de l'horloge interne MINIMOC (`internal_clock_bpm`, `uint16_t`) |

---

## 10. Stockage SD

| Chemin | Contenu |
|--------|---------|
| `/PRESETS/PRSTnn.DAT` | Presets MIDI (format v3) |
| `/REC/RECnnn.DAT` | Enregistrements MIDI (ring buffer) |