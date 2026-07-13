# Faisabilité : Transformateurs & Générateurs MIDI sur miniMoc

## Contexte

miniMoc est un routeur MIDI DAWless (Teensy 4.1, écran OLED 128x64, encodeur rotatif) avec un éditeur web compagnon "weblink" (Web MIDI/SysEx + Web Serial pour le flashage). Aujourd'hui, la matrice de routage (`route_matrix`, `firmware/logic.h:104-198`) ne fait que **recopier les messages MIDI d'une entrée vers une sortie**, avec au maximum un remap de canal — aucun traitement du message lui-même.

L'idée : ajouter deux familles de blocs de traitement, comme le font d'autres équipements MIDI (Retrokits RK-006, Squarp Hermod, Chase Bliss, etc.) :
- **Transformateurs** — modifient un message MIDI existant en transit (transpose, scale/clamp de vélocité, remap/scale/quantize de CC, offset de PC, scale de pitch bend, etc.)
- **Générateurs** — produisent eux-mêmes des messages MIDI de façon autonome (LFO, arpégiateur, séquenceur euclidien)

Ces deux enums (`TransformType`, `GeneratorType`) n'existent pas dans le code actuel — c'est un **greenfield feature**, pas une fonctionnalité à terminer. Il n'y a aucune trace dans le CHANGELOG ou l'historique git. Ce document est une évaluation de faisabilité et un espace de réflexion architecturale, pas encore un plan d'implémentation détaillé.

## Ce qui existe et sur quoi ça s'accrocherait

