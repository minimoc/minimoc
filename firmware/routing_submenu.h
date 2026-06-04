#pragma once
// routing_submenu.h — IHM de routage hybride BASIC + FLUX
//
// États :
//   RS_MAIN          : menu principal (MATRICE / FLUX / CH10)
//   RS_BASIC_GRID    : grille 5×9 interactive
//   RS_FLUX_LIST     : liste des flux + [+] nouveau
//   RS_FLUX_STEP1    : sélection ports d'entrée + canaux
//   RS_FLUX_STEP1_CH : picker canaux entrée (overlay)
//   RS_FLUX_STEP2    : sélection ports de sortie + canaux
//   RS_FLUX_STEP2_CH : picker canaux sortie (overlay)

enum RoutingSubState {
  RS_MAIN,
  RS_BASIC_GRID,
  RS_FLUX_LIST,
  RS_FLUX_ACTION,    // menu contextuel : Modifier / Supprimer
  RS_FLUX_STEP1,
  RS_FLUX_STEP1_CH,
  RS_FLUX_STEP2,
  RS_FLUX_STEP2_CH,
};

static RoutingSubState rs_state = RS_MAIN;

// ── Variables de navigation ───────────────────────────────────────
static uint8_t rs_cursor     = 0;   // RS_MAIN / RS_FLUX_LIST
static uint8_t rs_grid_pos   = 0;   // RS_BASIC_GRID : position 0-44
static uint8_t rs_flux_idx   = 0;   // index du flux en cours d'édition
static Flux    rs_flux_tmp;          // flux tampon avant validation
static uint8_t rs_step_cursor= 0;   // RS_FLUX_STEP1/2 : curseur port
static uint8_t rs_ch_port    = 0;   // port sélectionné pour le picker
static bool    rs_ch_is_out  = false; // picker input(false) ou output(true)
static uint8_t  rs_ch_cursor  = 0;   // curseur dans le picker 0-16 (16=TOUT)
static uint8_t  rs_action_cur = 0;   // RS_FLUX_ACTION : 0=Modifier 1=Supprimer
static uint8_t  rs_saved_port = 0xFF; // port sauvegardé avant toggle (long press recovery)
static uint16_t rs_saved_mask = 0;   // chan_mask sauvegardé


// Étiquettes
static const char* INPUT_LABELS[]  = { "A","B","C","D","E" };
static const char* OUTPUT_LABELS[] = { "1","2","3","4","5","6","7","8","9" };

// ── RS_MAIN ───────────────────────────────────────────────────────
static const char* RS_MAIN_ITEMS[] = { "MATRICE", "FLUX" };
#define RS_MAIN_COUNT 2

void rs_draw_main() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 14, "ROUTAGE");
  u8g2.drawHLine(0, 18, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);
  for (uint8_t i = 0; i < RS_MAIN_COUNT; i++) {
    int y = 36 + i * 14;
    char buf[20];
    if (i == 1) snprintf(buf, sizeof(buf), "FLUX  (%u)", flux_count);
    else        strncpy(buf, RS_MAIN_ITEMS[i], sizeof(buf));
    if (i == rs_cursor) { u8g2.drawBox(0,y-10,SCREEN_W,13); u8g2.setDrawColor(0); }
    u8g2.drawStr(8, y, buf);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

// ── RS_BASIC_GRID — grille 5×9 ────────────────────────────────────
// Layout : col_w=13px, left_w=11px → 11+9×13=128. row_h=9px, hdr_h=8px.
// Ligne header y=8, lignes data y=17,26,35,44,53.
#define BG_LEFT 11
#define BG_COL  13
#define BG_HDR_Y 8
#define BG_ROW0  17
#define BG_ROW_H 9

void rs_draw_basic_grid() {
  uint8_t cur_row = rs_grid_pos / 9;
  uint8_t cur_col = rs_grid_pos % 9;

  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_SMALL);

  // Header colonnes
  for (uint8_t j = 0; j < 9; j++) {
    uint8_t x = BG_LEFT + j * BG_COL + 4;
    u8g2.drawStr(x, BG_HDR_Y, OUTPUT_LABELS[j]);
  }

  // Lignes
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t y = BG_ROW0 + i * BG_ROW_H;
    u8g2.drawStr(0, y, INPUT_LABELS[i]);
    for (uint8_t j = 0; j < 9; j++) {
      uint8_t x = BG_LEFT + j * BG_COL;
      bool on  = (basic_matrix[i] >> j) & 1;
      bool sel = (i == cur_row && j == cur_col);
      if (sel) {
        u8g2.drawBox(x, y-7, BG_COL-1, 8);
        u8g2.setDrawColor(0);
        u8g2.drawStr(x+4, y, on ? "#" : ".");
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(x+4, y, on ? "#" : ".");
      }
    }
  }

  u8g2.sendBuffer();
}

