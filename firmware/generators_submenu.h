#pragma once
// generators_submenu.h — IHM des générateurs (LFO + séquenceur euclidien)
//
// Calquée sur routing_submenu.h (mêmes conventions : état, buffer temporaire,
// écrans u8g2). Un générateur n'a pas d'entrée : le wizard n'a donc qu'une
// étape "sorties" (identique à STEP2/STEP2_CH de routing_submenu.h) au lieu
// des deux (entrées + sorties) d'un Flux. GS_SYNC_MODE/GS_RATE/GS_DIVISION
// et GS_OUT/GS_OUT_CH sont partagés par les deux types de générateur ; le
// reste du chemin diverge selon GS_TYPE (voir Generator.type, logic.h).
//
// États :
//   GS_LIST         : liste des générateurs (max GEN_MAX) + [+] nouveau
//   GS_ACTION       : menu contextuel Modifier / Supprimer
//   GS_TYPE         : choix LFO / Euclidien
//   GS_WAVEFORM     : choix de la forme d'onde (LFO uniquement)
//   GS_SYNC_MODE    : Libre (Hz) / Synchro (horloge MIDI) — partagé
//   GS_RATE         : réglage fréquence, mode libre — partagé
//   GS_DIVISION     : réglage division, mode synchro — partagé
//   GS_DEPTH        : amplitude (0-63) — LFO
//   GS_CENTER       : valeur centrale (0-127) — LFO
//   GS_CC_NUMBER    : numéro de CC ciblé (0-127) — LFO
//   GS_NOTE         : note MIDI fixe (0-127) — EUCLID
//   GS_STEPS        : nb de pas (1-16) — EUCLID
//   GS_PULSES       : nb de hits (0-steps) — EUCLID
//   GS_ROTATION     : rotation du motif (0-steps-1) — EUCLID
//   GS_GATE_PERCENT : durée du gate en % du pas (1-100) — EUCLID
//   GS_OUT          : sélection ports de sortie + canaux — partagé
//   GS_OUT_CH       : picker canaux sortie (overlay) — partagé

enum GsSubState {
  GS_LIST,
  GS_ACTION,
  GS_TYPE,
  GS_WAVEFORM,
  GS_SYNC_MODE,
  GS_RATE,
  GS_DIVISION,
  GS_DEPTH,
  GS_CENTER,
  GS_CC_NUMBER,
  GS_NOTE,
  GS_STEPS,
  GS_PULSES,
  GS_ROTATION,
  GS_GATE_PERCENT,
  GS_OUT,
  GS_OUT_CH,
};

static GsSubState gs_state = GS_LIST;

static uint8_t   gs_cursor     = 0;   // GS_LIST
static uint8_t   gs_gen_idx    = 0;   // index du générateur en cours d'édition
static Generator gs_gen_tmp;          // générateur tampon avant validation
static uint8_t   gs_step_cursor= 0;   // curseur générique (WAVEFORM/SYNC_MODE/DIVISION/OUT)
static uint8_t   gs_action_cur = 0;   // GS_ACTION : 0=Modifier 1=Supprimer
static uint8_t   gs_ch_port    = 0;   // GS_OUT_CH : port concerné
static uint8_t   gs_ch_cursor  = 0;   // GS_OUT_CH : curseur 0-15

static const char* GS_WAVEFORM_ITEMS[] = { "Triangle", "Sinus", "Carre", "Scie", "Aleatoire" };
static const char* GS_WAVEFORM_SHORT[] = { "Tri", "Sin", "Car", "Sci", "Ale" };
static const char* GS_SYNC_ITEMS[]     = { "Libre (Hz)", "Synchro" };
static const char* GS_TYPE_ITEMS[]     = { "LFO", "Euclidien" };

// Accélération de l'encodeur sur les réglages numériques (RATE/DEPTH/CENTER/
// CC_NUMBER) — même mécanisme que le réglage BPM (transport_submenu.h) : pas
// large si les crans s'enchaînent vite, pas de 1 sinon. Un seul écran
// numérique actif à la fois (machine à états gs_state), donc une seule
// variable de dernier-cran suffit pour les quatre.
#define GS_STEP_ACCEL_MS 180u   // même seuil que TRANSPORT_BPM_ACCEL_MS
static uint32_t gs_last_enc_ms = 0;

static uint16_t _gs_step(uint16_t normal, uint16_t fast) {
  uint32_t now = millis();
  uint16_t step = (now - gs_last_enc_ms < GS_STEP_ACCEL_MS) ? fast : normal;
  gs_last_enc_ms = now;
  return step;
}

