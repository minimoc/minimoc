# miniMoc — Cahier de tests unitaires

> Basé sur `docs/SPECS.md` et `docs/USERGUIDE.md`. À dérouler avec la configuration matérielle ci-dessous.
> Pour chaque test : dérouler la procédure, cocher OK/KO, noter les écarts observés en commentaire.

---

## 0. Configuration matérielle de test

### Entrées

| Port | Type | Appareil |
|------|------|----------|
| A | TRS | Novation SL MK2 |
| B | TRS | Electribe 2 |
| C | USB Host | Arturia MiniLab |
| D | USB Host | *(libre — réservé pour tests de branchement à chaud)* |
| E | USB Host | *(libre — réservé pour tests de branchement à chaud)* |

### Sorties

| Port | Type | Appareil |
|------|------|----------|
| 1 | TRS | Dreadbox Typhon |
| 2 | TRS | Minifreak |
| 3 | TRS | Electribe 2 |
| 4 | TRS | reface CP |
| 5 | TRS | Keytron SD1000 |
| 6 | TRS | Novation SL MK2 |
| 7 | USB Host | OP-1 |
| 8 | USB Host | *(libre)* |
| 9 | USB Host | *(libre)* |

### Poste informatique

| Rôle | Machine |
|------|---------|
| Smart Mirror / enregistrement DAW | PC + Ableton Live (branché en USB sur le miniMoc) |
| Éditeur web (weblink) | PC + navigateur Chrome ou Edge, fichier `weblink/index.html` (ou URL hébergée) |

> **Point d'attention config** : l'Electribe 2 est câblée à la fois en entrée B et en sortie 3, et le Novation SL MK2 à la fois en entrée A et en sortie 6. Vérifier lors des tests de bouclage qu'un message n'est pas indéfiniment renvoyé vers sa propre source (cf. §2 test MATRICE-05, §3 test FLUX-06).
> **Point d'attention USB Host** : MiniLab (entrée C) et OP-1 (sortie 7) sont deux appareils USB distincts. En mode AUTO, l'assignation des slots est FIFO — l'ordre de branchement détermine qui devient C/D/E. Ce point est directement testé en §7.

---

## 1. Connectique & détection de base

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| CONN-01 | Démarrer le miniMoc sans rien de branché | Écran logo puis carousel s'affiche normalement, aucun blocage | |
| CONN-02 | Brancher Novation SL MK2 sur A, Electribe 2 sur B (câbles TRS) | MONITOR (page VU-mètres) affiche une activité sur A et B dès qu'une note est jouée sur chaque appareil | |
| CONN-03 | Brancher MiniLab sur un port USB Host du miniMoc | SYSTEM → INFO liste MiniLab parmi les devices USB (D1..D6) ; MONITOR affiche l'activité sur le port logique C (mode AUTO, 1er device = C) | |
| CONN-04 | Brancher les 6 sorties TRS (Typhon=1, Minifreak=2, Electribe2=3, reface CP=4, Keytron=5, SL MK2=6) et OP-1 sur une sortie USB Host | SYSTEM → INFO confirme la présence d'OP-1 ; aucune sortie TRS ne nécessite de config | |
| CONN-05 | Débrancher puis rebrancher le câble USB PC pendant que le miniMoc fonctionne seul (sans PC) | Le routage MIDI matériel (TRS/USB Host) continue de fonctionner sans interruption pendant le débranchement | |

---