// ── RS_FLUX_LIST ──────────────────────────────────────────────────
// Chaque ligne : "F1[*] A.1 B → 1.5 4"  (abrégé)
// Dernier item : "[+] Nouveau flux"
// rs_cursor : 0..flux_count = liste, flux_count = [+]
static uint8_t rs_flux_scroll = 0;  // premier flux visible

// Format : "A B:1 → 1 4:5 4:2c"
//   port seul     = tous canaux   (ex: "A")
//   port:n        = canal n fixe  (ex: "B:1")
//   port:nc       = n canaux      (ex: "A:3c")
//   sortie seule  = passthrough   (ex: "1")
//   sortie:n      = canal n fixe  (ex: "4:5")
//   sortie:nc     = n canaux      (ex: "4:2c")
static void _flux_summary(uint8_t f, char* buf, uint8_t maxlen) {
  char tmp[8]; uint8_t pos = 0;
  auto _app = [&](const char* s) {
    for (const char* c = s; *c && pos < maxlen-1; c++) buf[pos++] = *c;
  };
  // Entrées
  for (uint8_t ii = 0; ii < flux_list[f].n_in; ii++) {
    if (ii > 0 && pos < maxlen-1) buf[pos++] = ' ';
    buf[pos++] = 'A' + flux_list[f].in[ii].port;
    uint16_t m = flux_list[f].in[ii].chan_mask;
    if (m != 0) {
      int nc = __builtin_popcount(m);
      if (nc == 1) { snprintf(tmp,sizeof(tmp),":%u", __builtin_ctz(m)+1); }
      else         { snprintf(tmp,sizeof(tmp),":%dc", nc); }
      _app(tmp);
    }
  }
  // Flèche
  _app(" \x10 ");   // \x10 = → dans u8g2_font_5x7_tf
  // Sorties
  for (uint8_t oi = 0; oi < flux_list[f].n_out; oi++) {
    if (oi > 0 && pos < maxlen-1) buf[pos++] = ' ';
    snprintf(tmp, sizeof(tmp), "%u", flux_list[f].out[oi].port + 1);
    _app(tmp);
    uint16_t om = flux_list[f].out[oi].chan_mask;
    if (om != 0) {
      int nc = __builtin_popcount(om);
      if (nc == 1) { snprintf(tmp,sizeof(tmp),":%u", __builtin_ctz(om)+1); }
      else         { snprintf(tmp,sizeof(tmp),":%dc", nc); }
      _app(tmp);
    }
  }
  buf[pos] = '\0';
}

void rs_draw_flux_list() {
  uint8_t total = flux_count + 1;  // +1 pour [+]
  if (rs_cursor < rs_flux_scroll) rs_flux_scroll = rs_cursor;
  if (rs_cursor >= rs_flux_scroll + 4) rs_flux_scroll = rs_cursor - 3;

  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, "FLUX");
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_SMALL);

  for (uint8_t vi = 0; vi < 4; vi++) {
    uint8_t idx = rs_flux_scroll + vi;
    if (idx >= total) break;
    uint8_t y = 21 + vi * 10;
    bool sel = (idx == rs_cursor);
    if (sel) { u8g2.drawBox(0, y-7, SCREEN_W, 9); u8g2.setDrawColor(0); }
    char buf[26];
    if (idx == flux_count) {
      strncpy(buf, "[+] Nouveau flux", sizeof(buf));
    } else {
      char summ[20]; _flux_summary(idx, summ, sizeof(summ));
      snprintf(buf, sizeof(buf), "F%u - %s", idx+1, summ);
    }
    u8g2.drawStr(2, y, buf);
    u8g2.setDrawColor(1);
  }
  // Flèches scroll
  u8g2.setFont(UI_FONT_SMALL);
  if (rs_flux_scroll > 0)             u8g2.drawStr(120, 18, "^");
  if (rs_flux_scroll + 4 < total)     u8g2.drawStr(120, 62, "v");
  u8g2.sendBuffer();
}