// ── Résumé compact d'un générateur (liste) ────────────────────────
// Format LFO   : "Tri 1.0Hz CC1 -> 1 4"  /  "Sin 1/4 CC74 -> 2"
// Format EUCLID: "Euc 3/8 1/4 N60 -> 3:1"
static void _gen_summary(uint8_t idx, char* buf, uint8_t maxlen) {
  Generator &g = gen_list[idx];
  char tmp[12]; uint8_t pos = 0;
  auto _app = [&](const char* s) {
    for (const char* c = s; *c && pos < maxlen-1; c++) buf[pos++] = *c;
  };
  if (g.sync_mode == LFO_SYNC_FREE) {
    snprintf(tmp, sizeof(tmp), "%u.%uHz", g.rate_x10hz/10, g.rate_x10hz%10);
  } else {
    snprintf(tmp, sizeof(tmp), "%s", LFO_DIVISIONS[g.division_idx].label);
  }
  if (g.type == GEN_LFO) {
    _app(GS_WAVEFORM_SHORT[g.waveform]);
    _app(" ");
    _app(tmp);
    snprintf(tmp, sizeof(tmp), " CC%u", g.cc_number);
    _app(tmp);
  } else {
    _app("Euc ");
    snprintf(tmp, sizeof(tmp), "%u/%u", g.euclid_pulses, g.euclid_steps);
    _app(tmp);
    if (g.sync_mode == LFO_SYNC_FREE) snprintf(tmp, sizeof(tmp), "%u.%uHz", g.rate_x10hz/10, g.rate_x10hz%10);
    else                              snprintf(tmp, sizeof(tmp), "%s", LFO_DIVISIONS[g.division_idx].label);
    _app(" "); _app(tmp);
    snprintf(tmp, sizeof(tmp), " N%u", g.note);
    _app(tmp);
  }
  _app(" \x10 ");   // \x10 = →
  for (uint8_t oi = 0; oi < g.n_out; oi++) {
    if (oi > 0 && pos < maxlen-1) buf[pos++] = ' ';
    snprintf(tmp, sizeof(tmp), "%u", g.out[oi].port + 1);
    _app(tmp);
    // Canal(aux) — contrairement à un Flux, un générateur n'a pas de
    // passthrough : chan_mask==0 signifie "rien n'est envoyé sur cette
    // sortie", donc toujours affiché explicitement pour rendre le problème
    // visible depuis la liste (cf. gs_draw_out()/GS_OUT_CH pour le fix associé).
    uint16_t m = g.out[oi].chan_mask;
    int nc = __builtin_popcount(m);
    if (nc == 0)      { _app(":0c"); }
    else if (nc == 1) { snprintf(tmp,sizeof(tmp),":%u", __builtin_ctz(m)+1); _app(tmp); }
    else              { snprintf(tmp,sizeof(tmp),":%dc", nc); _app(tmp); }
  }
  buf[pos] = '\0';
}

// ── GS_LIST ────────────────────────────────────────────────────────
static uint8_t gs_list_scroll = 0;

void gs_draw_list() {
  bool can_add = (gen_count < GEN_MAX);
  uint8_t total = gen_count + (can_add ? 1 : 0);
  if (gs_cursor < gs_list_scroll) gs_list_scroll = gs_cursor;
  if (gs_cursor >= gs_list_scroll + 4) gs_list_scroll = gs_cursor - 3;

  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, "GENERATEURS");
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_SMALL);

  for (uint8_t vi = 0; vi < 4; vi++) {
    uint8_t idx = gs_list_scroll + vi;
    if (idx >= total) break;
    uint8_t y = 21 + vi * 10;
    bool sel = (idx == gs_cursor);
    if (sel) { u8g2.drawBox(0, y-7, SCREEN_W, 9); u8g2.setDrawColor(0); }
    char buf[32];
    if (idx == gen_count) {
      strncpy(buf, "[+] Nouveau", sizeof(buf));
    } else {
      char summ[20]; _gen_summary(idx, summ, sizeof(summ));
      snprintf(buf, sizeof(buf), "L%u %s%s", idx+1,
               gen_list[idx].active ? "" : "(off) ", summ);
    }
    u8g2.drawStr(2, y, buf);
    u8g2.setDrawColor(1);
  }
  if (gs_list_scroll > 0)          u8g2.drawStr(120, 18, "^");
  if (gs_list_scroll + 4 < total)  u8g2.drawStr(120, 62, "v");
  u8g2.sendBuffer();
}