- **Routage** (`firmware/logic.h:104-198`, `firmware/_midi.h`) : `route_matrix[in_port][in_chan][out_port]` est une table booléenne + remap de canal, construite depuis `basic_matrix` et une liste de règles `Flux` (`flux_list[16]`, chacune avec des slots d'entrée/sortie). Les handlers (`handleNoteOn/Off`, `handleControlChange`, `processPitchBend`, `processAfterTouch`, `handleProgramChange` dans `_midi.h`) font le lookup puis envoient directement (`SEND_NOTE_ON` etc.) — **aucun point d'accroche pour un traitement intermédiaire n'existe aujourd'hui**.
- **UI on-device** (`firmware/routing_submenu.h`) : un menu en wizard (liste de Flux → étape entrées → étape sorties → sélecteur de canal par port). L'écran 128px est déjà serré ; un écran générique "liste de paramètres" serait nécessaire pour éditer des transforms (pas juste réutiliser les grilles fixes actuelles).
- **weblink** (`weblink/index.html`) : mirroir exact du firmware — `FluxEditor`, SysEx opcodes 0x04–0x0D déjà occupés (BPM, matrice, flux, sync, contraste, host config). Un mécanisme de version/bump SysEx existe déjà (`0x0A`), donc étendre le protocole est un problème déjà anticipé, pas un blocage.
- **Contraintes temps réel** : le clock est déjà géré par ISR (`_midi.h:135-176`, marqué real-time-critical, non-blocking). Tout générateur autonome (LFO/arpège/euclidien) devra s'accrocher à cette horloge existante plutôt que d'en créer une nouvelle.

## Analyse de faisabilité

**Verdict : oui, c'est faisable et réaliste — mais ce n'est pas un petit patch.** C'est une fonctionnalité transverse qui touche le firmware (dispatch + stockage EEPROM), l'UI OLED, l'UI weblink et le protocole SysEx simultanément. Rien n'est architecturalement bloqué ni n'exige de bricolage exotique : ce sont des traitements MIDI simples et bien connus (transpose, scale, clamp, remap, LFO, euclidien...), déjà faits dans d'autres équipements.

### Transformateurs — complexité modérée, à faire en premier
- Modèle de données : attacher un transform optionnel à chaque `Flux` (le concept de "règle" existe déjà, c'est une extension naturelle plutôt qu'une nouvelle structure).
- Point d'accroche : insérer l'appel du transform entre le lookup `route_matrix` et les `SEND_*`, dans ~5-6 handlers — mécanique mais répétitif.
- Coût CPU : négligeable (arithmétique simple), aucun souci sur un Teensy 4.1 à 600 MHz.
- Stockage EEPROM : de la place libre (prochain octet libre ~39), mais la disposition doit être versionnée proprement (le projet a déjà un mécanisme de migration via l'OTA/version bump).
- UI OLED + weblink : travail répétitif (un écran/formulaire par type de transform), pas complexe individuellement mais volumineux si on veut couvrir les ~25 types listés.

### Générateurs — plus complexe, à faire ensuite
- Contrairement aux transforms, un générateur n'est **pas déclenché par une entrée** — c'est une source autonome qui doit être routée vers une ou plusieurs sorties. Idée à valider : le traiter comme une "entrée virtuelle" dans `flux_list`, ce qui réutiliserait gratuitement toute la UI et la logique de routage existantes pour ses sorties — probablement le levier de simplification le plus important du projet.
- LFO : simple (forme d'onde, taux, sync clock, CC cible) — complexité modérée.
- Arpégiateur : état des notes tenues, modes de direction, latch — complexité moyenne-haute, plus de cas limites.
- Euclidien : peut réutiliser l'infra de clock/division déjà présente (`TRANS_CLOCK_DIVIDER` est dans la même famille d'idée) — modéré.
- Risque principal : tout code de tick doit respecter les contraintes non-bloquantes déjà en place dans l'ISR de clock.

### Effort réaliste
Pas une fonctionnalité de "quelques heures" — plutôt un projet de plusieurs semaines si on veut une poignée de transforms + 2-3 générateurs avec firmware **et** weblink à jour, fait type par type de façon incrémentale. Le principal risque n'est pas technique mais de **volume** : chaque nouveau type de transform/générateur implique un écran OLED + un formulaire weblink + un opcode SysEx à maintenir en synchro des deux côtés.

## Décisions prises

1. **Transform attaché à la route individuelle (niveau Flux)** — chaque `Flux` porte un transform optionnel ; cohérent avec le fait que `Flux` est déjà l'unité de règle dans `flux_list`.
2. **Générateur = entrée virtuelle** — un générateur (LFO, arpégiateur, euclidien) est traité comme une source supplémentaire aux côtés des 5 ports d'entrée physiques, et se raccorde à des sorties via le mécanisme `Flux` existant. Ça évite de dupliquer la logique de routage/canaux pour les générateurs.
3. **Un écran OLED dédié par type de transform** — pas d'écran générique "liste de paramètres" mutualisé ; chaque type de transform (transpose, vel scale, CC remap, etc.) a son propre écran de configuration, plus simple à lire sur 128x64 mais plus de volume de code UI à produire et maintenir (et à répliquer côté weblink).

Prochaine étape si on avance : définir précisément la structure de données du transform attaché au Flux (union/struct par type ? enum + params génériques ?) et le nouveau format d'entrée virtuelle pour les générateurs, avant de toucher au firmware.

## État

Faisabilité confirmée, décisions d'architecture verrouillées (ci-dessus). Pas de code écrit. En attente d'une demande explicite pour passer à un plan d'implémentation détaillé (structures de données précises, découpage des opcodes SysEx, ordre des types de transform/générateur à livrer en premier).

## POC recommandé : TRANS_NOTE_TRANSPOSE

Premier transform à coder : transpose de note (offset signé, ex. -24..+24 demi-tons), attaché à un `Flux`. Choisi parce que :
- Un seul paramètre (int8), pas de courbe/plage à gérer — valide la plomberie complète (Flux → EEPROM → firmware → OLED → weblink → SysEx) sans complexité annexe.
- Utile en soi (adapter le registre entre clavier/synthé, décaler un séquenceur).
- Touche exactement 2 handlers (`handleNoteOn`/`handleNoteOff` dans `_midi.h`), assez pour valider le pattern d'insertion du hook sans la variété CC/PC/PB/AT.
- UI minimale des deux côtés (un seul widget valeur, OLED + weblink).

Une fois ce transform validé bout-en-bout, les ~24 autres types deviennent un travail répétitif plutôt qu'un risque architectural.

## Extension architecturale : points virtuels (patch bay)

Idée proposée : généraliser "générateur = source virtuelle" en introduisant des **points virtuels** qui peuvent être à la fois :
- la **destination** d'un Flux (au lieu d'un port de sortie physique)
- la **source** d'un autre Flux (au lieu d'un port d'entrée physique / générateur)

Ça transforme le modèle actuel (table plate `route_matrix` : une entrée → une sortie) en un **graphe de patch** permettant de chaîner plusieurs transforms en série (`entrée → transform A → point virtuel → transform B → sortie`) et de faire du fan-in/fan-out entre points virtuels — beaucoup plus de combinaisons qu'un simple transform mono-étape entre une entrée et une sortie.

**Implications architecturales :**
- **Espace des ports élargi** : entrées physiques + générateurs + points virtuels comme sources ; sorties physiques + points virtuels comme destinations. Extension de la liste déjà proposée dans le wizard OLED / `FluxEditor` weblink, pas un nouveau paradigme d'UI.
- **Détection de cycle nécessaire** : un point virtuel étant source ET destination, une boucle est possible (A → ... → A) — à détecter à la validation de la config (DFS/tri topologique), jamais au runtime.
- **Résolution recommandée à la config, pas au runtime** : aplatir/précalculer le graphe en une séquence de transforms lors d'un changement de config (comme le fait déjà `recompute_route_matrix()`), pour garder les handlers temps réel (`_midi.h`, ISR clock) aussi simples qu'aujourd'hui — un simple lookup + séquence de transforms précalculée, sans marche de graphe par message.
- **Stockage** : coût EEPROM marginal (juste un id de port supplémentaire), le vrai coût est en logique de validation/aplatissement à la config.

**Verdict** : faisable, et ça augmente significativement la puissance du système (vrai patch bay chaînable) sans compromettre le temps réel, à condition d'aplatir le graphe à la config plutôt que de le parcourir en live. C'est une extension architecturale plus large que les 3 décisions initiales — à traiter comme une itération après le POC transpose, pas en même temps.
