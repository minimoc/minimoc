#pragma once
// monitor.h — Moniteur d'activité MIDI temps réel (VU-mètre)
//
// mon_push() : appelé depuis rec_push() — ~10 ns, jamais bloquant
// mon_draw() : appelé depuis ui_tick() uniquement quand l'écran MONITOR est actif
//
// Layout 128×64 :
//   y  0-29  : 5 entrées  (A / B / H1 / H2 / H3)  — barres VU 18 px max
//   y 30     : séparateur
//   y 31-63  : 9 sorties  (1-6 / H1 / H2 / H3)    — barres VU 18 px max

#define MON_FADE_MS  500u   // durée de la décroissance en ms

// ----------------------------------------------------------------
// Données partagées — écriture depuis rec_push, lecture depuis mon_draw
// ----------------------------------------------------------------
static uint8_t  mon_in_vel[5]  = {0};   // 0=A 1=B 2=H1 3=H2 4=H3
static uint32_t mon_in_ms[5]   = {0};
static uint8_t  mon_out_vel[9] = {0};   // 0=Sortie1 … 5=Sortie6 6=HubOut1 7=HubOut2 8=HubOut3
static uint32_t mon_out_ms[9]  = {0};

// ----------------------------------------------------------------
// mon_push — appelé depuis rec_push, basse priorité
// source : 1=A 2=B 3=H1 4=H2 5=H3 (0=PC : pas affiché en entrée)
// output : 1-9
// val    : velocity / valeur CC (d2), ou 80 si absent (PC/PB/AT)
// ----------------------------------------------------------------
inline void mon_push(uint8_t source, uint8_t output, uint8_t val) {
    uint8_t level = (val > 0) ? val : 80u;   // niveau minimal pour PC/PB/AT/PC
    uint32_t now  = millis();

    if (source >= 1 && source <= 5) {
        mon_in_vel[source - 1] = level;
        mon_in_ms [source - 1] = now;
    }
    if (output >= 1 && output <= 9) {
        mon_out_vel[output - 1] = level;
        mon_out_ms [output - 1] = now;
    }
}

// ----------------------------------------------------------------
// mon_draw — rendu OLED, appelé uniquement depuis ui_tick
// ----------------------------------------------------------------
void mon_draw() {
    u8g2.clearBuffer();

    const uint32_t now = millis();

    // ── Paramètres de mise en page ──────────────────────────────
    const uint8_t IN_W     = 128 / 5;   // 25 px par entrée
    const uint8_t OUT_W    = 128 / 9;   // 14 px par sortie
    const uint8_t BAR_H    = 17;        // hauteur max barre (intérieur cadre)
    const uint8_t IN_LBL_Y =  7;        // baseline étiquettes entrées
    const uint8_t IN_FRM_Y = 10;        // y haut cadre entrées
    const uint8_t SEP_Y    = 30;        // séparateur
    const uint8_t OUT_LBL_Y= 38;        // baseline étiquettes sorties
    const uint8_t OUT_FRM_Y= 41;        // y haut cadre sorties
    const uint8_t OUT_FRM_H= 21;        // hauteur cadre sorties

    static const char* in_lbl[]  = { "A", "B", "C", "D", "E" };
    static const char* out_lbl[] = { "1","2","3","4","5","6","7","8","9" };

    u8g2.setFont(u8g2_font_5x7_tf);   // = UI_FONT_SMALL (défini après dans carousel_ui.h)

    // ── Entrées ─────────────────────────────────────────────────
    for (uint8_t i = 0; i < 5; i++) {
        const uint8_t x = i * IN_W;

        // Séparateur vertical (sauf premier)
        if (i > 0) u8g2.drawVLine(x, 0, SEP_Y);

        // Étiquette centrée
        const uint8_t lw = u8g2.getStrWidth(in_lbl[i]);
        u8g2.drawStr(x + (IN_W - lw) / 2, IN_LBL_Y, in_lbl[i]);

        // Cadre VU
        u8g2.drawFrame(x + 2, IN_FRM_Y, IN_W - 4, BAR_H + 2);

        // Barre VU avec décroissance
        const uint32_t age = now - mon_in_ms[i];
        if (age < MON_FADE_MS && mon_in_vel[i] > 0) {
            uint8_t h = (uint8_t)((uint32_t)mon_in_vel[i] * BAR_H / 127u);
            if (h == 0) h = 1;
            h = (uint8_t)((uint32_t)h * (MON_FADE_MS - age) / MON_FADE_MS);
            if (h > 0) {
                u8g2.drawBox(x + 3,
                             IN_FRM_Y + BAR_H + 1 - h,
                             IN_W - 6, h);
            }
        }
    }

    // Séparateur horizontal
    u8g2.drawHLine(0, SEP_Y, 128);

    // ── Sorties ─────────────────────────────────────────────────
    for (uint8_t i = 0; i < 9; i++) {
        const uint8_t x = i * OUT_W;

        // Séparateur vertical
        if (i > 0) u8g2.drawVLine(x, SEP_Y + 1, 63 - SEP_Y);

        // Étiquette centrée
        const uint8_t lw = u8g2.getStrWidth(out_lbl[i]);
        u8g2.drawStr(x + (OUT_W - lw) / 2, OUT_LBL_Y, out_lbl[i]);

        // Cadre VU
        u8g2.drawFrame(x + 1, OUT_FRM_Y, OUT_W - 2, OUT_FRM_H);

        // Barre VU avec décroissance
        const uint32_t age = now - mon_out_ms[i];
        if (age < MON_FADE_MS && mon_out_vel[i] > 0) {
            uint8_t h = (uint8_t)((uint32_t)mon_out_vel[i] * (OUT_FRM_H - 2) / 127u);
            if (h == 0) h = 1;
            h = (uint8_t)((uint32_t)h * (MON_FADE_MS - age) / MON_FADE_MS);
            if (h > 0) {
                u8g2.drawBox(x + 2,
                             OUT_FRM_Y + OUT_FRM_H - 1 - h,
                             OUT_W - 4, h);
            }
        }
    }

    u8g2.sendBuffer();
}

