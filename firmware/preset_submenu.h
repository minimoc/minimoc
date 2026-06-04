#pragma once
// preset_submenu.h — Preset save/load sub-menu + presets d'usine

enum PresetSubState {
  PS_MAIN,
  PS_CONFIRM,
  PS_FACTORY,   // sélection d'un preset d'usine
};

static PresetSubState ps_state      = PS_MAIN;
static uint8_t        ps_cursor     = 0;
static uint8_t        ps_fac_cursor = 0;   // curseur dans la liste factory
static unsigned long  ps_confirm_at = 0;
static const char*    ps_confirm_msg = "";

static const char* PS_ITEMS[] = { "Preset:", "Sauvegarder", "Charger", "FACTORY" };
#define PS_ITEM_COUNT 4

#define PS_TITLE_Y  14
#define PS_LINE_Y   18
#define PS_ITEM0_Y  28
#define PS_ITEM_DY  12
#define PS_BOX_H    12

// ── Ecran principal ───────────────────────────────────────────────
void ps_draw_main() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, PS_TITLE_Y, "PRESETS");
  u8g2.drawHLine(0, PS_LINE_Y, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);

  for (uint8_t i = 0; i < PS_ITEM_COUNT; i++) {
    int y = PS_ITEM0_Y + i * PS_ITEM_DY;
    if (i == ps_cursor) {
      u8g2.drawBox(0, y - 9, SCREEN_W, PS_BOX_H);
      u8g2.setDrawColor(0);
    }
    if (i == 0) {
      char buf[24];
      bool exists = sd_preset_exists(current_preset);
      snprintf(buf, sizeof(buf), "Preset: %u%s", current_preset, exists ? " *" : "  ");
      u8g2.drawStr(4, y, buf);
    } else {
      u8g2.drawStr(4, y, PS_ITEMS[i]);
    }
    u8g2.setDrawColor(1);
  }

  if (!sd_ok) {
    u8g2.setFont(UI_FONT_SMALL);
    u8g2.drawStr(0, 63, "SD indisponible");
  }
  u8g2.sendBuffer();
}

// ── Ecran confirmation ────────────────────────────────────────────
void ps_draw_confirm() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  int w = u8g2.getStrWidth(ps_confirm_msg);
  u8g2.drawStr((SCREEN_W - w) / 2, 36, ps_confirm_msg);
  u8g2.sendBuffer();
}

// ── Ecran presets d'usine ─────────────────────────────────────────
void ps_draw_factory() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, "FACTORY");
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);

  for (uint8_t i = 0; i < FACTORY_COUNT; i++) {
    uint8_t y = 23 + i * 10;
    if (i == ps_fac_cursor) {
      u8g2.drawBox(0, y - 8, SCREEN_W, 10);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(4, y, FACTORY_PRESETS[i].name);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

// ── Handler principal ─────────────────────────────────────────────
bool ps_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
  if (ps_state == PS_CONFIRM) {
    if (millis() - ps_confirm_at > 1200) {
      ps_state = PS_MAIN;
      return true;   // auto-retour au carousel après confirmation
    }
    ps_draw_confirm();
    return false;
  }

  if (ps_state == PS_FACTORY) {
    if (enc_up   && ps_fac_cursor > 0)               ps_fac_cursor--;
    if (enc_down && ps_fac_cursor < FACTORY_COUNT-1)  ps_fac_cursor++;
    if (btn_valid) {
      apply_factory(ps_fac_cursor);
      ps_confirm_msg = FACTORY_PRESETS[ps_fac_cursor].name;
      ps_state = PS_CONFIRM; ps_confirm_at = millis();
    }
    if (btn_back) ps_state = PS_MAIN;
    if (ps_state == PS_FACTORY) ps_draw_factory(); else ps_draw_confirm();
    return false;
  }

  // PS_MAIN
  if (enc_up   && ps_cursor > 0)              ps_cursor--;
  if (enc_down && ps_cursor < PS_ITEM_COUNT-1) ps_cursor++;

  if (btn_valid) {
    if (ps_cursor == 0) {
      current_preset++;
      if (current_preset > MAX_PRESETS) current_preset = 1;
    } else if (ps_cursor == 1) {
      if (!sd_ok) { ps_confirm_msg = "Pas de SD!"; }
      else { ps_confirm_msg = silent_save_preset() ? "Sauvegarde OK!" : "Erreur SD!"; }
      ps_state = PS_CONFIRM; ps_confirm_at = millis();
    } else if (ps_cursor == 2) {
      if (!sd_ok) { ps_confirm_msg = "Pas de SD!"; }
      else if (!sd_preset_exists(current_preset)) { ps_confirm_msg = "Preset vide"; }
      else { ps_confirm_msg = silent_load_preset() ? "Charge OK!" : "Erreur SD!"; }
      ps_state = PS_CONFIRM; ps_confirm_at = millis();
    } else if (ps_cursor == 3) {
      ps_state = PS_FACTORY; ps_fac_cursor = 0;
    }
  }

  if (btn_back) return true;
  if      (ps_state == PS_CONFIRM) ps_draw_confirm();
  else if (ps_state == PS_FACTORY) ps_draw_factory();
  else                             ps_draw_main();
  return false;
}

void ps_enter() {
  ps_state    = PS_MAIN;
  ps_cursor   = 0;
}
