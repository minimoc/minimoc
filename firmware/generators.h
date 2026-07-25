#pragma once
// generators.h — boucle de service des générateurs (LFO + euclidien + motifs)
//
// Inclus APRÈS _midi.h (utilise generators_send_cc()/generators_send_note_on()/
// off()) : c'est ici, et seulement ici, que la phase/le pas calculé en amont
// (mode synchro : logic.h/bpm_push_clock ; mode libre : ci-dessous) se
// traduit en un envoi MIDI réel.
//
// generators_tick() est appelée en continu depuis loop() (voir firmware.ino),
// hors du bloc `if (modeInteractif)` : un générateur doit continuer de
// tourner même écran éteint.

// Période de service interne, indépendante du refresh écran (~50ms) et du
// clock MIDI — assez rapide pour un rendu de LFO lisse jusqu'à 50 Hz (mode
// libre), assez lente pour ne pas saturer le bus MIDI de CC redondants (le
// filtre "valeur inchangée" ci-dessous fait le gros du travail anti-spam).
// Limite pratique pour un séquenceur euclidien en mode libre : à cette
// granularité (100 Hz), un pas plus rapide qu'environ 10ms risque d'être
// manqué (aucun rattrapage multi-pas) — largement suffisant pour un usage
// rythmique normal, pas pensé pour du taux audio.
#define GEN_TICK_MS 10u

static inline void _gen_flush_lfo(uint8_t i, Generator &g, GeneratorRuntime &rt, uint32_t dt) {
    if (g.sync_mode == LFO_SYNC_FREE) {
        // Avance la phase (0..65535, wrap naturel sur uint16_t) au rythme
        // de rate_x10hz (0.1 Hz par unité) sur le delta-temps écoulé.
        uint32_t inc = (uint32_t)65536u * g.rate_x10hz * dt / 10000u;
        rt.phase_acc = (rt.phase_acc + inc) & 0xFFFFu;
        uint16_t raw = lfo_waveform_value(g.waveform, (uint16_t)rt.phase_acc, rt);
        uint8_t  cc  = lfo_output_value(raw, g.center, g.depth);
        if (cc != rt.last_sent_value) {
            rt.pending       = true;
            rt.pending_value = cc;
        }
    }
    // Mode LFO_SYNC_CLOCK : rt.pending est déjà positionné par
    // generators_on_clock_tick() (logic.h), appelée depuis bpm_push_clock().

    if (rt.pending) {
        generators_send_cc(i, rt.pending_value);
        rt.last_sent_value = rt.pending_value;
        rt.pending         = false;
    }
}

static inline void _gen_flush_euclid(uint8_t i, Generator &g, GeneratorRuntime &rt, uint32_t dt, uint32_t now_ms) {
    if (g.sync_mode == LFO_SYNC_FREE && g.euclid_steps > 0) {
        // Avance la phase du pas courant au rythme de rate_x10hz interprété
        // comme "pas/seconde" (même formule que le LFO, unité différente).
        uint32_t inc       = (uint32_t)65536u * g.rate_x10hz * dt / 10000u;
        uint32_t new_phase = rt.phase_acc + inc;
        bool     wrapped   = new_phase >= 65536u;   // au moins un pas franchi
        rt.phase_acc       = new_phase & 0xFFFFu;

        // Fermeture du gate en cours si son échéance est passée (indépendant du wrap de pas).
        if (rt.gate_active && now_ms >= rt.gate_off_ms) {
            rt.off_note         = rt.active_note;
            rt.pending_note_off = true;
            rt.gate_active       = false;
        }

        if (wrapped) {
            uint16_t pattern = euclid_pattern(g.euclid_steps, g.euclid_pulses, g.euclid_rotation);
            bool hit = (pattern >> rt.euclid_step) & 1u;
            if (hit) {
                if (rt.gate_active) {   // gate encore ouvert : couper avant le retrigger
                    rt.off_note         = rt.active_note;
                    rt.pending_note_off = true;
                }
                rt.active_note     = g.note;
                rt.pending_note_on = true;
                rt.gate_active      = true;
                uint32_t step_ms = 10000u / g.rate_x10hz;   // durée d'un pas en ms
                uint32_t gate_ms = step_ms * g.gate_percent / 100u;
                rt.gate_off_ms = now_ms + (gate_ms == 0 ? 1 : gate_ms);
            }
            rt.euclid_step++;
            if (rt.euclid_step >= g.euclid_steps) rt.euclid_step = 0;
        }
    }
    // Mode LFO_SYNC_CLOCK : pending_note_on/off déjà armés par generators_on_clock_tick() (logic.h).

    // OFF avant ON : couvre le cas retrigger (une note encore ouverte doit
    // s'éteindre avant que la suivante ne parte), cf. logic.h.
    if (rt.pending_note_off) {
        generators_send_note_off(i, rt.off_note);
        rt.pending_note_off = false;
    }
    if (rt.pending_note_on) {
        generators_send_note_on(i, rt.active_note, EUCLID_VELOCITY);
        rt.pending_note_on = false;
    }
}

// GEN_PATTERN — pas de mode libre (toujours synchro) : rien à avancer ici,
// juste vider les voix armées par generators_on_clock_tick() (logic.h).
// Callable sans condition sur transport_running : rien n'est jamais armé de
// nouveau pendant l'arrêt (cf. garde dans generators_on_clock_tick()), donc
// il n'y a que d'éventuels Note OFF résiduels (generators_on_transport_stop())
// à vider — même logique que la branche "transport à l'arrêt" pour l'euclidien.
static inline void _gen_flush_pattern(uint8_t i, GeneratorRuntime &rt) {
    // OFF avant ON sur toutes les voix (cohérent avec le reste du moteur —
    // cf. retrigger euclidien ci-dessus).
    for (uint8_t v = 0; v < PATTERN_MAX_VOICES; v++) {
        PatternVoice &voice = rt.voices[v];
        if (voice.pending_off) {
            generators_send_note_off(i, voice.off_note);
            voice.pending_off = false;
        }
    }
    for (uint8_t v = 0; v < PATTERN_MAX_VOICES; v++) {
        PatternVoice &voice = rt.voices[v];
        if (voice.pending_on) {
            generators_send_note_on(i, voice.note, voice.velocity);
            voice.pending_on = false;
        }
    }
}

inline void generators_tick(uint32_t now_ms) {
    static uint32_t last_ms = 0;
    uint32_t dt = now_ms - last_ms;
    if (dt < GEN_TICK_MS) return;
    last_ms = now_ms;

    for (uint8_t i = 0; i < gen_count; i++) {
        Generator        &g  = gen_list[i];
        GeneratorRuntime &rt = gen_rt[i];
        if (!g.active) continue;

        if (g.type == GEN_PATTERN) {
            // Pas de mode libre : toujours vider les voix, transport à
            // l'arrêt ou non (cf. commentaire de _gen_flush_pattern).
            _gen_flush_pattern(i, rt);
            continue;
        }

        if (transport_running) {
            if (g.type == GEN_LFO) _gen_flush_lfo(i, g, rt, dt);
            else                   _gen_flush_euclid(i, g, rt, dt, now_ms);
        } else if (rt.pending_note_off) {
            // Transport à l'arrêt : ne rien avancer/envoyer de nouveau, mais
            // toujours vider une Note OFF déjà armée (par ex. par
            // generators_on_transport_stop(), logic.h) — jamais de note bloquée.
            generators_send_note_off(i, rt.off_note);
            rt.pending_note_off = false;
        }
    }
}
