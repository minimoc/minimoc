#pragma once
// sync_submenu.h — Sélection du maître SYNC
//
// Liste défilante : sources SYNC_LABELS (cf. logic.h) + OFF. Chaque lettre
// A-E propose deux sources distinctes — TRS/USB Host physique d'un côté,
// câble USB miroir venant du PC de l'autre (ex. le smartmirror) — pour éviter
// qu'un maître "A" écoute les deux en même temps. MiniMoc peut aussi se
// choisir lui-même comme horloge interne (tempo fixe).
//
// La source active (sync_master) est marquée d'un "*" ; le curseur de
// navigation (encart inversé) est indépendant tant que OK n'a pas été pressé.
//
// OFF = tous les real-time filtrés

#define SYNC_OPT_COUNT (SYNC_SOURCE_COUNT + 1)   // sources + OFF

static uint8_t sync_cursor = SYNC_OPT_COUNT - 1;
static uint8_t sync_scroll = 0;   // index du premier item visible dans la liste

static uint8_t _sync_to_cursor() {
    return (sync_master == 0xFF) ? (SYNC_OPT_COUNT - 1) : sync_master;
}
static uint8_t _cursor_to_sync() {
    return (sync_cursor == SYNC_OPT_COUNT - 1) ? 0xFF : sync_cursor;
}

#define SY_ITEM0_Y 28
#define SY_ROW_H   11
#define SY_VISIBLE 4   // lignes visibles simultanément dans la liste

// ── Rendu ─────────────────────────────────────────────────────────
void sync_draw() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 14, "SYNC");
    u8g2.drawHLine(0, 18, SCREEN_W);

    // Ajuste le défilement pour garder le curseur visible
    if (sync_cursor < sync_scroll)                   sync_scroll = sync_cursor;
    if (sync_cursor >= sync_scroll + SY_VISIBLE)     sync_scroll = sync_cursor - SY_VISIBLE + 1;

    uint8_t active_idx = _sync_to_cursor();

    u8g2.setFont(UI_FONT_BODY);
    for (uint8_t vi = 0; vi < SY_VISIBLE; vi++) {
        uint8_t i = sync_scroll + vi;
        if (i >= SYNC_OPT_COUNT) break;
        int y = SY_ITEM0_Y + vi * SY_ROW_H;

        if (i == sync_cursor) {
            u8g2.drawBox(0, y - 9, SCREEN_W, SY_ROW_H);
            u8g2.setDrawColor(0);
        }

        const char* label = (i < SYNC_SOURCE_COUNT) ? SYNC_LABELS[i] : "OFF";
        char buf[16];
        if (i == active_idx) snprintf(buf, sizeof(buf), "%s *", label);
        else                  snprintf(buf, sizeof(buf), "%s",  label);
        u8g2.drawStr(4, y, buf);

        u8g2.setDrawColor(1);
    }

    // Flèches de défilement
    u8g2.setFont(UI_FONT_SMALL);
    if (sync_scroll > 0)                             u8g2.drawStr(122, 25, "^");
    if (sync_scroll + SY_VISIBLE < SYNC_OPT_COUNT)   u8g2.drawStr(122, 63, "v");

    u8g2.sendBuffer();
}

// ── Handler ───────────────────────────────────────────────────────
bool sync_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
    if (enc_up   && sync_cursor > 0)                sync_cursor--;
    if (enc_down && sync_cursor < SYNC_OPT_COUNT-1) sync_cursor++;

    if (btn_valid) {
        sync_master = _cursor_to_sync();
        sync_save();
        internal_clock_apply();   // démarre/arrête l'horloge interne selon le nouveau choix
    }

    sync_draw();
    return btn_back;
}

void sync_enter() {
    sync_cursor = _sync_to_cursor();
    sync_scroll = 0;
}