## 2. Routage basique — MATRICE (grille 5×9)

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| MATRICE-01 | Charger le preset d'usine VIDE, jouer une note sur Novation SL MK2 (A) | Aucune sortie ne reçoit la note | |
| MATRICE-02 | Activer la cellule [A, 1] (MATRICE), jouer une note sur A | Dreadbox Typhon (sortie 1) reçoit la note, sur tous les canaux (passthrough canal) | |
| MATRICE-03 | Activer [A,1] et [A,2] simultanément | Note de Novation SL MK2 arrive à la fois sur Typhon (1) et Minifreak (2) | |
| MATRICE-04 | Charger le preset DIAGONALE (A→1, B→2, C→3, D→4, E→5) | Novation SL MK2 (A) → Typhon (1) ; Electribe2 (B) → Minifreak (2) ; MiniLab (C) → Electribe2 (3) | |
| MATRICE-05 | Activer [B,3] (Electribe2 → Electribe2 en sortie 3) | La note jouée sur l'Electribe2 (entrée B) ressort sur sa propre entrée MIDI (sortie 3) — vérifier absence de boucle infinie ou de blocage (le miniMoc ne renvoie pas la note reçue en retour, pas de ré-injection) | |
| MATRICE-06 | Charger le preset MERGE | Toutes les entrées (A,B,C) arrivent sur toutes les sorties actives ; jouer sur MiniLab (C) doit atteindre les 6 sorties TRS + OP-1 (7) | |
| MATRICE-07 | Charger USB>TRS | MiniLab (C, entrée USB) routé vers les sorties TRS 1–6 ; aucune sortie vers OP-1 (7) | |
| MATRICE-08 | Charger TRS>USB | Novation SL MK2 (A) et Electribe2 (B) routés vers OP-1 (sortie 7 USB Host) ; aucune sortie TRS | |
| MATRICE-09 | Désactiver toutes les cellules (retour VIDE) puis vérifier via MONITOR | Barres d'entrée VU-mètre continuent de s'allumer (réception indépendante du routage) mais aucune sortie ne s'anime | |

---

## 3. Routage avancé — FLUX

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| FLUX-01 | Créer un flux : Entrée B canal 10 uniquement → Sortie 3 canal 1 (remap) | Jouer canal 10 sur Electribe2 (B) → arrive sur Electribe2 (sortie 3) reconverti en canal 1 ; jouer canal 1 sur B ne passe pas | |
| FLUX-02 | Créer un flux : Entrée A (ALL canaux) → Sortie 6 (ALL, passthrough) | Toute note de Novation SL MK2 (A), quel que soit le canal, ressort sur sa propre sortie 6 (SL MK2), canal conservé | |
| FLUX-03 | Créer un flux multi-entrées : {A, ch1} + {B, ch2} → {sortie 4, ch1} | ch1 de SL MK2 et ch2 d'Electribe2 arrivent tous deux sur reface CP (sortie 4) en canal 1 | |
| FLUX-04 | Créer 16 flux (maximum) | Le 17e flux est refusé ou impossible à créer (limite respectée) | |
| FLUX-05 | Activer un flux ET la cellule MATRICE correspondante en parallèle (ex FLUX-01 + MATRICE [B,3]) | Doublement du signal MIDI observé sur Electribe2 sortie 3 (comportement cumulatif documenté — non un bug) | |
| FLUX-06 | Modifier un flux existant (bouton Éditer) puis le supprimer (✕) | Modification appliquée immédiatement ; suppression stoppe le routage correspondant sans affecter les autres flux/matrice | |
| FLUX-07 | Créer un flux avec sélection multi-canaux via la grille 4×4 (ex. ch1, ch3, ch7 sur MiniLab C) | Seules les notes sur les canaux cochés passent, les autres canaux de C sont bloqués pour ce flux | |

---