// ----------------------------------------------------------------
// BPM tracking — comptage fréquentiel sur fenêtre glissante 2 s
// ----------------------------------------------------------------
// On compte le nombre de ticks MIDI Clock reçus dans les 2 dernières
// secondes. Aucun rejet : les ticks lus en double (double MIDIUSB.read()
// par itération) sont simplement comptés, ce qui est correct.
// BPM = count × 60 / (24 × 2)  →  BPM = count × 1.25
//
// Buffer circulaire de timestamps (ms) pour la fenêtre glissante.
// À 300 BPM : 120 ticks/s × 2 s = 240 ticks max → 256 suffisent.
#define BPM_BUF_SIZE   256u
#define BPM_WINDOW_MS  2000u
#define BPM_TIMEOUT_MS 2000u

static uint32_t bpm_ts_ms[BPM_BUF_SIZE] = {0};
static uint16_t bpm_buf_head  = 0;
static uint16_t bpm_buf_count = 0;
static float    bpm_value     = 0.0f;
static uint32_t bpm_last_ms   = 0;

inline void bpm_push_clock() {
    uint32_t now_ms = millis();
    bpm_last_ms = now_ms;

    bpm_ts_ms[bpm_buf_head] = now_ms;
    bpm_buf_head = (bpm_buf_head + 1) % BPM_BUF_SIZE;
    if (bpm_buf_count < BPM_BUF_SIZE) bpm_buf_count++;
}

// Appelé depuis mon_draw_bpm() à chaque rafraîchissement (~50 ms)
static void bpm_compute() {
    if (bpm_buf_count == 0) return;
    uint32_t now_ms = millis();
    uint32_t cutoff = now_ms - BPM_WINDOW_MS;

    // Compter les ticks dans la fenêtre (en partant du plus récent)
    uint16_t count = 0;
    for (uint16_t i = 0; i < bpm_buf_count; i++) {
        uint16_t idx = (bpm_buf_head + BPM_BUF_SIZE - 1 - i) % BPM_BUF_SIZE;
        if (bpm_ts_ms[idx] <= cutoff) break;
        count++;
    }

    if (count >= 24u) {   // au moins 1 temps complet
        float raw = (float)count * (60000.0f / (24.0f * BPM_WINDOW_MS));
        bpm_value = (bpm_value < 1.0f) ? raw : 0.8f * bpm_value + 0.2f * raw;
    }
}
