# Mémo — Réduction jitter menu Monitor

## Problème

Le menu Monitor (VU-mètre + BPM) introduisait du jitter et de l'instabilité dans le traitement MIDI.

**Cause racine :** l'appel `u8g2.sendBuffer()` (transfert I2C vers l'écran OLED) bloque le Teensy environ **20 ms** à chaque exécution. Avec un rafraîchissement toutes les 50 ms, le processeur était occupé par l'affichage **40 % du temps**, empêchant la lecture des messages MIDI USB en temps voulu.

---

## Modifications apportées

### 1. Throttle du rafraîchissement : 50 ms → 300 ms
**Fichier :** `firmware/monitor_submenu.h` — fonction `mon_handle_input()`

Un timestamp `mon_last_draw_ms` conditionne l'appel aux fonctions de rendu (`mon_draw()` / `mon_draw_bpm()`) : elles ne sont exécutées que si 300 ms se sont écoulées depuis le dernier dessin. Un changement de page via l'encodeur force un redraw immédiat (l'affichage répond instantanément à la navigation).

### 2. Vitesse I2C : 400 kHz → 1 MHz
**Fichier :** `firmware/firmware.ino` — fonction `setup()`

`Wire.setClock(400000)` remplacé par `Wire.setClock(1000000)`.
Le Teensy 4.1 supporte le mode Fast-mode Plus (1 MHz). À cette vitesse, chaque `sendBuffer()` passe de ~20 ms à ~8 ms.

---

## Effet combiné

| Situation | Durée sendBuffer | Fréquence | Temps bloqué / seconde |
|---|---|---|---|
| Avant | ~20 ms | toutes les 50 ms | ~400 ms/s |
| Après | ~8 ms | toutes les 300 ms | ~27 ms/s |

Réduction d'environ **15× du temps de blocage I2C** pendant l'affichage Monitor.

---

## Vérifications après flashage

### 1. Stabilité de l'écran OLED
- Naviguer dans le menu Monitor : l'affichage doit être propre, sans artefacts, lignes parasites ou freeze.
- Rester sur la page VU-mètre quelques secondes, puis passer à la page BPM.
- Si l'écran est instable → voir **Rollback** ci-dessous.

### 2. Rafraîchissement VU-mètre et BPM
- Le VU-mètre doit se mettre à jour environ 3 fois par seconde (toutes les ~300 ms). L'animation est moins fluide qu'avant mais reste lisible.
- Le BPM doit afficher la valeur correcte et se rafraîchir au même rythme.
- Changer de page avec l'encodeur : le nouveau contenu doit apparaître immédiatement (sans attendre les 300 ms).

### 3. Réduction du jitter MIDI
- Brancher un séquenceur ou une boîte à rythme envoyant du MIDI Clock.
- Comparer le jitter horloge mesuré dans le DAW (ou avec un outil de monitoring MIDI) avant/après.
- En mode Monitor actif, le jitter doit être significativement plus faible qu'avant.

### 4. Fonctionnement normal hors Monitor
- Vérifier que les autres menus (Preset, Sync, Routage, System) ne sont pas affectés.
- Le carousel doit défiler normalement.

---

## Rollback si l'OLED est instable à 1 MHz

Dans `firmware/firmware.ino`, ligne `Wire.setClock(...)` :

| Valeur | Mode | À essayer si… |
|---|---|---|
| `1000000` | Fast-mode Plus | **valeur actuelle** |
| `800000` | intermédiaire | artefacts légers |
| `400000` | Fast (valeur d'origine) | freeze ou corruption d'image |

Le throttle 300 ms reste bénéfique quelle que soit la vitesse I2C choisie.