## 4. Synchronisation MIDI Clock (SYNC)

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| SYNC-01 | Régler SYNC = A (TRS), envoyer Clock depuis Novation SL MK2 sur l'entrée A | Toutes les sorties actives (TRS 1–6 + OP-1 si branché) reçoivent le Clock ; MONITOR page BPM affiche le tempo mesuré | |
| SYNC-02 | Régler SYNC = C (USB Host), envoyer Clock depuis MiniLab | Clock diffusé sur toutes les sorties ; vérifier qu'aucune autre source n'est écoutée en parallèle | |
| SYNC-03 | Régler SYNC = A (USB/PC), envoyer Clock depuis Ableton via le câble miroir A | Clock d'Ableton propagé sur les sorties, alors qu'un clock TRS simultané sur l'entrée physique A est ignoré | |
| SYNC-04 | Envoyer simultanément un Clock TRS sur A et un Clock USB/PC sur le miroir A, avec SYNC=A (TRS) | Seul le clock TRS physique est suivi, le clock USB/PC est ignoré (pas de tempo instable/mélangé) | |
| SYNC-05 | Régler SYNC = MINIMOC, définir un BPM via le menu TRANSPORT | Le miniMoc génère lui-même le Clock en continu, y compris en naviguant dans d'autres menus ou avec le VU-mètre MONITOR actif | |
| SYNC-06 | SYNC = MINIMOC, envoyer volontairement un Start MIDI externe depuis SL MK2 sur A | Le Start externe est ignoré (seule la source sélectionnée est maître) | |
| SYNC-07 | Régler SYNC = OFF | Aucun Clock/Start/Stop/Continue n'est propagé sur aucune sortie, quel que soit ce qui est branché en entrée | |
| SYNC-08 | Parcourir la liste des 11 sources + OFF depuis l'écran OLED SYNC | La source active est marquée d'un `*`, indépendamment de la position du curseur ; le choix est conservé après redémarrage (persistance EEPROM) | |
| SYNC-09 | Sélectionner une source SYNC depuis l'onglet SYNC de l'éditeur web | Réglage appliqué et sauvegardé immédiatement, visible aussi sur l'écran OLED | |

---

## 5. Transport (Play/Pause/Stop, MMC)

*Pré-requis : SYNC = MINIMOC.*

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| TRSP-01 | Entrer dans le menu TRANSPORT, tourner l'encodeur | BPM affiché change de ±1 ; ±5 si rotation rapide (< 180 ms entre crans) | |
| TRSP-02 | Clic encodeur (1er appui) | Indicateur passe à PLAY, un message Start (FA) est envoyé sur toutes les sorties + miroir PC | |
| TRSP-03 | Clic simple BACK pendant PLAY | Retour au menu précédent (pas de Stop) | |
| TRSP-04 | Revenir dans TRANSPORT, clic encodeur (Pause) | Indicateur PAUSE, message Stop (FC) envoyé, `transport_resumable=true` | |
| TRSP-05 | Clic encodeur à nouveau depuis PAUSE | Message Continue (FB) envoyé (pas un nouveau Start) | |
| TRSP-06 | Depuis PAUSE, clic simple BACK | STOP réel (le prochain Play enverra Start, pas Continue) | |
| TRSP-07 | Maintenir BACK + clic encodeur | Message MMC Record Strobe envoyé (SysEx `F0 7F 7F 06 06 F7`) — vérifier réception sur un device MMC-compatible ou via un analyseur MIDI | |
| TRSP-08 | Maintenir BACK + tourner à droite / à gauche | MMC Fast Forward / Rewind envoyés respectivement | |
| TRSP-09 | Rester sur l'écran TRANSPORT et vérifier le long-appui BACK global (retour logo) | Le geste global BACK→logo (1s) est désactivé sur cet écran, pour ne pas interrompre les gestes chordés | |
| TRSP-10 | Piloter Play/Pause/Stop/Record/FF/RW depuis l'onglet SYNC de l'éditeur web | Mêmes effets que les gestes physiques, cohérence affichée entre OLED et éditeur web | |

---