// ── RS_FLUX_STEP1 / STEP2 — sélection ports ───────────────────────
// ports sélectionnés : stocker dans rs_flux_tmp
// Pour STEP1 : ports d'entrée (INPUT_LABELS), cursor 0-5 (5=→ SORTIES)
// Pour STEP2 : ports de sortie (OUTPUT_LABELS), cursor 0-9 (9=✓ OK)

static bool _step1_has_port(uint8_t p) {
  for (uint8_t i = 0; i < rs_flux_tmp.n_in; i++)
    if (rs_flux_tmp.in[i].port == p) return true;
  return false;
}
static bool _step2_has_port(uint8_t p) {
  for (uint8_t i = 0; i < rs_flux_tmp.n_out; i++)
    if (rs_flux_tmp.out[i].port == p) return true;
  return false;
}
static uint16_t _step1_chan(uint8_t p) {
  for (uint8_t i = 0; i < rs_flux_tmp.n_in; i++)
    if (rs_flux_tmp.in[i].port == p) return rs_flux_tmp.in[i].chan_mask;
  return 0;
}
static uint16_t _step2_chan(uint8_t p) {
  for (uint8_t i = 0; i < rs_flux_tmp.n_out; i++)
    if (rs_flux_tmp.out[i].port == p) return rs_flux_tmp.out[i].chan_mask;
  return 0;
}

