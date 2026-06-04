#define FIRMWARE_VERSION "v2.0.2"

#define MIDIA MIDIA_5
#define MIDI5 MIDIA_5

#define MIDIB MIDIB_6
#define MIDI6 MIDIB_6


// DEBUG LED
void traceLED() {
      digitalWrite(LED_BUILTIN, HIGH); // Allume la LED de la Teensy
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);  // Éteint la LED      
}

#include <U8g2lib.h>
#include <Wire.h>
#include <string.h> 
#include <EEPROM.h> 
#include <MIDI.h>
#include <USBHost_t36.h>
#include <MTP_Teensy.h>

#include "graphics.h"
#include "logic.h"
#include "monitor.h"       // données VU-mètre — doit précéder sd_recorder (mon_push appelé dans rec_push)
#include "sd_recorder.h"   // déclare sd, sd_ok — doit précéder sd_presets et preset
#include "sd_presets.h"    // sd_preset_save/load/exists, SD_PRESET_MAX
#include "sd_mtp_adapter.h"

SdFsAdapter sd_mtp_fs;

#include "preset.h"        // silent_save/load_preset — après sd_presets
#include "_midi.h"
#include "PCEditor.h"
#include "ota.h"

// --- New carousel UI (replaces menu.h) ---
#include "carousel_ui.h"
#include "routing_submenu.h"
#include "preset_submenu.h"
#include "info_submenu.h"
#include "monitor_submenu.h"
#include "usb_host_config.h"   // après _midi.h (MIDIUSB1/2/3) et carousel_ui.h (UI_FONT_*)
#include "sync_submenu.h"      // après usb_host_config.h (utilise hc_dev_name)
#include "system_submenu.h"    // après info_submenu.h et usb_host_config.h
#include "controls.h"   // must come after carousel_ui.h (uses u8g2)


// -----------------------------------------------------------------
// J. SETUP ET LOOP
// -----------------------------------------------------------------

