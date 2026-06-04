#pragma once
#include <Encoder.h>
#include <Bounce2.h>

// Encoder on pins 39/40, click button on 41, back button on 38
Encoder myEnc(40, 39);
Bounce  selectButton = Bounce();
long    oldPosition  = -999;

unsigned long buttonPressStartTime = 0;
const int     longPressDuration    = 1000;

bool modeInteractif = false;

const int PIN_BACK  = 38;
const int PIN_VALID = 41;

const long    DEBOUNCE_DELAY_MS  = 150;
unsigned long last_debounce_time = 0;

// Encoder: one step accepted per ENC_DEBOUNCE_MS regardless of pulse count
static const unsigned long ENC_DEBOUNCE_MS = 120;
static unsigned long       last_enc_time   = 0;

// Encodeur différé : l'event n'est appliqué qu'après ENC_APPLY_MS sans clic.
// Si un clic survient dans ce délai, l'event est annulé (rotation mécanique parasite).
static const unsigned long ENC_APPLY_MS = 60;
static int8_t    enc_pending    = 0;   // 0=rien  +1=down  -1=up
static uint32_t  enc_pending_ms = 0;

// -----------------------------------------------------------------
// TOP-LEVEL UI STATE
// -----------------------------------------------------------------

enum UIScreen {
  UI_CAROUSEL,   // main carousel selector
  UI_PRESETS,    // preset sub-menu
  UI_SYNC,       // sync sub-menu
  UI_ROUTING,    // routing sub-menu
  UI_MONITOR,    // moniteur VU-mètre temps réel
  UI_SYSTEM,     // system sub-menu (INFO + HOST CONFIG)
};

static UIScreen ui_screen = UI_CAROUSEL;

// -----------------------------------------------------------------
// INPUT SAMPLING — returns raw button state this tick
// -----------------------------------------------------------------

struct InputState {
  bool enc_up;
  bool enc_down;
  bool btn_valid;
  bool btn_back;
};

static InputState read_inputs() {
  InputState s = {false, false, false, false};

  long newPos = myEnc.read();
  long diff   = newPos - oldPosition;
  unsigned long now = millis();

  if (diff != 0) {
    oldPosition = newPos;  // toujours consommer pour éviter la dérive
    if (now - last_enc_time >= ENC_DEBOUNCE_MS) {
      enc_pending    = (diff > 0) ? 1 : -1;
      enc_pending_ms = now;
      last_enc_time  = now;
    }
  }

  // Bouton SELECT — si clic détecté, annuler la rotation en attente (artefact mécanique)
  selectButton.update();
  bool btn_fell = selectButton.fell();
  if (btn_fell) enc_pending = 0;

  // Appliquer la rotation différée si le délai est écoulé sans clic
  if (enc_pending != 0 && (now - enc_pending_ms) >= ENC_APPLY_MS) {
    s.enc_down = (enc_pending > 0);
    s.enc_up   = (enc_pending < 0);
    enc_pending = 0;
  }

  if ((now - last_debounce_time) >= DEBOUNCE_DELAY_MS) {
    if (btn_fell)  s.btn_valid = true;
    s.btn_back = (digitalRead(PIN_BACK) == LOW);
    if (s.btn_valid || s.btn_back) {
      last_debounce_time = now;
    }
  }
  return s;
}

// -----------------------------------------------------------------
// MAIN UI TICK — call every ~50 ms from loop()
// -----------------------------------------------------------------

void ui_tick() {
  InputState in = read_inputs();
  bool has_io   = in.enc_up || in.enc_down || in.btn_valid || in.btn_back;

  // Détecte un changement d'écran (transition via btn_valid ou btn_back au tick précédent)
  // pour forcer le premier dessin du nouvel écran.
  static UIScreen prev_screen = UI_CAROUSEL;
  bool screen_just_entered = (ui_screen != prev_screen);
  prev_screen = ui_screen;   // mis à jour AVANT le switch

  // Ne redessiner que si nécessaire pour éviter de bloquer le MIDI sur l'I2C (~20ms/sendBuffer).
  // MONITOR : animation VU-mètre → refresh permanent.
  // CAROUSEL avec glissement en cours → refresh jusqu'à la fin de l'animation.
  // Autres écrans : seulement sur input utilisateur ou à l'entrée du sous-menu.
  bool need_anim = (ui_screen == UI_MONITOR)
               || (ui_screen == UI_CAROUSEL && carousel_anim_offset != 0);

  // Laisser tourner pendant un long press en cours dans le menu ROUTAGE
  // (sinon le timer rs_valid_hold_ms n'est jamais vérifié quand le bouton est tenu)
  bool routing_lp = (ui_screen == UI_ROUTING)
                 && (rs_valid_hold_ms > 0)
                 && !rs_valid_long_done;

  if (!has_io && !need_anim && !screen_just_entered && !routing_lp) return;

  switch (ui_screen) {

    case UI_CAROUSEL: {
      bool validated = carousel_update(in.enc_up, in.enc_down, in.btn_valid);
      if (validated) {
        // Ordre carousel : PRESET / SYNC / ROUTAGE / MONITOR / SYSTEM
        switch (carousel_selected) {
          case 0: ps_enter();   ui_screen = UI_PRESETS;  break;
          case 1: sync_enter(); ui_screen = UI_SYNC;     break;
          case 2: rs_enter();   ui_screen = UI_ROUTING;  break;
          case 3: mon_enter();  ui_screen = UI_MONITOR;  break;
          case 4: sys_enter();  ui_screen = UI_SYSTEM;   break;
        }
      }
      break;
    }

    case UI_PRESETS: {
      bool done = ps_handle_input(in.enc_up, in.enc_down, in.btn_valid, in.btn_back);
      if (done) ui_screen = UI_CAROUSEL;
      break;
    }

    case UI_SYNC: {
      bool done = sync_handle_input(in.enc_up, in.enc_down, in.btn_valid, in.btn_back);
      if (done) ui_screen = UI_CAROUSEL;
      break;
    }

    case UI_ROUTING: {
      bool done = rs_handle_input(in.enc_up, in.enc_down, in.btn_valid, in.btn_back);
      if (done) ui_screen = UI_CAROUSEL;
      break;
    }

    case UI_MONITOR: {
      bool done = mon_handle_input(in.enc_up, in.enc_down, in.btn_valid, in.btn_back);
      if (done) ui_screen = UI_CAROUSEL;
      break;
    }

    case UI_SYSTEM: {
      bool done = sys_handle_input(in.enc_up, in.enc_down, in.btn_valid, in.btn_back);
      if (done) ui_screen = UI_CAROUSEL;
      break;
    }
  }
}