// ── RS_FLUX_ACTION — menu contextuel Modifier / Supprimer ─────────
void rs_draw_flux_action() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  // Titre avec résumé du flux sélectionné
  char header[24]; snprintf(header, sizeof(header), "F%u", rs_flux_idx + 1);
  u8g2.drawStr(0, 14, header);
  u8g2.drawHLine(0, 18, SCREEN_W);
  // Résumé du flux en petit
  u8g2.setFont(UI_FONT_SMALL);
  char summ[26]; _flux_summary(rs_flux_idx, summ, sizeof(summ));
  u8g2.drawStr(0, 28, summ);
  // Options
  u8g2.setFont(UI_FONT_BODY);
  const char* opts[] = { "Modifier", "Supprimer" };
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t y = 42 + i * 14;
    if (rs_action_cur == i) {
      u8g2.drawBox(0, y - 10, SCREEN_W, 13);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(8, y, opts[i]);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

void rs_draw_step1() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, "FLUX \x10 ENTREES");  // \x10 = →
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);

  // 5 ports A-E dans la ligne + item "→ SORTIES"
  // Chaque port : box 20px wide, y centré
  for (uint8_t p = 0; p < 5; p++) {
    uint8_t x = 3 + p * 22;
    bool sel = (rs_step_cursor == p);
    bool on  = _step1_has_port(p);
    if (sel) { u8g2.drawRFrame(x-1, 28, 20, 20, 2); }
    if (on)  { u8g2.drawBox(x, 29, 18, 18); u8g2.setDrawColor(0); }
    u8g2.drawStr(x+5, 41, INPUT_LABELS[p]);
    u8g2.setDrawColor(1);
  }
  // Bouton "→ SORTIES" en bas de l'écran
  bool sel_arrow = (rs_step_cursor == 5);
  if (sel_arrow) { u8g2.drawBox(0, 54, SCREEN_W, 10); u8g2.setDrawColor(0); }
  else           { u8g2.drawFrame(0, 54, SCREEN_W, 10); }
  u8g2.setFont(UI_FONT_SMALL);
  uint8_t lw = u8g2.getStrWidth("\x10 SORTIES");
  u8g2.drawStr((SCREEN_W - lw) / 2, 62, "\x10 SORTIES");
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void rs_draw_step2() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, "FLUX \x11 SORTIES");
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);

  // 9 ports sur une ligne (curseur 0-8), OK en bas (curseur 9)
  for (uint8_t p = 0; p < 9; p++) {
    uint8_t x   = 3 + p * 13;
    bool sel = (rs_step_cursor == p);
    bool on  = _step2_has_port(p);
    if (sel) { u8g2.drawRFrame(x-1, 28, 12, 20, 2); }
    if (on)  { u8g2.drawBox(x, 29, 10, 18); u8g2.setDrawColor(0); }
    u8g2.drawStr(x+2, 41, OUTPUT_LABELS[p]);
    u8g2.setDrawColor(1);
  }

  // Bouton "✓ OK" en bas (même style que "→ SORTIES" dans STEP1)
  bool sel_ok = (rs_step_cursor == 9);
  if (sel_ok) { u8g2.drawBox(0, 54, SCREEN_W, 10); u8g2.setDrawColor(0); }
  else        { u8g2.drawFrame(0, 54, SCREEN_W, 10); }
  u8g2.setFont(UI_FONT_SMALL);
  uint8_t lw = u8g2.getStrWidth("\x08 VALIDER");
  u8g2.drawStr((SCREEN_W - lw) / 2, 62, "\x08 VALIDER");
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ── RS_FLUX_STEP1_CH / STEP2_CH — picker canaux ───────────────────
// Grille 4×4 canaux 1-16 + item [TOUT] en bas
// rs_ch_port = port concerné, rs_ch_is_out = false:in, true:out
void rs_draw_ch_picker() {
  // Layout : titre y=7, grille 4×4 (ch 1-16) y=9..52, TOUT/AUTO y=54..63
  // Fond noir (OLED par défaut), sélectionné = fond blanc / texte noir
  // Curseur non sélectionné = cadre blanc

  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_SMALL);

  // Titre
  char title[22];
  if (!rs_ch_is_out)
    snprintf(title, sizeof(title), "IN %s  canaux:", INPUT_LABELS[rs_ch_port]);
  else
    snprintf(title, sizeof(title), "OUT %u  canal dest:", rs_ch_port + 1);
  u8g2.drawStr(0, 7, title);

  // Récupérer la sélection courante
  uint16_t in_mask  = 0;
  uint16_t out_mask = 0;
  if (!rs_ch_is_out) {
    for (uint8_t ii = 0; ii < rs_flux_tmp.n_in; ii++)
      if (rs_flux_tmp.in[ii].port == rs_ch_port) { in_mask = rs_flux_tmp.in[ii].chan_mask; break; }
  } else {
    for (uint8_t oi = 0; oi < rs_flux_tmp.n_out; oi++)
      if (rs_flux_tmp.out[oi].port == rs_ch_port) { out_mask = rs_flux_tmp.out[oi].chan_mask; break; }
  }

  // Grille 4×4 : canaux 1-16 (positions 0-15)
  // Cellule : 32px large × 11px haut, baseline = y_top + 8
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t col  = i % 4;
    uint8_t row  = i / 4;
    uint8_t x    = col * 32;
    uint8_t ytop = 9 + row * 11;   // y haut de la cellule
    bool cur    = (rs_ch_cursor == i);
    bool active = rs_ch_is_out ? ((out_mask >> i) & 1)
                               : (in_mask != 0 && ((in_mask >> i) & 1));

    if (active) {
      u8g2.drawBox(x, ytop, 31, 10);
      u8g2.setDrawColor(0);    // noir pour le texte et le cadre sur fond blanc
    }
    // Cadre curseur toujours dessiné avec la couleur courante :
    //   case active  → couleur 0 (noir) sur fond blanc = bien visible
    //   case inactive → couleur 1 (blanc) sur fond noir = bien visible
    if (cur) {
      u8g2.drawFrame(x, ytop, 31, 10);
    }

    char buf[4]; snprintf(buf, sizeof(buf), "%u", i + 1);
    uint8_t tw = u8g2.getStrWidth(buf);
    u8g2.drawStr(x + (31 - tw) / 2, ytop + 8, buf);
    u8g2.setDrawColor(1);
  }

  u8g2.sendBuffer();
}

