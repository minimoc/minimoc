#pragma once
// transport_submenu.h — Réglage du tempo + transport de l'horloge interne
// MiniMoc (source MINIMOC du menu SYNC, voir internal_clock_apply()/_isr()
// dans _midi.h).
//
// Écran minimal : le BPM en grand, centré, et l'état PLAY/PAUSE/STOP en dessous.
//
// Gestes :
//  - Tourner l'encodeur (BACK relâché) : ±1 BPM par cran, ±5 si les crans
//    s'enchaînent vite (rotation rapide), appliqué immédiatement à l'horloge
//    interne ; sauvegardé en EEPROM à la sortie du menu.
//  - Clic encodeur (BACK relâché) : bascule Play/Pause — Start au premier
//    départ, Stop en pause, Continue (pas Start) à la reprise, via
//    _sync_forward() (même chemin que n'importe quel maître SYNC).
//  - Clic simple sur BACK : dépend de l'état — en PAUSE, ça fait STOP (reste
//    sur cet écran) ; dans tous les autres cas (PLAY ou déjà STOP), ça
//    retourne au menu précédent. Résolu immédiatement, pas de délai.
//  - STOP : même effet que Pause (Stop temps réel, arrête réellement la
//    musique) + MMC Stop pour un enregistreur/DAW externe, mais sans reprise
//    possible : le Play suivant redémarre (Start) au lieu de reprendre (Continue).
//  - BACK tenu + clic encodeur : RECORD (MMC Record Strobe).
//  - BACK tenu + tourner à droite : AVANCE (MMC Fast Forward), un cran = un envoi.
//  - BACK tenu + tourner à gauche : REWIND (MMC Rewind), un cran = un envoi.
//
// Le long-appui global BACK→écran logo (firmware.ino) est désactivé tant que
// cet écran est actif, pour ne pas interrompre les gestes chordés ci-dessus.

#define TRANSPORT_BPM_STEP      1u
#define TRANSPORT_BPM_STEP_FAST 5u    // pas utilisé quand les crans s'enchaînent vite
#define TRANSPORT_BPM_ACCEL_MS  180u  // écart max entre deux crans pour considérer que ça tourne vite

static bool transport_playing   = false;
static bool transport_resumable = false;  // true en pause : le Play suivant envoie Continue au lieu de Start

// État du geste BACK — voir transport_handle_input()
static bool transport_back_was_down = false;  // niveau précédent de btn_back
static bool transport_back_chorded  = false;  // un geste chordé a eu lieu pendant l'appui en cours

static uint32_t transport_last_enc_ms = 0;    // horodatage du dernier cran appliqué (accélération BPM)

// ── Actions de transport — réutilisées par l'encodeur (ci-dessous) et par le
// weblink (commande SysEx 0x11, cf. PCEditor.h) ──────────────────────────────
inline void transport_do_record()      { send_mmc(MMC_RECORD_STROBE); }
inline void transport_do_fastforward() { send_mmc(MMC_FAST_FORWARD); }
inline void transport_do_rewind()      { send_mmc(MMC_REWIND); }

inline void transport_do_play_pause() {
    if (!transport_playing) {
        _sync_forward((byte)(transport_resumable ? midi::Continue : midi::Start));
        transport_playing = true;
    } else {
        _sync_forward((byte)midi::Stop);
        transport_playing   = false;
        transport_resumable = true;   // pause : le prochain Play reprendra (Continue)
    }
}

inline void transport_do_stop() {
    _sync_forward((byte)midi::Stop);
    send_mmc(MMC_STOP);
    transport_playing   = false;
    transport_resumable = false;   // un vrai STOP réinitialise : le Play suivant redémarre (Start)
}

void transport_draw() {
    u8g2.clearBuffer();

    char buf[5];
    snprintf(buf, sizeof(buf), "%u", internal_clock_bpm);
    u8g2.setFont(u8g2_font_logisoso42_tn);
    int16_t tw = (int16_t)u8g2.getStrWidth(buf);
    u8g2.drawStr((SCREEN_W - tw) / 2, 44, buf);

    u8g2.setFont(UI_FONT_SMALL);
    const char* status = transport_playing ? "PLAY" : (transport_resumable ? "PAUSE" : "STOP");
    uint8_t sw = u8g2.getStrWidth(status);
    u8g2.drawStr((SCREEN_W - sw) / 2, 63, status);

    u8g2.sendBuffer();
}

bool transport_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
    bool exit_menu = false;

    if (btn_back) {
        // ── Gestes chordés : BACK tenu + encodeur ──────────────────────
        if (btn_valid) { transport_do_record();      transport_back_chorded = true; }
        if (enc_down)  { transport_do_fastforward();  transport_back_chorded = true; }
        if (enc_up)    { transport_do_rewind();       transport_back_chorded = true; }
    } else {
        if (transport_back_was_down) {
            // BACK vient d'être relâché
            if (transport_back_chorded) {
                // Un chord a eu lieu pendant cet appui : ni STOP ni sortie de menu.
                transport_back_chorded = false;
            } else if (!transport_playing && transport_resumable) {
                // En pause : le clic simple fait STOP (reste sur cet écran).
                transport_do_stop();
            } else {
                // Sinon (en lecture ou déjà stoppé) : clic simple = retour au menu précédent.
                internal_clock_bpm_save();
                exit_menu = true;
            }
        }

        // Encodeur = réglage BPM, seulement quand BACK n'est pas tenu.
        // Accélération : pas de 5 si le cran précédent date de moins de
        // TRANSPORT_BPM_ACCEL_MS (rotation rapide), sinon pas de 1.
        if (enc_down || enc_up) {
            uint32_t now  = millis();
            uint16_t step = (now - transport_last_enc_ms < TRANSPORT_BPM_ACCEL_MS)
                             ? TRANSPORT_BPM_STEP_FAST : TRANSPORT_BPM_STEP;
            transport_last_enc_ms = now;

            if (enc_down) {
                internal_clock_bpm = (internal_clock_bpm + step > INTERNAL_CLOCK_BPM_MAX)
                                       ? INTERNAL_CLOCK_BPM_MAX : internal_clock_bpm + step;
            } else {
                internal_clock_bpm = (internal_clock_bpm < INTERNAL_CLOCK_BPM_MIN + step)
                                       ? INTERNAL_CLOCK_BPM_MIN : internal_clock_bpm - step;
            }
            internal_clock_apply();
        }

        if (btn_valid) {
            transport_do_play_pause();
        }
    }
    transport_back_was_down = btn_back;

    if (exit_menu) return true;   // retour au carousel

    transport_draw();
    return false;
}

void transport_enter() {
    transport_back_was_down = false;
    transport_back_chorded  = false;
    transport_last_enc_ms   = 0;
}