// ── GS_ACTION ──────────────────────────────────────────────────────
void gs_draw_action() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  char header[8]; snprintf(header, sizeof(header), "L%u", gs_gen_idx + 1);
  u8g2.drawStr(0, 14, header);
  u8g2.drawHLine(0, 18, SCREEN_W);
  u8g2.setFont(UI_FONT_SMALL);
  char summ[26]; _gen_summary(gs_gen_idx, summ, sizeof(summ));
  u8g2.drawStr(0, 28, summ);
  u8g2.setFont(UI_FONT_BODY);
  const char* opts[] = { "Modifier", "Supprimer" };
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t y = 42 + i * 14;
    if (gs_action_cur == i) { u8g2.drawBox(0, y-10, SCREEN_W, 13); u8g2.setDrawColor(0); }
    u8g2.drawStr(8, y, opts[i]);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

// ── Écran générique "liste d'items" (WAVEFORM / SYNC_MODE / DIVISION) ──
static void _gs_draw_item_list(const char* title, const char* const* items, uint8_t count) {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);
  for (uint8_t i = 0; i < count; i++) {
    uint8_t y = 22 + i * 10;
    if (i == gs_step_cursor) { u8g2.drawBox(0, y-8, SCREEN_W, 10); u8g2.setDrawColor(0); }
    u8g2.drawStr(8, y, items[i]);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

void gs_draw_type()      { _gs_draw_item_list("TYPE", GS_TYPE_ITEMS, GEN_TYPE_COUNT); }
void gs_draw_waveform()  { _gs_draw_item_list("FORME D'ONDE", GS_WAVEFORM_ITEMS, LFO_WAVEFORM_COUNT); }
void gs_draw_sync_mode() { _gs_draw_item_list("SYNCHRO", GS_SYNC_ITEMS, 2); }

void gs_draw_division() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, "DIVISION");
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);
  for (uint8_t i = 0; i < LFO_DIVISION_COUNT; i++) {
    uint8_t y = 22 + i * 6;
    if (i == gs_step_cursor) { u8g2.drawBox(0, y-6, SCREEN_W, 7); u8g2.setDrawColor(0); }
    u8g2.setFont(UI_FONT_SMALL);
    u8g2.drawStr(8, y, LFO_DIVISIONS[i].label);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

// ── Écran générique "valeur numérique centrée" (RATE/DEPTH/CENTER/CC) ──
static void _gs_draw_numeric(const char* title, const char* val, const char* hint) {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, SCREEN_W);

  u8g2.setFont(UI_FONT_BODY);
  uint8_t vw = u8g2.getStrWidth(val);
  u8g2.drawStr((SCREEN_W - vw) / 2, 36, val);

  if (hint) {
    u8g2.setFont(UI_FONT_SMALL);
    uint8_t hw = u8g2.getStrWidth(hint);
    u8g2.drawStr((SCREEN_W - hw) / 2, 47, hint);
  }

  u8g2.drawBox(0, 54, SCREEN_W, 10);
  u8g2.setDrawColor(0);
  u8g2.setFont(UI_FONT_SMALL);
  uint8_t lw = u8g2.getStrWidth("\x08 VALIDER");
  u8g2.drawStr((SCREEN_W - lw) / 2, 62, "\x08 VALIDER");
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void gs_draw_rate() {
  char val[12]; snprintf(val, sizeof(val), "%u.%u Hz", gs_gen_tmp.rate_x10hz/10, gs_gen_tmp.rate_x10hz%10);
  _gs_draw_numeric("FREQUENCE", val, "0.1 .. 50.0 Hz");
}
void gs_draw_depth() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.depth);
  _gs_draw_numeric("AMPLITUDE", val, "0 .. 63");
}
void gs_draw_center() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.center);
  _gs_draw_numeric("VALEUR CENTRALE", val, "0 .. 127");
}
void gs_draw_cc_number() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.cc_number);
  _gs_draw_numeric("NUMERO CC", val, "0 .. 127");
}
void gs_draw_note() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.note);
  _gs_draw_numeric("NOTE", val, "0 .. 127");
}
void gs_draw_steps() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.euclid_steps);
  char hint[16]; snprintf(hint, sizeof(hint), "1 .. %u", EUCLID_STEPS_MAX);
  _gs_draw_numeric("PAS", val, hint);
}
void gs_draw_pulses() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.euclid_pulses);
  char hint[16]; snprintf(hint, sizeof(hint), "0 .. %u", gs_gen_tmp.euclid_steps);
  _gs_draw_numeric("PULSES", val, hint);
}
void gs_draw_rotation() {
  char val[8]; snprintf(val, sizeof(val), "%u", gs_gen_tmp.euclid_rotation);
  uint8_t rot_max = (gs_gen_tmp.euclid_steps > 0) ? (uint8_t)(gs_gen_tmp.euclid_steps - 1) : 0;
  char hint[16]; snprintf(hint, sizeof(hint), "0 .. %u", rot_max);
  _gs_draw_numeric("ROTATION", val, hint);
}
void gs_draw_gate_percent() {
  char val[8]; snprintf(val, sizeof(val), "%u%%", gs_gen_tmp.gate_percent);
  _gs_draw_numeric("GATE", val, "1 .. 100 %");
}