// ── HANDLERS ─────────────────────────────────────────────────────

// Helpers step1 : toggle port in/out of rs_flux_tmp
static void _toggle_step1_port(uint8_t p) {
  for (uint8_t i = 0; i < rs_flux_tmp.n_in; i++) {
    if (rs_flux_tmp.in[i].port == p) {
      // Supprimer
      for (uint8_t j = i; j < rs_flux_tmp.n_in-1; j++) rs_flux_tmp.in[j] = rs_flux_tmp.in[j+1];
      rs_flux_tmp.n_in--; return;
    }
  }
  if (rs_flux_tmp.n_in < FLUX_MAX_IN) {
    rs_flux_tmp.in[rs_flux_tmp.n_in++] = {p, 0};
  }
}
static void _toggle_step2_port(uint8_t p) {
  for (uint8_t i = 0; i < rs_flux_tmp.n_out; i++) {
    if (rs_flux_tmp.out[i].port == p) {
      for (uint8_t j = i; j < rs_flux_tmp.n_out-1; j++) rs_flux_tmp.out[j] = rs_flux_tmp.out[j+1];
      rs_flux_tmp.n_out--; return;
    }
  }
  if (rs_flux_tmp.n_out < FLUX_MAX_OUT) {
    rs_flux_tmp.out[rs_flux_tmp.n_out++] = {p, 0};
  }
}

// Long press detection pour RS_FLUX_STEP1/STEP2
static unsigned long rs_valid_hold_ms = 0;
static bool          rs_valid_long_done = false;

static void rs_draw_current();  // forward déclaration