void setup() {

  ota_setup();   // USB CDC + TeensyOTA — avant toute autre init

// pour effacer l'eeprom en cas de changement de structure
// for (int i = 0 ; i < 4096 ; i++) {
//    EEPROM.write(i, 0);
//}

// 1024 octets de buffer au lieu de 1k pour éviter les pertes éventuelles de packets
Serial1.addMemoryForRead(new uint8_t[1024], 1024); // On passe de 64 à 1024 octets !
Serial2.addMemoryForRead(new uint8_t[1024], 1024); // On passe de 64 à 1024 octets !


#ifdef DEBUG
  Serial.begin(115200);
#endif

  // On écoute le port USB relié au PC (pour éditeur web sur le PC)
  usbMIDI.setHandleSystemExclusive(handleEditorSysex);

  // MIDI
  MIDI1.begin();
  MIDI2.begin();
  MIDI3.begin();
  MIDI4.begin();
  MIDI5.begin(MIDI_CHANNEL_OMNI);
  MIDI6.begin(MIDI_CHANNEL_OMNI);

  MIDIA.turnThruOff();
  MIDIB.turnThruOff();

  MIDIA.setHandleNoteOn(handleNoteOnA);
  MIDIA.setHandleNoteOff(handleNoteOffA);
  MIDIA.setHandleControlChange(handleControlChangeA);
  MIDIA.setHandleProgramChange(handleProgramChangeA);
  
// Port A utilise les versions "A"
  MIDIA.setHandlePitchBend(handlePitchBendA);
  MIDIA.setHandleAfterTouchChannel(handleAfterTouchA);

  // Port B utilise les versions "B"
  MIDIB.setHandlePitchBend(handlePitchBendB);
  MIDIB.setHandleAfterTouchChannel(handleAfterTouchB);

  MIDIB.setHandleNoteOn(handleNoteOnB);
  MIDIB.setHandleNoteOff(handleNoteOffB);
  MIDIB.setHandleControlChange(handleControlChangeB);
  MIDIB.setHandleProgramChange(handleProgramChangeB);

  // USB HOST — callbacks real-time (clock/transport) pour le maître SYNC
  MIDIUSB1.setHandleRealTimeSystem(handleRT_dev0);
  MIDIUSB2.setHandleRealTimeSystem(handleRT_dev1);
  MIDIUSB3.setHandleRealTimeSystem(handleRT_dev2);
  MIDIUSB4.setHandleRealTimeSystem(handleRT_dev3);
  MIDIUSB5.setHandleRealTimeSystem(handleRT_dev4);
  MIDIUSB6.setHandleRealTimeSystem(handleRT_dev5);

  // USB HOST — callbacks dynamiques par slot (port_slot[] détermine le rôle)
  MIDIUSB1.setHandleNoteOn(handleNoteOn_dev0);
  MIDIUSB1.setHandleNoteOff(handleNoteOff_dev0);
  MIDIUSB1.setHandleControlChange(handleCC_dev0);
  MIDIUSB1.setHandleProgramChange(handlePC_dev0);
  MIDIUSB1.setHandlePitchChange(handlePB_dev0);
  MIDIUSB1.setHandleAfterTouchChannel(handleAT_dev0);

  MIDIUSB2.setHandleNoteOn(handleNoteOn_dev1);
  MIDIUSB2.setHandleNoteOff(handleNoteOff_dev1);
  MIDIUSB2.setHandleControlChange(handleCC_dev1);
  MIDIUSB2.setHandleProgramChange(handlePC_dev1);
  MIDIUSB2.setHandlePitchChange(handlePB_dev1);
  MIDIUSB2.setHandleAfterTouchChannel(handleAT_dev1);

  MIDIUSB3.setHandleNoteOn(handleNoteOn_dev2);
  MIDIUSB3.setHandleNoteOff(handleNoteOff_dev2);
  MIDIUSB3.setHandleControlChange(handleCC_dev2);
  MIDIUSB3.setHandleProgramChange(handlePC_dev2);
  MIDIUSB3.setHandlePitchChange(handlePB_dev2);
  MIDIUSB3.setHandleAfterTouchChannel(handleAT_dev2);

  MIDIUSB4.setHandleNoteOn(handleNoteOn_dev3);
  MIDIUSB4.setHandleNoteOff(handleNoteOff_dev3);
  MIDIUSB4.setHandleControlChange(handleCC_dev3);
  MIDIUSB4.setHandleProgramChange(handlePC_dev3);
  MIDIUSB4.setHandlePitchChange(handlePB_dev3);
  MIDIUSB4.setHandleAfterTouchChannel(handleAT_dev3);

  MIDIUSB5.setHandleNoteOn(handleNoteOn_dev4);
  MIDIUSB5.setHandleNoteOff(handleNoteOff_dev4);
  MIDIUSB5.setHandleControlChange(handleCC_dev4);
  MIDIUSB5.setHandleProgramChange(handlePC_dev4);
  MIDIUSB5.setHandlePitchChange(handlePB_dev4);
  MIDIUSB5.setHandleAfterTouchChannel(handleAT_dev4);

  MIDIUSB6.setHandleNoteOn(handleNoteOn_dev5);
  MIDIUSB6.setHandleNoteOff(handleNoteOff_dev5);
  MIDIUSB6.setHandleControlChange(handleCC_dev5);
  MIDIUSB6.setHandleProgramChange(handlePC_dev5);
  MIDIUSB6.setHandlePitchChange(handlePB_dev5);
  MIDIUSB6.setHandleAfterTouchChannel(handleAT_dev5);

  // Pool USB aligné sur le sketch testHOST (config validée stable en hot-plug).
  static Pipe_t     usb_extra_pipes[12] __attribute__((aligned(32)));
  static Transfer_t usb_extra_xfr[48]  __attribute__((aligned(32)));
  myusb.contribute_Pipes(usb_extra_pipes, 12);
  myusb.contribute_Transfers(usb_extra_xfr, 48);

  myusb.begin();

  // Pins et OLED
  selectButton.attach(PIN_VALID, INPUT_PULLUP);
  selectButton.interval(5);
  pinMode(PIN_BACK, INPUT_PULLUP);
  Wire.begin();
  Wire.setClock(400000);   // 400 kHz : valeur sûre pour le SSD1306
  u8g2.begin();
  u8g2.sendF("ca", 0xD5, 0xF0);   // oscillateur SSD1306 au max (défaut 0x80)

  // --- Carousel principal : PRESET / SYNC / ROUTAGE / MONITOR / SYSTEM ---
  carousel_register("PRESET",  icon_preset_16,   icon_preset_32,   NULL);
  carousel_register("SYNC",    icon_sync_16,     icon_sync_32,     NULL);
  carousel_register("ROUTAGE", icon_routing_16,  icon_routing_32,  NULL);
  carousel_register("MONITOR", icon_monitor_16,  icon_monitor_32,  NULL);
  carousel_register("SYSTEM",  icon_info_16,     icon_info_32,     NULL);

  sd_setup();
  if (sd_ok) {
    sd_preset_init();
    sd_mtp_fs.init(sd);
    MTP.addFilesystem(sd_mtp_fs, "MiniMoc SD");
  }
  MTP.begin();

  hc_load();       // Restaure assignation USB Host + état LOCK depuis EEPROM
  sync_load();     // Restaure le maître SYNC depuis EEPROM
  contrast_load(); contrast_apply(); // Restaure le contraste OLED depuis EEPROM

  // Chargement du dernier preset actif depuis la SD
  uint8_t stored_preset = 0;
  EEPROM.get(ACTIVE_PRESET_ADDR, stored_preset);
  if (stored_preset >= 1 && stored_preset <= MAX_PRESETS) {
    current_preset = stored_preset;
    silent_load_preset();   // silencieux si preset absent ou SD indisponible
    recompute_route_matrix(); // recalcule route_matrix depuis basic_matrix + flux_list
  } else {
    current_preset = 1;
  }

  // Démarrage du mode interactif ou pas
  delay(10);
  if (digitalRead(PIN_BACK) == LOW) {
    modeInteractif = true;
  } else {
    oldPosition = myEnc.read();   // sync encodeur avant d'afficher le logo
    afficherLogoVerveine();
  }
}