// ── GS_OUT / GS_OUT_CH — sélection sorties (identique à routing_submenu.h STEP2) ──
static bool _gs_out_has_port(uint8_t p) {
  for (uint8_t i = 0; i < gs_gen_tmp.n_out; i++)
    if (gs_gen_tmp.out[i].port == p) return true;
  return false;
}
// Port réellement actif = présent dans la liste ET avec au moins un canal
// sélectionné. Distinct de _gs_out_has_port() (utilisé pour l'ajout/retrait
// de l'entrée elle-même) : une entrée avec chan_mask==0 ne doit PAS
// apparaître comme "active" à l'écran, sinon l'utilisateur ne peut pas voir
// qu'aucun CC ne sera envoyé sur ce port (cf. GS_OUT_CH plus bas).
static bool _gs_out_port_sends(uint8_t p) {
  for (uint8_t i = 0; i < gs_gen_tmp.n_out; i++)
    if (gs_gen_tmp.out[i].port == p) return gs_gen_tmp.out[i].chan_mask != 0;
  return false;
}
// Ajout/retrait d'un port de sortie — même comportement que
// _toggle_step2_port() (routing_submenu.h) : un nouveau port démarre sans
// canal présélectionné (chan_mask=0), le picker GS_OUT_CH s'ouvre vide.
// Contrairement à un Flux, chan_mask=0 n'a pas de sens final ici (pas de
// canal source à "passthrough" : 0 canal = rien envoyé) — mais plutôt que de
// contraindre l'édition du masque, on élague les entrées encore à 0 quand on
// quitte le picker (cf. GS_OUT_CH / btn_back plus bas), exactement comme un
// port qu'on n'aurait jamais vraiment configuré.
static void _gs_toggle_out_port(uint8_t p) {
  for (uint8_t i = 0; i < gs_gen_tmp.n_out; i++) {
    if (gs_gen_tmp.out[i].port == p) {
      for (uint8_t j = i; j < gs_gen_tmp.n_out-1; j++) gs_gen_tmp.out[j] = gs_gen_tmp.out[j+1];
      gs_gen_tmp.n_out--; return;
    }
  }
  if (gs_gen_tmp.n_out < FLUX_MAX_OUT) {
    gs_gen_tmp.out[gs_gen_tmp.n_out++] = {p, (uint16_t)0u};
  }
}
// Retire de gs_gen_tmp.out[] toute entrée sans canal sélectionné (chan_mask
// ==0) — appelé en quittant GS_OUT_CH (cf. plus bas), pour qu'un port jamais
// vraiment configuré (aucun canal coché) disparaisse plutôt que de rester
// silencieusement inactif dans la liste.
static void _gs_prune_empty_outs() {
  for (uint8_t i = 0; i < gs_gen_tmp.n_out; ) {
    if (gs_gen_tmp.out[i].chan_mask == 0) {
      for (uint8_t j = i; j < gs_gen_tmp.n_out-1; j++) gs_gen_tmp.out[j] = gs_gen_tmp.out[j+1];
      gs_gen_tmp.n_out--;
    } else {
      i++;
    }
  }
}

