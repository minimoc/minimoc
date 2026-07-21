Installation de l'env. IDE Arduino:

1 - télécharger et installer la dernière version https://www.arduino.cc/en/software/
2 - installer TeensyDduino: https://www.pjrc.com/teensy/td_download.html
3 - copier le fichier boards.txt modifié + usb_desc.h modifié
4 - supprimer C:\Users\XXXXXXXX\AppData\Roaming\arduino-ide
5 - Selectionner Teensy 4.1 MINOMOC comme boards
6 - Selectionner Tools/USB TYPE : Serial + MIDI16 + MTP
7 - librairies:
Installer les bibliothèques manquantes

Via Tools > Manage Libraries... (gestionnaire de bibliothèques), installe celles qui ne sont pas fournies par Teensyduino :

- U8g2 (écran OLED)
