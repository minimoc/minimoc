# Licences des bibliothèques tierces

Le firmware miniMoc utilise les bibliothèques externes listées ci-dessous.
Elles ne sont **pas** incluses dans ce dépôt : elles doivent être installées
séparément via le gestionnaire de bibliothèques de l'IDE Arduino / Teensyduino
(voir la section « Dépendances » du README).

Chaque bibliothèque reste soumise à sa propre licence. Les mentions de copyright
ci-dessous sont conservées conformément aux termes des licences MIT et BSD.

| Bibliothèque | Auteur | Licence | Lien |
|---|---|---|---|
| U8g2 | oliver (olikraus) | BSD 2-Clause | https://github.com/olikraus/u8g2 |
| Arduino MIDI Library (v5) | Francois Best | MIT | https://github.com/FortySevenEffects/arduino_midi_library |
| USBHost_t36 | Paul Stoffregen / PJRC | MIT | https://github.com/PaulStoffregen/USBHost_t36 |
| MTP_Teensy | PJRC / contributors | MIT | https://github.com/KurtE/MTP_Teensy |
| SdFat | Bill Greiman | MIT | https://github.com/greiman/SdFat |
| Encoder | Paul Stoffregen / PJRC | MIT | https://github.com/PaulStoffregen/Encoder |
| Bounce2 | Thomas Ouellet Fredericks | MIT | https://github.com/thomasfredericks/Bounce2 |
| Core Teensy (Wire, EEPROM, FS, usb_names) | PJRC | MIT (+ clause PJRC) | https://github.com/PaulStoffregen/cores |

## Notes importantes

- **U8g2 — fonts.** Le firmware n'utilise que des fonts du domaine public
  (`5x7_tf`, `6x10_tf`, `7x13_tf`, `8x13_tf`). La bibliothèque U8g2 contient par
  ailleurs un font sous GPLv2 (`u8g2_font_siji_t_6x10`) qui n'est **pas** utilisé
  par ce projet et n'entre donc pas dans le binaire compilé. Pour que cela reste
  vrai, U8g2 ne doit jamais être copié dans ce dépôt : il est référencé comme
  dépendance externe uniquement.

- **Fichiers d'exemple.** Certaines bibliothèques (MIDI, USBHost_t36) contiennent
  des sketches d'exemple sous licence GPL/LGPL. Ces fichiers ne sont jamais
  compilés dans le firmware miniMoc et n'ont aucun impact sur sa licence. C'est
  une raison de plus de ne pas embarquer ces bibliothèques dans le dépôt.

- **Clause PJRC (core Teensy).** Les fichiers du core Teensy portent une clause
  additionnelle PJRC qui concerne l'intégration dans un IDE / toolchain listant
  des cibles matérielles. Elle ne s'applique pas à un firmware utilisateur.