void gs_draw_out() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_TITLE);
  char title[20];
  snprintf(title, sizeof(title), "%s \x11 SORTIES", gs_gen_tmp.type == GEN_LFO ? "LFO" : "EUC");
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, SCREEN_W);
  u8g2.setFont(UI_FONT_BODY);

  for (uint8_t p = 0; p < 9; p++) {
    uint8_t x   = 3 + p * 13;
    bool sel = (gs_step_cursor == p);
    bool on  = _gs_out_port_sends(p);   // pas _gs_out_has_port() : une entrée à 0 canal ne doit pas paraître active
    if (sel) { u8g2.drawRFrame(x-1, 28, 12, 20, 2); }
    if (on)  { u8g2.drawBox(x, 29, 10, 18); u8g2.setDrawColor(0); }
    u8g2.drawStr(x+2, 41, OUTPUT_LABELS[p]);
    u8g2.setDrawColor(1);
  }

  bool sel_ok = (gs_step_cursor == 9);
  if (sel_ok) { u8g2.drawBox(0, 54, SCREEN_W, 10); u8g2.setDrawColor(0); }
  else        { u8g2.drawFrame(0, 54, SCREEN_W, 10); }
  u8g2.setFont(UI_FONT_SMALL);
  uint8_t lw = u8g2.getStrWidth("\x08 VALIDER");
  u8g2.drawStr((SCREEN_W - lw) / 2, 62, "\x08 VALIDER");
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// Picker canaux — mise en page identique à rs_draw_ch_picker() (routing_submenu.h)
void gs_draw_out_ch() {
  u8g2.clearBuffer();
  u8g2.setFont(UI_FONT_SMALL);

  char title[22];
  snprintf(title, sizeof(title), "OUT %u  canal(aux):", gs_ch_port + 1);
  u8g2.drawStr(0, 7, title);

  uint16_t mask = 0;
  for (uint8_t oi = 0; oi < gs_gen_tmp.n_out; oi++)
    if (gs_gen_tmp.out[oi].port == gs_ch_port) { mask = gs_gen_tmp.out[oi].chan_mask; break; }

  for (uint8_t i = 0; i < 16; i++) {
    uint8_t col  = i % 4;
    uint8_t row  = i / 4;
    uint8_t x    = col * 32;
    uint8_t ytop = 9 + row * 11;
    bool cur    = (gs_ch_cursor == i);
    bool active = (mask >> i) & 1;

    if (active) {
      u8g2.drawBox(x, ytop, 31, 10);
      u8g2.setDrawColor(0);
    }
    if (cur) u8g2.drawFrame(x, ytop, 31, 10);

    char buf[4]; snprintf(buf, sizeof(buf), "%u", i + 1);
    uint8_t tw = u8g2.getStrWidth(buf);
    u8g2.drawStr(x + (31 - tw) / 2, ytop + 8, buf);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

// ── Enregistre gs_gen_tmp dans gen_list et revient à la liste ────────
static void _gen_save_and_return() {
  if (gs_gen_idx >= gen_count) gen_count++;
  gen_list[gs_gen_idx] = gs_gen_tmp;
  memset(&gen_rt[gs_gen_idx], 0, sizeof(GeneratorRuntime));
  gs_state  = GS_LIST;
  gs_cursor = gs_gen_idx;
}

static void gs_draw_current();  // forward déclaration

bool gs_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {

  switch (gs_state) {

    // ── LIST ─────────────────────────────────────────────────────
    case GS_LIST: {
      bool can_add = (gen_count < GEN_MAX);
      uint8_t total = gen_count + (can_add ? 1 : 0);
      if (enc_up   && gs_cursor > 0)        gs_cursor--;
      if (enc_down && gs_cursor < total-1)  gs_cursor++;
      if (btn_valid) {
        if (can_add && gs_cursor == gen_count) {
          // Défauts des DEUX types posés d'emblée : l'utilisateur peut
          // naviguer en arrière jusqu'à GS_TYPE et changer d'avis sans
          // retomber sur des champs à zéro pour l'autre type.
          memset(&gs_gen_tmp, 0, sizeof(gs_gen_tmp));
          gs_gen_tmp.active          = true;
          gs_gen_tmp.type            = GEN_LFO;
          gs_gen_tmp.waveform        = LFO_TRIANGLE;
          gs_gen_tmp.sync_mode       = LFO_SYNC_FREE;
          gs_gen_tmp.rate_x10hz      = LFO_RATE_X10HZ_DEFAULT;
          gs_gen_tmp.division_idx    = 2;   // 1/4
          gs_gen_tmp.depth           = 63;
          gs_gen_tmp.center          = 64;
          gs_gen_tmp.cc_number       = 1;   // modwheel par défaut
          gs_gen_tmp.note            = 60;  // Do central
          gs_gen_tmp.euclid_steps    = 8;
          gs_gen_tmp.euclid_pulses   = 3;
          gs_gen_tmp.euclid_rotation = 0;
          gs_gen_tmp.gate_percent    = 50;
          gs_gen_idx    = gen_count;
          gs_step_cursor= gs_gen_tmp.type;
          gs_state = GS_TYPE;
        } else {
          gs_gen_idx    = gs_cursor;
          gs_action_cur = 0;
          gs_state = GS_ACTION;
        }
      }
      if (btn_back) return true;
      break;
    }

    // ── ACTION — Modifier / Supprimer ───────────────────────────
    case GS_ACTION:
      if (enc_up   && gs_action_cur > 0) gs_action_cur--;
      if (enc_down && gs_action_cur < 1) gs_action_cur++;
      if (btn_valid) {
        if (gs_action_cur == 0) {
          gs_gen_tmp     = gen_list[gs_gen_idx];
          gs_step_cursor = gs_gen_tmp.type;
          gs_state = GS_TYPE;
        } else {
          for (uint8_t i = gs_gen_idx; i < gen_count - 1; i++) {
            gen_list[i] = gen_list[i + 1];
            gen_rt[i]   = gen_rt[i + 1];
          }
          gen_count--;
          memset(&gen_rt[gen_count], 0, sizeof(GeneratorRuntime));
          if (gs_cursor >= gen_count) gs_cursor = gen_count;
          gs_state = GS_LIST;
        }
      }
      if (btn_back) { gs_state = GS_LIST; }
      break;

    // ── TYPE ─────────────────────────────────────────────────────
    case GS_TYPE:
      if (enc_up   && gs_step_cursor > 0)                  gs_step_cursor--;
      if (enc_down && gs_step_cursor < GEN_TYPE_COUNT-1)   gs_step_cursor++;
      if (btn_valid) {
        gs_gen_tmp.type = gs_step_cursor;
        if (gs_gen_tmp.type == GEN_LFO) {
          gs_step_cursor = gs_gen_tmp.waveform;
          gs_state = GS_WAVEFORM;
        } else {
          gs_step_cursor = gs_gen_tmp.sync_mode;
          gs_state = GS_SYNC_MODE;
        }
      }
      if (btn_back) { gs_state = GS_LIST; gs_cursor = gs_gen_idx; }
      break;

    // ── WAVEFORM (LFO) ───────────────────────────────────────────
    case GS_WAVEFORM:
      if (enc_up   && gs_step_cursor > 0)                     gs_step_cursor--;
      if (enc_down && gs_step_cursor < LFO_WAVEFORM_COUNT-1)  gs_step_cursor++;
      if (btn_valid) {
        gs_gen_tmp.waveform = gs_step_cursor;
        gs_step_cursor = gs_gen_tmp.sync_mode;
        gs_state = GS_SYNC_MODE;
      }
      if (btn_back) { gs_step_cursor = gs_gen_tmp.type; gs_state = GS_TYPE; }
      break;

    // ── SYNC_MODE — partagé LFO/EUCLID ────────────────────────────
    case GS_SYNC_MODE:
      if (enc_up   && gs_step_cursor > 0) gs_step_cursor--;
      if (enc_down && gs_step_cursor < 1) gs_step_cursor++;
      if (btn_valid) {
        gs_gen_tmp.sync_mode = gs_step_cursor;
        if (gs_gen_tmp.sync_mode == LFO_SYNC_FREE) { gs_state = GS_RATE; }
        else { gs_step_cursor = gs_gen_tmp.division_idx; gs_state = GS_DIVISION; }
      }
      if (btn_back) {
        if (gs_gen_tmp.type == GEN_LFO) { gs_step_cursor = gs_gen_tmp.waveform; gs_state = GS_WAVEFORM; }
        else                            { gs_step_cursor = gs_gen_tmp.type;    gs_state = GS_TYPE; }
      }
      break;

    // ── RATE (mode libre) — partagé LFO/EUCLID ────────────────────
    case GS_RATE:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 10);   // rotation rapide : pas de 1.0Hz au lieu de 0.1Hz
        if (enc_down) {
          gs_gen_tmp.rate_x10hz = (gs_gen_tmp.rate_x10hz + step > LFO_RATE_X10HZ_MAX)
                                    ? LFO_RATE_X10HZ_MAX : gs_gen_tmp.rate_x10hz + step;
        } else {
          gs_gen_tmp.rate_x10hz = (gs_gen_tmp.rate_x10hz < LFO_RATE_X10HZ_MIN + step)
                                    ? LFO_RATE_X10HZ_MIN : gs_gen_tmp.rate_x10hz - step;
        }
      }
      if (btn_valid) { gs_state = (gs_gen_tmp.type == GEN_LFO) ? GS_DEPTH : GS_NOTE; }
      if (btn_back)  { gs_step_cursor = gs_gen_tmp.sync_mode; gs_state = GS_SYNC_MODE; }
      break;

    // ── DIVISION (mode synchro) — partagé LFO/EUCLID ──────────────
    case GS_DIVISION:
      if (enc_up   && gs_step_cursor > 0)                      gs_step_cursor--;
      if (enc_down && gs_step_cursor < LFO_DIVISION_COUNT-1)   gs_step_cursor++;
      if (btn_valid) {
        gs_gen_tmp.division_idx = gs_step_cursor;
        gs_state = (gs_gen_tmp.type == GEN_LFO) ? GS_DEPTH : GS_NOTE;
      }
      if (btn_back) { gs_step_cursor = gs_gen_tmp.sync_mode; gs_state = GS_SYNC_MODE; }
      break;

    // ── DEPTH ────────────────────────────────────────────────────
    case GS_DEPTH:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 5);
        if (enc_down) {
          gs_gen_tmp.depth = (gs_gen_tmp.depth + step > 63) ? 63 : (uint8_t)(gs_gen_tmp.depth + step);
        } else {
          gs_gen_tmp.depth = (gs_gen_tmp.depth < step) ? 0 : (uint8_t)(gs_gen_tmp.depth - step);
        }
      }
      if (btn_valid) { gs_state = GS_CENTER; }
      if (btn_back)  { gs_state = (gs_gen_tmp.sync_mode == LFO_SYNC_FREE) ? GS_RATE : GS_DIVISION; }
      break;

    // ── CENTER ───────────────────────────────────────────────────
    case GS_CENTER:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 5);
        if (enc_down) {
          gs_gen_tmp.center = (gs_gen_tmp.center + step > 127) ? 127 : (uint8_t)(gs_gen_tmp.center + step);
        } else {
          gs_gen_tmp.center = (gs_gen_tmp.center < step) ? 0 : (uint8_t)(gs_gen_tmp.center - step);
        }
      }
      if (btn_valid) { gs_state = GS_CC_NUMBER; }
      if (btn_back)  { gs_state = GS_DEPTH; }
      break;

    // ── CC_NUMBER ────────────────────────────────────────────────
    case GS_CC_NUMBER:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 5);
        if (enc_down) {
          gs_gen_tmp.cc_number = (gs_gen_tmp.cc_number + step > 127) ? 127 : (uint8_t)(gs_gen_tmp.cc_number + step);
        } else {
          gs_gen_tmp.cc_number = (gs_gen_tmp.cc_number < step) ? 0 : (uint8_t)(gs_gen_tmp.cc_number - step);
        }
      }
      if (btn_valid) { gs_step_cursor = 0; gs_state = GS_OUT; }
      if (btn_back)  { gs_state = GS_CENTER; }
      break;

    // ── NOTE (euclidien) ─────────────────────────────────────────
    case GS_NOTE:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 5);
        if (enc_down) gs_gen_tmp.note = (gs_gen_tmp.note + step > 127) ? 127 : (uint8_t)(gs_gen_tmp.note + step);
        else          gs_gen_tmp.note = (gs_gen_tmp.note < step) ? 0 : (uint8_t)(gs_gen_tmp.note - step);
      }
      if (btn_valid) { gs_state = GS_STEPS; }
      if (btn_back)  { gs_state = (gs_gen_tmp.sync_mode == LFO_SYNC_FREE) ? GS_RATE : GS_DIVISION; }
      break;

    // ── STEPS ────────────────────────────────────────────────────
    case GS_STEPS:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 4);
        if (enc_down) {
          gs_gen_tmp.euclid_steps = (gs_gen_tmp.euclid_steps + step > EUCLID_STEPS_MAX)
                                      ? EUCLID_STEPS_MAX : (uint8_t)(gs_gen_tmp.euclid_steps + step);
        } else {
          gs_gen_tmp.euclid_steps = (gs_gen_tmp.euclid_steps <= step)
                                      ? 1 : (uint8_t)(gs_gen_tmp.euclid_steps - step);
        }
        // Garder pulses/rotation dans les bornes du nouveau nombre de pas.
        if (gs_gen_tmp.euclid_pulses > gs_gen_tmp.euclid_steps)
          gs_gen_tmp.euclid_pulses = gs_gen_tmp.euclid_steps;
        if (gs_gen_tmp.euclid_rotation >= gs_gen_tmp.euclid_steps)
          gs_gen_tmp.euclid_rotation = gs_gen_tmp.euclid_steps - 1;
      }
      if (btn_valid) { gs_state = GS_PULSES; }
      if (btn_back)  { gs_state = GS_NOTE; }
      break;

    // ── PULSES ───────────────────────────────────────────────────
    case GS_PULSES:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 4);
        if (enc_down) {
          gs_gen_tmp.euclid_pulses = (gs_gen_tmp.euclid_pulses + step > gs_gen_tmp.euclid_steps)
                                       ? gs_gen_tmp.euclid_steps : (uint8_t)(gs_gen_tmp.euclid_pulses + step);
        } else {
          gs_gen_tmp.euclid_pulses = (gs_gen_tmp.euclid_pulses < step)
                                       ? 0 : (uint8_t)(gs_gen_tmp.euclid_pulses - step);
        }
      }
      if (btn_valid) { gs_state = GS_ROTATION; }
      if (btn_back)  { gs_state = GS_STEPS; }
      break;

    // ── ROTATION ─────────────────────────────────────────────────
    case GS_ROTATION: {
      uint8_t rot_max = (gs_gen_tmp.euclid_steps > 0) ? (uint8_t)(gs_gen_tmp.euclid_steps - 1) : 0;
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 4);
        if (enc_down) {
          gs_gen_tmp.euclid_rotation = (gs_gen_tmp.euclid_rotation + step > rot_max)
                                          ? rot_max : (uint8_t)(gs_gen_tmp.euclid_rotation + step);
        } else {
          gs_gen_tmp.euclid_rotation = (gs_gen_tmp.euclid_rotation < step)
                                          ? 0 : (uint8_t)(gs_gen_tmp.euclid_rotation - step);
        }
      }
      if (btn_valid) { gs_state = GS_GATE_PERCENT; }
      if (btn_back)  { gs_state = GS_PULSES; }
      break;
    }

    // ── GATE_PERCENT ─────────────────────────────────────────────
    case GS_GATE_PERCENT:
      if (enc_up || enc_down) {
        uint16_t step = _gs_step(1, 10);
        if (enc_down) {
          gs_gen_tmp.gate_percent = (gs_gen_tmp.gate_percent + step > 100)
                                       ? 100 : (uint8_t)(gs_gen_tmp.gate_percent + step);
        } else {
          gs_gen_tmp.gate_percent = (gs_gen_tmp.gate_percent <= step)
                                       ? 1 : (uint8_t)(gs_gen_tmp.gate_percent - step);
        }
      }
      if (btn_valid) { gs_step_cursor = 0; gs_state = GS_OUT; }
      if (btn_back)  { gs_state = GS_ROTATION; }
      break;

    // ── OUT — sorties ────────────────────────────────────────────
    case GS_OUT:
      if (enc_up   && gs_step_cursor > 0) gs_step_cursor--;
      if (enc_down && gs_step_cursor < 9) gs_step_cursor++;
      if (btn_valid) {
        if (gs_step_cursor == 9) {
          _gen_save_and_return();
        } else if (_gs_out_has_port(gs_step_cursor)) {
          _gs_toggle_out_port(gs_step_cursor);
        } else {
          _gs_toggle_out_port(gs_step_cursor);
          gs_ch_port   = gs_step_cursor;
          gs_ch_cursor = 0;
          gs_state = GS_OUT_CH;
        }
      }
      if (btn_back) { gs_state = (gs_gen_tmp.type == GEN_LFO) ? GS_CC_NUMBER : GS_GATE_PERCENT; }
      break;

    // ── OUT_CH — picker canaux sortie ───────────────────────────
    case GS_OUT_CH:
      if (enc_up   && gs_ch_cursor > 0)  gs_ch_cursor--;
      if (enc_down && gs_ch_cursor < 15) gs_ch_cursor++;
      if (btn_valid) {
        for (uint8_t i = 0; i < gs_gen_tmp.n_out; i++) {
          if (gs_gen_tmp.out[i].port != gs_ch_port) continue;
          gs_gen_tmp.out[i].chan_mask ^= (uint16_t)(1u << gs_ch_cursor);
          break;
        }
      }
      if (btn_back) { _gs_prune_empty_outs(); gs_state = GS_OUT; }
      break;
  }

  gs_draw_current();
  return false;
}