## 6. Presets (SD)

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| PRST-01 | Configurer un routage (MATRICE + FLUX + SYNC), Sauver sur preset 5 | Fichier `/PRESETS/PRST05.DAT` créé/mis à jour sur la carte SD | |
| PRST-02 | Charger un autre preset puis recharger le preset 5 | Routage, flux et réglage SYNC restaurés à l'identique | |
| PRST-03 | Redémarrer le miniMoc (coupure d'alimentation) | Le dernier preset actif (EEPROM) est rechargé automatiquement au boot | |
| PRST-04 | Charger chacun des 8 presets d'usine (VIDE, DIAGONALE, MERGE, DISPATCH, TOTAL, SPLIT, USB>TRS, TRS>USB) | Chaque preset produit le comportement décrit dans le guide utilisateur | |
| PRST-05 | Charger un preset legacy v1 (si disponible) | Migration automatique vers v3 sans perte (route_matrix legacy → basic_matrix déduite) | |
| PRST-06 | Sauver/charger un preset via l'éditeur web | Comportement identique à l'écran OLED, astérisque `*` affiché en cas de modif non sauvée | |
| PRST-07 | Tester la limite haute (preset 32) | Sauvegarde/chargement fonctionne sans erreur | |

---

## 7. USB Host — HOST CONFIG

*Pré-requis : MiniLab (entrée) et OP-1 (sortie) branchés sur deux ports USB Host distincts.*

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| HOST-01 | Mode AUTO, brancher MiniLab en premier puis OP-1 en second | MiniLab → port logique C (+7 en miroir de slot), OP-1 second device → assigné au slot suivant (D+8) — noter que sans MANUEL, OP-1 n'est pas nécessairement en sortie 7 | |
| HOST-02 | Mode AUTO, débrancher/rebrancher les deux appareils dans l'ordre inverse | L'assignation change en fonction du nouvel ordre de branchement (comportement FIFO documenté) | |
| HOST-03 | Passer en mode MANUEL depuis SYSTEM → HOST CONFIG | Listes déroulantes activées, assigner explicitement le slot de MiniLab → C et le slot d'OP-1 → 7 | |
| HOST-04 | Avec le mapping manuel de HOST-03, débrancher/rebrancher MiniLab et OP-1 en même temps, en désordre | Chaque appareil retrouve son port logique via VID:PID, indépendamment de l'ordre physique de branchement | |
| HOST-05 | Éteindre/rallumer le miniMoc avec le mapping MANUEL déjà configuré | Mapping conservé après redémarrage (persistance EEPROM adresse 2) | |
| HOST-06 | Configurer HOST CONFIG depuis l'éditeur web (onglet HOST CONFIG) | Résultat identique à la configuration via écran OLED | |
| HOST-07 | Brancher un 3e appareil USB Host sur le port D ou E libre (test de branchement à chaud) | Le nouvel appareil est détecté sans reboot ; SYSTEM → INFO le liste | |
| HOST-08 | Provoquer un problème de reconnaissance (débrancher pendant l'activité) puis lancer USB LINK | Re-détection des appareils en ~2,5 s sans reboot complet | |
| HOST-09 | Lancer USB RESET | Redémarrage complet du Teensy, tous les appareils USB Host se ré-énumèrent proprement | |

---

## 8. Monitor

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| MON-01 | Entrer dans MONITOR (page VU-mètres), jouer sur chacune des 3 entrées (A, B, C) | Chaque barre d'entrée s'allume à réception, avec fondu ~500 ms, indépendamment du routage actif | |
| MON-02 | Router A→1 (MATRICE) et jouer sur A | Barre d'entrée A ET barre de sortie 1 (Typhon) s'animent | |
| MON-03 | Tourner l'encodeur vers la droite pour passer à la page BPM, SYNC = A (TRS) actif avec Clock envoyé | Tempo du maître affiché en grand, arrondi à l'entier le plus proche ; source maître rappelée en haut | |
| MON-04 | Couper la source Clock plus de 2 secondes | Écran affiche NO CLOCK | |
| MON-05 | Régler SYSTEM → BPM REFRESH sur 5000 ms, changer le tempo pendant l'affichage de la page BPM | L'affichage ne se met à jour qu'après ~5 s, sauf clic encodeur qui force une actualisation immédiate | |
| MON-06 | Régler BPM REFRESH sur MANUEL | Affichage jamais rafraîchi automatiquement ; seul le clic encodeur actualise | |
| MON-07 | Vérifier le clignotement de la LED embarquée pendant la lecture SYNC=MINIMOC | LED clignote à chaque temps (toutes les 24 ticks), indépendamment de l'écran affiché | |
| MON-08 | Retourner à l'écran d'accueil après consultation MONITOR | Recommandation du guide : limiter le temps sur MONITOR en usage à forte densité MIDI pour éviter le jitter dû au refresh I²C (~20 ms) — vérifier absence de jitter audible une fois revenu au carousel | |

---

## 9. Menu SYSTEM

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| SYS-01 | SYSTEM → INFO | Stats carte SD affichées + liste des devices USB physiques branchés (MiniLab, OP-1) | |
| SYS-02 | SYSTEM → CONTRASTE, faire varier de 16 à 255 | Luminosité OLED change visiblement sur toute la plage | |
| SYS-03 | SYSTEM → USB LINK | Polling 2,5 s, appareils USB non reconnus forcés à se ré-énumérer, sans reboot | |
| SYS-04 | SYSTEM → USB RESET | Reboot complet du Teensy (équivalent power-cycle), retour au logo puis restauration preset/config | |
| SYS-05 | SYSTEM → BPM REFRESH, parcourir toutes les options (250/500/1000/2000/5000/MANUEL) | Chaque valeur est sélectionnable et persistée en EEPROM (adresse 36) | |
| SYS-06 | Retirer la carte SD puis consulter SYSTEM → INFO | Message d'absence de carte cohérent, pas de blocage du firmware | |

---

## 10. Smart Mirror (Ableton / DAW)

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| MIRROR-01 | Brancher le miniMoc en USB sur le PC, ouvrir Ableton Live | Le miniMoc apparaît comme périphérique MIDI multi-port (15 câbles virtuels), sans installation de driver | |
| MIRROR-02 | Dans Ableton, armer une piste sur le câble correspondant à l'entrée A, jouer sur Novation SL MK2 | La note est reçue en temps réel dans Ableton (câble 1 = entrée physique A) | |
| MIRROR-03 | Router A→1 (MATRICE), armer dans Ableton le câble correspondant à la sortie 1 | Ableton reçoit également le miroir de ce qui sort vers Dreadbox Typhon (câble 6-11 = sorties 1–6) | |
| MIRROR-04 | Jouer sur MiniLab (entrée C, USB Host) avec Ableton armé sur le câble 3 (entrées USB Host C/D/E) | Réception confirmée dans Ableton indépendamment du routage | |
| MIRROR-05 | Envoyer du MIDI depuis Ableton sur le câble miroir de sortie 7 (OP-1) | Le message ressort physiquement sur OP-1 comme si envoyé directement par le miniMoc | |
| MIRROR-06 | Débrancher le PC en cours d'utilisation | Le miniMoc continue de fonctionner en autonome (Smart Mirror non requis pour le fonctionnement standalone) | |
| MIRROR-07 | Rebrancher le PC | Les mêmes ports réapparaissent dans Ableton avec les mêmes numéros de câble | |
| MIRROR-08 | Test croisé SYNC-03/04 (§4) avec Ableton comme source de Clock via le câble miroir A | Confirme le comportement TRS vs USB/PC documenté en §4.1 des specs | |

---

## 11. Enregistrement MIDI sur SD

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| REC-01 | Jouer sur plusieurs entrées avec un routage actif pendant quelques minutes | Fichier(s) `/REC/RECnnn.DAT` créé(s)/mis à jour sur la carte SD | |
| REC-02 | Remplir jusqu'à REC099 puis continuer à jouer | REC001 est écrasé (ring buffer confirmé) | |
| REC-03 | Brancher le miniMoc en USB sous Windows, vérifier qu'il apparaît en disque amovible | Fichiers `/REC/*.DAT` accessibles en copie | |
| REC-04 | Charger un `.dat` dans l'éditeur web, onglet DAT → MIDI | Événements affichés dans un tableau (horodatage, source, destination, type, canal) | |
| REC-05 | Configurer l'export : diviser par sortie physique, BPM 120, PPQ 960, cocher les pistes | Fichier `.mid` téléchargé, lisible dans un lecteur MIDI standard, tempo et résolution respectés | |
| REC-06 | Répéter REC-05 en divisant par source d'entrée plutôt que sortie | Pistes réparties différemment dans le fichier exporté, conformément au choix | |

---

## 12. Weblink / Éditeur web

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| WEB-01 | Ouvrir `weblink/index.html` dans Chrome/Edge, brancher le miniMoc en USB | Menu déroulant "— Port MIDI —" liste le miniMoc | |
| WEB-02 | Sélectionner le port miniMoc | Point de statut passe au vert ("Connecté"), badge de version firmware affiché à côté du logo | |
| WEB-03 | Ouvrir avec un navigateur non-Chromium (Firefox) | Connexion impossible (Web MIDI API non supportée) — comportement attendu, à documenter | |
| WEB-04 | Tester chaque onglet (SYNC, MATRICE, FLUX, HOST CONFIG, FIRMWARE, DAT→MIDI) avec la config matérielle en place | Chaque onglet reflète et modifie l'état réel du miniMoc en temps réel | |
| WEB-05 | Modifier une cellule MATRICE depuis le navigateur, vérifier sur l'écran OLED | Changement appliqué immédiatement et visible sur les deux interfaces | |
| WEB-06 | Onglet FIRMWARE → sélectionner une version depuis "Firmwares du serveur" | Fichier `.hex` téléchargé automatiquement depuis le serveur | |
| WEB-07 | Lancer la mise à jour firmware (⬆), choisir le port série dans le sélecteur navigateur | Barre de progression, confirmation, redémarrage automatique du miniMoc avec le nouveau firmware actif | |
| WEB-08 | Vérifier le comportement si le firmware connecté est plus ancien que l'éditeur web | Bandeau d'avertissement affiché avec lien vers une version compatible du configurateur | |
| WEB-09 | Onglet FIRMWARE → "Fichier local", glisser un `.hex` compilé manuellement (Arduino IDE, Exporter le binaire compilé) | Même flux de mise à jour que WEB-06/07, avec le fichier local | |
| WEB-10 | Brancher/débrancher le miniMoc pendant que l'éditeur web est ouvert | Le menu déroulant "Port MIDI" reflète la disponibilité (dé)connexion sans crash de la page | |

---

## 13. Robustesse / cas limites

| ID | Procédure | Résultat attendu | OK/KO |
|----|-----------|-------------------|:---:|
| ROB-01 | Couper l'alimentation brutalement pendant une écriture SD (sauvegarde preset ou enregistrement) | Redémarrage propre, pas de corruption bloquante de la carte SD (au pire perte du dernier événement) | |
| ROB-02 | Enchaîner rapidement de nombreux changements MATRICE/FLUX/SYNC via l'éditeur web | Pas de blocage ni désynchronisation entre firmware et éditeur web | |
| ROB-03 | Saturer une sortie TRS avec un flux de données MIDI dense (ex. pitch bend continu sur SL MK2) pendant que SYNC=MINIMOC | Le Clock interne (IntervalTimer/ISR) reste stable, pas de dérive tempo malgré la charge UART | |
| ROB-04 | Saturer une sortie USB Host (OP-1) de la même façon | `internal_clock_flush_usb()` absorbe le flag en best-effort ; vérifier qu'il n'y a pas de blocage prolongé du Clock USB Host, tout en acceptant une éventuelle latence supérieure au DIN | |
| ROB-05 | Naviguer dans tous les menus OLED pendant qu'un Clock externe rapide (200+ BPM) est actif sur une entrée TRS | Pas de perte de synchro DIN (l'ISR UART préempte l'affichage OLED bloquant ~8ms) | |
| ROB-06 | Débrancher un appareil USB Host en pleine transmission MIDI (ex. MiniLab pendant une note tenue) | Pas de note bloquée en "stuck note" sur les sorties routées ; récupération propre après USB LINK/RESET | |

---

## Synthèse

| Thème | Nb tests | OK | KO | Non testé |
|-------|:---:|:---:|:---:|:---:|
| 1. Connectique | 5 | | | |
| 2. Matrice | 9 | | | |
| 3. Flux | 7 | | | |
| 4. Sync | 9 | | | |
| 5. Transport | 10 | | | |
| 6. Presets | 7 | | | |
| 7. Host Config | 9 | | | |
| 8. Monitor | 8 | | | |
| 9. System | 6 | | | |
| 10. Smart Mirror | 8 | | | |
| 11. Enregistrement SD | 6 | | | |
| 12. Weblink | 10 | | | |
| 13. Robustesse | 6 | | | |
| **Total** | **100** | | | |
