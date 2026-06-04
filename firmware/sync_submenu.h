#pragma once
// sync_submenu.h — Sélection du maître SYNC
//
// Grille 2×3 de grandes cases :   [A]  [B]  [C]
//                                  [D]  [E] [OFF]
//
// Sélection active = fond blanc (inversé)
// Curseur (non sélectionné) = cadre blanc
//
// OFF = tous les real-time filtrés
// A/B/C/D/E = ce port transmet Clock/Start/Stop/Continue vers toutes les sorties

// ── Constantes ───────────────────────────────────────────────────
#define SYNC_OPT_OFF   5
#define SYNC_OPT_COUNT 6

static const char* SYNC_LABELS[] = {"A","B","C","D","E","OFF"};

static uint8_t sync_cursor = 5;

static uint8_t _sync_to_cursor() {
    return (sync_master == 0xFF) ? SYNC_OPT_OFF : sync_master;
}
static uint8_t _cursor_to_sync() {
    return (sync_cursor == SYNC_OPT_OFF) ? 0xFF : sync_cursor;
}

// ── Géométrie de la grille ────────────────────────────────────────
// 3 colonnes × 2 lignes, sous un titre de 11px
// Colonne : 42px large, 1px de gap → positions x = 0, 43, 86
// Ligne : de y=12 à y=37 (26px) et y=38 à y=63 (26px)
#define SY_COLS  3
#define SY_X(c)  ((c) * 43)
#define SY_Y(r)  (12 + (r) * 26)
#define SY_W     42
#define SY_H     26

// ── Rendu ─────────────────────────────────────────────────────────
void sync_draw() {
    u8g2.clearBuffer();

    // Titre compact
    u8g2.setFont(UI_FONT_SMALL);
    u8g2.drawStr(0, 8, "SYNC  Maitre horloge:");
    // pas de separator line : la grille commence juste en dessous

    uint8_t active = _sync_to_cursor();

    for (uint8_t i = 0; i < SYNC_OPT_COUNT; i++) {
        uint8_t col = i % SY_COLS;
        uint8_t row = i / SY_COLS;
        uint8_t x   = SY_X(col);
        uint8_t y   = SY_Y(row);

        bool sel = (sync_cursor == i);
        bool on  = (active == i);

        if (on) {
            u8g2.drawBox(x, y, SY_W, SY_H);
            u8g2.setDrawColor(0);
        }
        if (sel && !on) {
            u8g2.drawFrame(x, y, SY_W, SY_H);
        }
        if (sel && on) {
            // curseur sur la case active : cadre noir à l'intérieur
            u8g2.setDrawColor(0);
            u8g2.drawFrame(x+1, y+1, SY_W-2, SY_H-2);
        }

        // Texte centré dans la case (grande police)
        u8g2.setFont(u8g2_font_8x13_tf);
        uint8_t tw = u8g2.getStrWidth(SYNC_LABELS[i]);
        u8g2.drawStr(x + (SY_W - tw) / 2, y + SY_H - 4, SYNC_LABELS[i]);

        u8g2.setFont(UI_FONT_SMALL);
        u8g2.setDrawColor(1);
    }

    u8g2.sendBuffer();
}

// ── Handler ───────────────────────────────────────────────────────
bool sync_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
    if (enc_up   && sync_cursor > 0)                sync_cursor--;
    if (enc_down && sync_cursor < SYNC_OPT_COUNT-1) sync_cursor++;

    if (btn_valid) {
        sync_master = _cursor_to_sync();
        sync_save();
    }

    sync_draw();
    return btn_back;
}

void sync_enter() {
    sync_cursor = _sync_to_cursor();
}