static void gs_draw_current() {
  switch (gs_state) {
    case GS_LIST:      gs_draw_list();      break;
    case GS_ACTION:    gs_draw_action();    break;
    case GS_TYPE:       gs_draw_type();       break;
    case GS_WAVEFORM:  gs_draw_waveform();  break;
    case GS_SYNC_MODE: gs_draw_sync_mode(); break;
    case GS_RATE:      gs_draw_rate();      break;
    case GS_DIVISION:  gs_draw_division();  break;
    case GS_DEPTH:      gs_draw_depth();     break;
    case GS_CENTER:     gs_draw_center();    break;
    case GS_CC_NUMBER:  gs_draw_cc_number(); break;
    case GS_NOTE:         gs_draw_note();          break;
    case GS_STEPS:        gs_draw_steps();         break;
    case GS_PULSES:       gs_draw_pulses();        break;
    case GS_ROTATION:     gs_draw_rotation();      break;
    case GS_GATE_PERCENT: gs_draw_gate_percent();  break;
    case GS_OUT:         gs_draw_out();        break;
    case GS_OUT_CH:      gs_draw_out_ch();     break;
  }
}

void gs_enter() {
  gs_state       = GS_LIST;
  gs_cursor      = 0;
  gs_list_scroll = 0;
  gs_step_cursor = 0;
  gs_last_enc_ms = 0;   // évite un premier cran accéléré par erreur (cf. transport_enter())
}