bool rs_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {

  // Détection appui long sur VALID (channel picker Flux ou réglage pas grille)
  unsigned long now = millis();
  // Tracker le long press uniquement dans STEP1/STEP2 (là où il fait sens).
  // Tout autre btn_valid (MAIN, ACTION, LIST, pickers) ne démarre pas le timer.
  if (btn_valid &&
      (rs_state == RS_FLUX_STEP1 || rs_state == RS_FLUX_STEP2)) {
    rs_valid_hold_ms  = now;
    rs_valid_long_done = false;
  }
  bool long_press = false;
  if (rs_valid_hold_ms > 0 && !rs_valid_long_done &&
      (now - rs_valid_hold_ms) > 700 &&
      (rs_state == RS_FLUX_STEP1 || rs_state == RS_FLUX_STEP2)) {
    long_press = true;
    rs_valid_long_done = true;
    btn_valid = false;
  }

  switch (rs_state) {

    // ── MAIN ─────────────────────────────────────────────────────
    case RS_MAIN:
      if (enc_up   && rs_cursor > 0) rs_cursor--;
      if (enc_down && rs_cursor < RS_MAIN_COUNT-1) rs_cursor++;
      if (btn_valid) {
        if (rs_cursor == 0) { rs_state = RS_BASIC_GRID; rs_grid_pos = 0; }
        else                { rs_state = RS_FLUX_LIST; rs_cursor = 0; rs_flux_scroll = 0; }
      }
      if (btn_back) return true;
      break;

    // ── BASIC GRID ───────────────────────────────────────────────
    case RS_BASIC_GRID:
      if (enc_down && rs_grid_pos < 44) rs_grid_pos++;
      if (enc_up   && rs_grid_pos > 0 ) rs_grid_pos--;
      if (btn_valid) {
        uint8_t i = rs_grid_pos / 9, j = rs_grid_pos % 9;
        if (basic_matrix[i] & (1u << j)) basic_matrix[i] &= ~(uint16_t)(1u << j);
        else                              basic_matrix[i] |=  (uint16_t)(1u << j);
        recompute_route_matrix();
      }
      if (btn_back) { rs_state = RS_MAIN; rs_cursor = 0; }
      break;

    // ── FLUX LIST ────────────────────────────────────────────────
    case RS_FLUX_LIST: {
      uint8_t total = flux_count + 1;
      if (enc_up   && rs_cursor > 0)       rs_cursor--;
      if (enc_down && rs_cursor < total-1)  rs_cursor++;
      if (btn_valid) {
        if (rs_cursor == flux_count) {
          // Nouveau flux
          memset(&rs_flux_tmp, 0, sizeof(rs_flux_tmp));
          rs_flux_tmp.active = true;
          rs_flux_idx        = flux_count;
          rs_step_cursor     = 0;
          rs_valid_hold_ms   = 0;
          rs_valid_long_done = true;
          rs_state = RS_FLUX_STEP1;
        } else {
          // Ouvrir le menu d'action Modifier / Supprimer
          rs_flux_idx   = rs_cursor;
          rs_action_cur = 0;
          rs_state = RS_FLUX_ACTION;
        }
      }
      if (btn_back) { rs_state = RS_MAIN; rs_cursor = 1; }
      break;
    }

    // ── FLUX ACTION — Modifier / Supprimer ───────────────────────
    case RS_FLUX_ACTION:
      if (enc_up   && rs_action_cur > 0) rs_action_cur--;
      if (enc_down && rs_action_cur < 1) rs_action_cur++;
      if (btn_valid) {
        if (rs_action_cur == 0) {
          rs_flux_tmp      = flux_list[rs_flux_idx];
          rs_step_cursor   = 0;
          rs_valid_hold_ms  = 0;     // effacer tout timer résiduel
          rs_valid_long_done = true;
          rs_state = RS_FLUX_STEP1;
        } else {
          // Supprimer : décaler le tableau et revenir à la liste
          for (uint8_t i = rs_flux_idx; i < flux_count - 1; i++)
            flux_list[i] = flux_list[i + 1];
          flux_count--;
          recompute_route_matrix();
          if (rs_cursor >= flux_count) rs_cursor = flux_count;
          rs_state = RS_FLUX_LIST;
        }
      }
      if (btn_back) { rs_state = RS_FLUX_LIST; }
      break;

    // ── FLUX STEP 1 — entrées ────────────────────────────────────
    case RS_FLUX_STEP1:
      if (enc_up   && rs_step_cursor > 0) rs_step_cursor--;
      if (enc_down && rs_step_cursor < 5) rs_step_cursor++;
      if (btn_valid) {
        if (rs_step_cursor == 5) {
          rs_valid_hold_ms   = 0;     // éviter déclenchement parasite à l'entrée dans STEP2
          rs_valid_long_done = true;
          rs_state = RS_FLUX_STEP2;
          rs_step_cursor = 0;
        } else {
          // Sauvegarder le chan_mask avant suppression (pour restauration si long press)
          rs_saved_port = rs_step_cursor;
          rs_saved_mask = _step1_chan(rs_step_cursor);
          _toggle_step1_port(rs_step_cursor);
        }
      }
      if (long_press && rs_step_cursor < 5) {
        // Restaurer le port avec son chan_mask d'origine s'il a été retiré à T=0
        if (!_step1_has_port(rs_step_cursor)) {
          uint16_t mask = (rs_saved_port == rs_step_cursor) ? rs_saved_mask : 0;
          if (rs_flux_tmp.n_in < FLUX_MAX_IN)
            rs_flux_tmp.in[rs_flux_tmp.n_in++] = {rs_step_cursor, mask};
        }
        rs_saved_port = 0xFF;
        rs_ch_port   = rs_step_cursor;
        rs_ch_is_out = false;
        rs_ch_cursor = 0;
        rs_state = RS_FLUX_STEP1_CH;
      }
      if (btn_back) { rs_state = RS_FLUX_LIST; rs_cursor = rs_flux_idx; }
      break;

    // ── FLUX STEP 1 CH — picker canaux entrée ───────────────────
    case RS_FLUX_STEP1_CH:
      if (enc_up   && rs_ch_cursor > 0)  rs_ch_cursor--;
      if (enc_down && rs_ch_cursor < 15) rs_ch_cursor++;
      if (btn_valid) {
        for (uint8_t i = 0; i < rs_flux_tmp.n_in; i++) {
          if (rs_flux_tmp.in[i].port != rs_ch_port) continue;
          uint16_t bit = (uint16_t)(1u << rs_ch_cursor);
          rs_flux_tmp.in[i].chan_mask ^= bit;
          break;
        }
      }
      if (btn_back) {
        rs_valid_hold_ms   = 0;
        rs_valid_long_done = true;
        rs_state = RS_FLUX_STEP1;
      }
      break;

    // ── FLUX STEP 2 — sorties ────────────────────────────────────
    case RS_FLUX_STEP2:
      if (enc_up   && rs_step_cursor > 0) rs_step_cursor--;
      if (enc_down && rs_step_cursor < 9) rs_step_cursor++;
      if (btn_valid) {
        if (rs_step_cursor == 9) {
          // ✓ Valider le flux
          if (rs_flux_idx >= flux_count) flux_count++;
          flux_list[rs_flux_idx] = rs_flux_tmp;
          recompute_route_matrix();
          rs_state = RS_FLUX_LIST;
          rs_cursor = rs_flux_idx;
        } else {
          // Sauvegarder le chan_mask avant suppression (pour restauration si long press)
          rs_saved_port = rs_step_cursor;
          rs_saved_mask = _step2_chan(rs_step_cursor);
          _toggle_step2_port(rs_step_cursor);
        }
      }
      if (long_press && rs_step_cursor < 9) {
        // Restaurer le port avec son chan_mask d'origine s'il a été retiré à T=0
        if (!_step2_has_port(rs_step_cursor)) {
          uint16_t mask = (rs_saved_port == rs_step_cursor) ? rs_saved_mask : 0;
          if (rs_flux_tmp.n_out < FLUX_MAX_OUT)
            rs_flux_tmp.out[rs_flux_tmp.n_out++] = {rs_step_cursor, mask};
        }
        rs_saved_port = 0xFF;
        rs_ch_port   = rs_step_cursor;
        rs_ch_is_out = true;
        rs_ch_cursor = 0;
        rs_state = RS_FLUX_STEP2_CH;
      }
      if (btn_back) { rs_state = RS_FLUX_STEP1; rs_step_cursor = 0; }
      break;

    // ── FLUX STEP 2 CH — picker canal sortie ─────────────────────
    case RS_FLUX_STEP2_CH:
      if (enc_up   && rs_ch_cursor > 0)  rs_ch_cursor--;
      if (enc_down && rs_ch_cursor < 15) rs_ch_cursor++;
      if (btn_valid) {
        for (uint8_t i = 0; i < rs_flux_tmp.n_out; i++) {
          if (rs_flux_tmp.out[i].port != rs_ch_port) continue;
          rs_flux_tmp.out[i].chan_mask ^= (uint16_t)(1u << rs_ch_cursor);
          break;
        }
      }
      if (btn_back) {
        rs_valid_hold_ms   = 0;
        rs_valid_long_done = true;
        rs_state = RS_FLUX_STEP2;
      }
      break;

  }

  rs_draw_current();
  return false;
}

static void rs_draw_current() {
  switch (rs_state) {
    case RS_MAIN:         rs_draw_main();      break;
    case RS_BASIC_GRID:   rs_draw_basic_grid();break;
    case RS_FLUX_LIST:    rs_draw_flux_list();   break;
    case RS_FLUX_ACTION:  rs_draw_flux_action(); break;
    case RS_FLUX_STEP1:   rs_draw_step1();     break;
    case RS_FLUX_STEP1_CH:rs_draw_ch_picker(); break;
    case RS_FLUX_STEP2:   rs_draw_step2();     break;
    case RS_FLUX_STEP2_CH:rs_draw_ch_picker(); break;
  }
}

void rs_enter() {
  rs_state      = RS_MAIN;
  rs_cursor     = 0;
  rs_grid_pos   = 0;
  rs_flux_scroll= 0;
  rs_step_cursor= 0;
  rs_valid_hold_ms   = 0;
  rs_valid_long_done = false;
}