void loop() {

  ota_tick();   // vérifie Serial sans bloquer ; bloque seulement pendant un transfert OTA actif

int buttonState = digitalRead(PIN_BACK);

  // ── Réveil depuis le mode logo ────────────────────────────────
  // Toute action (BACK, VALID ou rotation encodeur) sort du logo.
  // On attend d'abord que les touches soient relâchées (cas du retour
  // au logo via appui long : le bouton est encore tenu à ce moment).
  static bool logo_wait_release = false;

  if (!modeInteractif) {
    if (logo_wait_release) {
      // Attendre relâchement complet avant d'autoriser le réveil
      if (buttonState == HIGH && digitalRead(PIN_VALID) == HIGH)
        logo_wait_release = false;
    } else {
      bool wake = (buttonState == LOW) || (digitalRead(PIN_VALID) == LOW);
      if (!wake) {
        long newEnc = myEnc.read();
        if (newEnc != oldPosition) { oldPosition = newEnc; wake = true; }
      }
      if (wake) {
        modeInteractif = true;
        carousel_draw_frame(carousel_anim_offset);
      }
    }
  }

  // ── Appui long BACK → bascule vers le logo ────────────────────
  if (buttonState == LOW) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
    }
    if (millis() - buttonPressStartTime > longPressDuration) {
      modeInteractif = !modeInteractif;
      if (!modeInteractif) {
        oldPosition = myEnc.read();
        logo_wait_release = true;   // attendre relâchement avant réveil
        afficherLogoVerveine();
      }
      buttonPressStartTime = 0;
    }
  } else {
    buttonPressStartTime = 0;
  }

  if (modeInteractif) {
    static unsigned long lastUI = 0;
    if (millis() - lastUI > 50) {
      lastUI = millis();
      ui_tick();
    }
  }

  // test d'un msg MIDI sur entrée A
  if (MIDIA.read()) {
    byte msgType = MIDIA.getType();

    // Forward Real-Time (clock/transport) — uniquement si port A est le maître SYNC
    if (msgType == midi::Start || msgType == midi::Stop || msgType == midi::Continue ||
        msgType == midi::Clock || msgType == midi::ActiveSensing || msgType == midi::SystemReset) {
      if (sync_master == 0) _sync_forward(msgType);
      // si sync_master != 0 (autre maître ou OFF) : filtré
    }

    usbMIDI.send(MIDIA.getType(), MIDIA.getData1(), MIDIA.getData2(),
                 MIDIA.getChannel(), USB_PORT_A);
  }

  if (MIDIB.read()) {
    byte msgType = MIDIB.getType();

    if (msgType == midi::Start || msgType == midi::Stop || msgType == midi::Continue ||
        msgType == midi::Clock || msgType == midi::ActiveSensing || msgType == midi::SystemReset) {
      if (sync_master == 1) _sync_forward(msgType);
      // si sync_master != 1 (autre maître ou OFF) : filtré
    }
    
    // On le renvoie vers le Port USB 
    usbMIDI.send(MIDIB.getType(), 
                 MIDIB.getData1(), 
                 MIDIB.getData2(), 
                 MIDIB.getChannel(), 
                 USB_PORT_B); // <--- Le chiffre 1 est le port USB
  }



  myusb.Task();
  MIDIUSB1.read();
  MIDIUSB2.read();
  MIDIUSB3.read();
  MIDIUSB4.read();
  MIDIUSB5.read();
  MIDIUSB6.read();
  hc_poll();   // détecte connexions/déconnexions pour le LOCK VID:PID

  // Second appel myusb.Task() avant les opérations bloquantes (sd_tick ~1-3ms,
  // MTP.loop variable) pour que le hub ne manque pas ses interrupts de port.
  // Sans ça, le gap entre deux Task() peut dépasser le bInterval du hub (~8ms).
  myusb.Task();
  MIDIUSB1.read(); MIDIUSB2.read(); MIDIUSB3.read();
  MIDIUSB4.read(); MIDIUSB5.read(); MIDIUSB6.read();

  // Lecture de tous les messages USB depuis le PC
  // usbMIDI.read() sans argument lit le prochain message quel que soit le câble.
  // usbMIDI.getCable() retourne ensuite le numéro de câble USB pour dispatcher.
  // Note: usbMIDI.read(n) filtre par canal MIDI (pas par câble) — ne pas utiliser pour ça.
  sd_tick();

  // MTP : accès SD depuis le PC — après sd_tick() pour donner priorité aux écritures
  // Garde : on s'abstient si le ring buffer dépasse 50% pour éviter la contention SD
  if (sd_ok) MTP.loop();

  while (usbMIDI.read()) {
    byte cable = usbMIDI.getCable();
    byte type  = usbMIDI.getType();
    byte ch    = usbMIDI.getChannel();
    byte d1    = usbMIDI.getData1();
    byte d2    = usbMIDI.getData2();

    if (cable == USB_PORT_A) {                   // câble 1 — Entrée A
      switch (type) {
        case midi::NoteOn:            handleNoteOn(1, ch, d1, d2, SRC_PC); break;
        case midi::NoteOff:           handleNoteOff(1, ch, d1, d2, SRC_PC); break;
        case midi::ControlChange:     handleControlChange(1, ch, d1, d2, SRC_PC); break;
        case midi::ProgramChange:     handleProgramChange(1, ch, d1, SRC_PC); break;
        case midi::PitchBend:         processPitchBend(0, ch, (int)(((int)d2 << 7) | d1) - 8192, SRC_PC); break;
        case midi::AfterTouchChannel: processAfterTouch(0, ch, d1, SRC_PC); break;
      }
    } else if (cable == USB_PORT_B) {            // câble 2 — Entrée B
      switch (type) {
        case midi::NoteOn:            handleNoteOn(2, ch, d1, d2, SRC_PC); break;
        case midi::NoteOff:           handleNoteOff(2, ch, d1, d2, SRC_PC); break;
        case midi::ControlChange:     handleControlChange(2, ch, d1, d2, SRC_PC); break;
        case midi::ProgramChange:     handleProgramChange(2, ch, d1, SRC_PC); break;
        case midi::PitchBend:         processPitchBend(1, ch, (int)(((int)d2 << 7) | d1) - 8192, SRC_PC); break;
        case midi::AfterTouchChannel: processAfterTouch(1, ch, d1, SRC_PC); break;
      }
    } else if (cable == USB_IN_C) {               // câble 3 — Entrée C
      switch (type) {
        case midi::NoteOn:            handleNoteOn(3, ch, d1, d2, SRC_PC); break;
        case midi::NoteOff:           handleNoteOff(3, ch, d1, d2, SRC_PC); break;
        case midi::ControlChange:     handleControlChange(3, ch, d1, d2, SRC_PC); break;
        case midi::ProgramChange:     handleProgramChange(3, ch, d1, SRC_PC); break;
        case midi::PitchBend:         processPitchBend(2, ch, (int)(((int)d2 << 7) | d1) - 8192, SRC_PC); break;
        case midi::AfterTouchChannel: processAfterTouch(2, ch, d1, SRC_PC); break;
      }
    } else if (cable == USB_IN_D) {               // câble 4 — Entrée D
      switch (type) {
        case midi::NoteOn:            handleNoteOn(4, ch, d1, d2, SRC_PC); break;
        case midi::NoteOff:           handleNoteOff(4, ch, d1, d2, SRC_PC); break;
        case midi::ControlChange:     handleControlChange(4, ch, d1, d2, SRC_PC); break;
        case midi::ProgramChange:     handleProgramChange(4, ch, d1, SRC_PC); break;
        case midi::PitchBend:         processPitchBend(3, ch, (int)(((int)d2 << 7) | d1) - 8192, SRC_PC); break;
        case midi::AfterTouchChannel: processAfterTouch(3, ch, d1, SRC_PC); break;
      }
    } else if (cable >= USB_OUT_1 && cable <= USB_OUT_6) {  // câbles 6-11 — Sorties physiques 1-6
      switch (cable - (USB_OUT_1 - 1)) {
        case 1: MIDI1.send(type, d1, d2, ch); break;
        case 2: MIDI2.send(type, d1, d2, ch); break;
        case 3: MIDI3.send(type, d1, d2, ch); break;
        case 4: MIDI4.send(type, d1, d2, ch); break;
        case 5: MIDI5.send(type, d1, d2, ch); break;
        case 6: MIDI6.send(type, d1, d2, ch); break;
      }
    } else if (cable == USB_OUT_7) {              // câble 11 — Sortie 7 direct
      { MIDIDevice_BigBuffer* _d = _usb_out_dev(0); if (_d && (bool)*_d) switch (type) {
        case midi::NoteOn:            _d->sendNoteOn(d1, d2, ch); break;
        case midi::NoteOff:           _d->sendNoteOff(d1, d2, ch); break;
        case midi::ControlChange:     _d->sendControlChange(d1, d2, ch); break;
        case midi::ProgramChange:     _d->sendProgramChange(d1, ch); break;
        case midi::PitchBend:         _d->sendPitchBend((int)(((int)d2 << 7) | d1) - 8192, ch); break;
        case midi::AfterTouchChannel: _d->sendAfterTouch(d1, ch); break;
      } }
    } else if (cable == USB_OUT_8) {              // câble 12 — Sortie 8 direct
      { MIDIDevice_BigBuffer* _d = _usb_out_dev(1); if (_d && (bool)*_d) switch (type) {
        case midi::NoteOn:            _d->sendNoteOn(d1, d2, ch); break;
        case midi::NoteOff:           _d->sendNoteOff(d1, d2, ch); break;
        case midi::ControlChange:     _d->sendControlChange(d1, d2, ch); break;
        case midi::ProgramChange:     _d->sendProgramChange(d1, ch); break;
        case midi::PitchBend:         _d->sendPitchBend((int)(((int)d2 << 7) | d1) - 8192, ch); break;
        case midi::AfterTouchChannel: _d->sendAfterTouch(d1, ch); break;
      } }
    } else if (cable == USB_IN_E) {               // câble 13 — Entrée E
      switch (type) {
        case midi::NoteOn:            handleNoteOn(5, ch, d1, d2, SRC_PC); break;
        case midi::NoteOff:           handleNoteOff(5, ch, d1, d2, SRC_PC); break;
        case midi::ControlChange:     handleControlChange(5, ch, d1, d2, SRC_PC); break;
        case midi::ProgramChange:     handleProgramChange(5, ch, d1, SRC_PC); break;
        case midi::PitchBend:         processPitchBend(4, ch, (int)(((int)d2 << 7) | d1) - 8192, SRC_PC); break;
        case midi::AfterTouchChannel: processAfterTouch(4, ch, d1, SRC_PC); break;
      }
    } else if (cable == USB_OUT_9) {              // câble 14 — Sortie 9 direct
      { MIDIDevice_BigBuffer* _d = _usb_out_dev(2); if (_d && (bool)*_d) switch (type) {
        case midi::NoteOn:            _d->sendNoteOn(d1, d2, ch); break;
        case midi::NoteOff:           _d->sendNoteOff(d1, d2, ch); break;
        case midi::ControlChange:     _d->sendControlChange(d1, d2, ch); break;
        case midi::ProgramChange:     _d->sendProgramChange(d1, ch); break;
        case midi::PitchBend:         _d->sendPitchBend((int)(((int)d2 << 7) | d1) - 8192, ch); break;
        case midi::AfterTouchChannel: _d->sendAfterTouch(d1, ch); break;
      } }
    }
    // câble 0 : éditeur SysEx — déjà géré par le callback handleEditorSysex
  }








}