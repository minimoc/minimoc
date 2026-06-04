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

Sélection d'un **unique port maître** pour Clock/Start/Stop/Continue/SystemReset :

| Mode | Comportement |
|------|-------------|
| **OFF** | Tous les messages real-time sont filtrés (rien ne passe) |
| **A/B/C/D/E** | Seul ce port transmet les RT vers **toutes les sorties** (1–9 incl. USB), les autres sont filtrés |

- Persisté en EEPROM (adresse 34)
- Les USB Host inputs (C/D/E) supportent la réception d'horloge via callbacks `setHandleRealTimeSystem`

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

### 7.2 Carousel principal (5 items)
```
PRESET → SYNC → ROUTAGE → MONITOR → SYSTEM
```

### 7.3 Menu PRESET
- Affichage du numéro de preset courant (indicateur SD si sauvegardé)
- Actions : changer de numéro / Sauvegarder / Charger

### 7.4 Menu SYNC
- Grille 2×3 : A / B / C / D / E / OFF
- Sélection active affichée en inversé (fond blanc / texte noir)
- VALID = sélectionner + sauvegarder EEPROM

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

### 7.7 Menu SYSTEM
```
SYSTEM
├── INFO         → stats SD + liste devices USB physiques (D1..D6)
├── HOST CONFIG  → assignation ports logiques / Mode AUTO-MANUEL / LOCK VID:PID
├── USB LINK     → polling USB 2,5s pour forcer l'énumération (sans reboot)
└── USB RESET    → reboot Cortex-M7 complet (équivalent power-cycle)
```

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
| 34 | Maître SYNC (0=A … 4=E, 0xFF=OFF) |

---

## 10. Stockage SD

| Chemin | Contenu |
|--------|---------|
| `/PRESETS/PRSTnn.DAT` | Presets MIDI (format v3) |
| `/REC/RECnnn.DAT` | Enregistrements MIDI (ring buffer) |