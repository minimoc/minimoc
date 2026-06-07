# miniMoc

**Routeur MIDI hybride DAWless, fait main.**


🇬🇧 In English — miniMoc is a handmade, open-hardware DAWless MIDI router built around a Teensy 4.1: 5 MIDI inputs, 9 outputs, TRS TYPE A + USB Host, a 128x64 OLED screen and a rotary encoder. Route MIDI between your synths and machines with no computer. Its signature feature, Smart Mirror: plug in a PC and the miniMoc's physical ports reappear as identical virtual ports in your DAW — no setup, no drivers. Unplug and you're back to standalone mode. The project is at v0.2 (routing, Smart Mirror and SD presets are solid; MIDI auto-recording is work in progress). Documentation is currently in French — English translation contributions are very welcome (see CONTRIBUTING.md). Licence: source-available, non-commercial (PolyForm NC for firmware, CC BY-NC for hardware) — personal use is free, commercial use requires permission.


Le miniMoc est un boîtier portable qui route le MIDI entre vos synthés, séquenceurs
et machines — sans ordinateur. cinq entrées (2 TRS Type A + 3 USB), neuf sorties (6 TRS TYPE A + 3 USB), USB Host, un écran OLED 128x64 pixels
et un encodeur pour tout piloter à la main.

> **Smart Mirror** — branchez un PC/MAC/LINUX/SMARTPHONE et les ports physiques du miniMoc réapparaissent
> à l'identique en ports virtuels dans votre DAW. Aucune configuration, aucun pilote
> à installer. Vous débranchez, vous repartez en mode autonome.



---

## Démonstrations vidéo

- **Le projet et l'appel aux compagnons** : https://www.youtube.com/watch?v=sc03914Uq1w
- **Présentation du hardware** (ce que vous recevez, comment c'est fait) : https://youtu.be/FKErdNdDdWI
- **La navigation dans les menus** : https://youtu.be/ahOwxcXlfOY
- **Le smartmirror expliqué** : https://youtu.be/d6lrl4uF-Bw

---

## État du projet — v0.2

Le miniMoc en est à sa version 0.2, assemblée à la main.

**Ce qui fonctionne et est éprouvé :**
- Routage MIDI complet (modes BASIC et ADVANCED)
- Smart Mirror (miroir des ports physiques vers le DAW)
- Sauvegarde / rappel des configurations sur carte SD
- Interface écran OLED + encodeur

**En chantier :**
- Auto-enregistrement MIDI (capture automatique des jams) — fonctionnel mais en
  cours de mise au point. C'est précisément là que les retours des premiers
  utilisateurs sont les plus utiles.

---

## Ce que c'est (et ce que ce n'est pas)

Le miniMoc est un projet dans un esprit maker. Ce n'est pas un
produit fini sous coque fermée : il se présente sous forme de carte (PCB) avec son
Teensy 4.1, et un *bezel* maintient l'écran en position. Il n'y a pas (encore) de
boîtier complet — c'est un terrain de contribution ouvert pour qui aime la CAO.

---

## Matériel

- **Microcontrôleur** : Teensy 4.1 (PJRC)
- **Affichage** : écran OLED monochrome (I²C)
- **Entrées / sorties** : 2+3 IN MIDI, 6+3 OUT MIDI
- **Contrôle** : encodeur rotatif + bouton

Les fichiers de conception (schémas, PCB, BOM) se trouvent dans le dossier
[`hardware/`](hardware/). Le *bezel* à imprimer est dans [`bezel/`](bezel/).

---

## Firmware

Le code du firmware est dans [`firmware/`](firmware/). Il se compile avec
**l'IDE Arduino + Teensyduino**, pour Teensy 4.1.

### Dépendances

Ces bibliothèques **ne sont pas incluses dans ce dépôt**. Installez-les via le
gestionnaire de bibliothèques de l'IDE Arduino (ou elles sont fournies avec
Teensyduino) :

- U8g2
- Arduino MIDI Library (version 5+)
- USBHost_t36 *(fournie avec Teensyduino)*
- MTP_Teensy
- SdFat
- Encoder *(fournie avec Teensyduino)*
- Bounce2

> ⚠️ N'essayez pas de récupérer ces bibliothèques depuis ce dépôt : elles n'y sont
> volontairement pas. Passez par le gestionnaire de bibliothèques Arduino.

Les licences de ces dépendances sont détaillées dans
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).

---

## Licence

**Sources ouvertes — usage personnel libre, commercialisation sur accord.**

- **Firmware** (dossier `firmware/`) : licence **PolyForm Noncommercial 1.0.0**.
  Vous pouvez l'utiliser, l'étudier, le modifier et le partager **pour un usage
  non commercial**.
- **Matériel** (dossiers `hardware/` et `bezel/`) : licence
  **Creative Commons Attribution-NonCommercial 4.0 (CC BY-NC 4.0)**.

Toute **utilisation commerciale** (vente, intégration dans un produit vendu, etc.)
nécessite mon accord préalable. Si vous souhaitez commercialiser quelque chose à
partir du miniMoc, contactez-moi : c'est tout à fait possible de s'entendre.

Voir les fichiers [`LICENSE`](LICENSE) (firmware) et
[`hardware/LICENSE`](hardware/LICENSE) (matériel).

---

## Contribuer

Le miniMoc avance avec celles et ceux qui l'utilisent. Retours d'usage,
corrections, idées de fonctionnalités, conception d'un boîtier complet : tout est
bienvenu. Voir [`CONTRIBUTING.md`](CONTRIBUTING.md).

---

*miniMoc — conçu et assemblé à la main par François Jacob.*
