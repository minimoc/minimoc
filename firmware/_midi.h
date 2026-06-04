#pragma once

    
// Câbles symétriques : même numéro T→PC (miroir) et PC→T (inject/direct)
const int USB_PORT_A   = 1;    // Entrée A
const int USB_PORT_B   = 2;    // Entrée B
const int USB_IN_C     = 3;    // Entrée C  (USB Host 1)
const int USB_IN_D     = 4;    // Entrée D  (USB Host 2)
const int USB_IN_E     = 5;    // Entrée E  (USB Host 3)
const int USB_OUT_1    = 6;    // Sortie physique 1
const int USB_OUT_2    = 7;    // Sortie physique 2
const int USB_OUT_3    = 8;    // Sortie physique 3
const int USB_OUT_4    = 9;    // Sortie physique 4
const int USB_OUT_5    = 10;   // Sortie physique 5
const int USB_OUT_6    = 11;   // Sortie physique 6
const int USB_OUT_7    = 12;   // Sortie 7  (USB Host 1)
const int USB_OUT_8    = 13;   // Sortie 8  (USB Host 2)
const int USB_OUT_9    = 14;   // Sortie 9  (USB Host 3)

// Codes source enregistrement SD
#define SRC_PC 0
#define SRC_A  1
#define SRC_B  2
#define SRC_C  3
#define SRC_D  4
#define SRC_E  5
// Alias rétrocompatibilité
#define SRC_USBH SRC_C

// ----------------------------------------------------------------
// Périphériques USB HOST — USBHost_t36 assigne en ordre FIFO :
// le premier déclaré est le premier à revendiquer un device.
// L'assignation port logique ↔ slot physique se configure via HOST CONFIG.
//   → 1er device  → MIDIUSB1 (slot 0)   → 2ème → MIDIUSB2 (slot 1)
//   → 3ème device → MIDIUSB3 (slot 2)   → 4ème → MIDIUSB4 (slot 3)
//   → 5ème device → MIDIUSB5 (slot 4)   → 6ème → MIDIUSB6 (slot 5)
// ----------------------------------------------------------------
USBHost myusb;
USBHub  hub1(myusb);   // recommandation Teensy : 3 instances pour
USBHub  hub2(myusb);   // robustesse des reconnexions, hubs multi-TT
USBHub  hub3(myusb);   // et hubs USB 3.0 à double interface

MIDIDevice_BigBuffer MIDIUSB1(myusb);    // slot 0
MIDIDevice_BigBuffer MIDIUSB2(myusb);    // slot 1
MIDIDevice_BigBuffer MIDIUSB3(myusb);    // slot 2
MIDIDevice_BigBuffer MIDIUSB4(myusb);    // slot 3
MIDIDevice_BigBuffer MIDIUSB5(myusb);    // slot 4
MIDIDevice_BigBuffer MIDIUSB6(myusb);    // slot 5

// MINI IN A et B
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDIA);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIB);

// MIDI OUT 1 à 6
MIDI_CREATE_INSTANCE(HardwareSerial, Serial8, MIDI1);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial3, MIDI2);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial7, MIDI3);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial6, MIDI4);
//MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI5);
//MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI6);

const int NUM_OUTPUTS = 9;

// ----------------------------------------------------------------
// Dynamic USB output dispatch — uses port_slot[] from logic.h
// out_port_idx : 0 = sortie 7, 1 = sortie 8, 2 = sortie 9
// ----------------------------------------------------------------
static inline MIDIDevice_BigBuffer* _usb_out_dev(uint8_t out_port_idx) {
    uint8_t s = port_slot[3 + out_port_idx];
    if (s == HC_NO_SLOT || s >= HC_SLOT_COUNT) return nullptr;
    switch (s) {
        case 0: return &MIDIUSB1;
        case 1: return &MIDIUSB2;
        case 2: return &MIDIUSB3;
        case 3: return &MIDIUSB4;
        case 4: return &MIDIUSB5;
        case 5: return &MIDIUSB6;
    }
    return nullptr;
}

// ----------------------------------------------------------------
// SYNC — forward real-time depuis le port maître vers toutes les sorties
// ----------------------------------------------------------------
static inline void _sync_forward(byte msgType) {
    midi::MidiType t = (midi::MidiType)msgType;
    MIDI1.sendRealTime(t); MIDI2.sendRealTime(t);
    MIDI3.sendRealTime(t); MIDI4.sendRealTime(t);
    MIDI5.sendRealTime(t); MIDI6.sendRealTime(t);
    for (uint8_t i = 0; i < 3; i++) {
        MIDIDevice_BigBuffer* d = _usb_out_dev(i);
        if (d && (bool)*d) d->sendRealTime(msgType);
    }
}

// Callback real-time pour les devices USB Host (slots 0-5)
// Vérifie si ce slot est le maître SYNC et forward si oui.
static inline void _usb_rt(uint8_t slot, byte msg) {
    if (sync_master == 0xFF) return;  // OFF : tout filtré
    for (uint8_t p = 0; p < 3; p++) {
        if (port_slot[p] == slot && sync_master == (uint8_t)(p + 2)) {
            _sync_forward(msg); return;
        }
    }
}
void handleRT_dev0(byte m) { _usb_rt(0, m); }
void handleRT_dev1(byte m) { _usb_rt(1, m); }
void handleRT_dev2(byte m) { _usb_rt(2, m); }
void handleRT_dev3(byte m) { _usb_rt(3, m); }
void handleRT_dev4(byte m) { _usb_rt(4, m); }
void handleRT_dev5(byte m) { _usb_rt(5, m); }

// Lookup tables pour callbacks d'entrée USB Host
static const uint8_t HC_IN_CABLE[]  = {USB_IN_C, USB_IN_D, USB_IN_E};
static const uint8_t HC_IN_MIDIIN[] = {3, 4, 5};   // midiIn 1-indexé (A=1,B=2,C=3,D=4,E=5)
static const uint8_t HC_IN_SRC[]    = {SRC_C, SRC_D, SRC_E};

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

void processPitchBend(byte portSource, byte channel, int bend, byte source);
void processAfterTouch(byte portSource, byte channel, byte pressure, byte source);

// ----------------------------------------------------------------
// Macro d'envoi sur une sortie (index 0-7)
// Évite de dupliquer 8 cases dans chaque handler
// ----------------------------------------------------------------
#define SEND_NOTE_ON(out, note, vel, dc, src) \
    switch(out) { \
        case 0: MIDI1.sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_1); break; \
        case 1: MIDI2.sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_2); break; \
        case 2: MIDI3.sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_3); break; \
        case 3: MIDI4.sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_4); break; \
        case 4: MIDI5.sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_5); break; \
        case 5: MIDI6.sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_6); break; \
        case 6: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(0); if(_ud&&(bool)*_ud) _ud->sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_7); break; } \
        case 7: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(1); if(_ud&&(bool)*_ud) _ud->sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_8); break; } \
        case 8: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(2); if(_ud&&(bool)*_ud) _ud->sendNoteOn(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOn(note,vel,dc,USB_OUT_9); break; } \
    }

#define SEND_NOTE_OFF(out, note, vel, dc, src) \
    switch(out) { \
        case 0: MIDI1.sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_1); break; \
        case 1: MIDI2.sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_2); break; \
        case 2: MIDI3.sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_3); break; \
        case 3: MIDI4.sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_4); break; \
        case 4: MIDI5.sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_5); break; \
        case 5: MIDI6.sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_6); break; \
        case 6: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(0); if(_ud&&(bool)*_ud) _ud->sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_7); break; } \
        case 7: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(1); if(_ud&&(bool)*_ud) _ud->sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_8); break; } \
        case 8: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(2); if(_ud&&(bool)*_ud) _ud->sendNoteOff(note,vel,dc); if(src!=SRC_PC) usbMIDI.sendNoteOff(note,vel,dc,USB_OUT_9); break; } \
    }

// ----------------------------------------------------------------
// NoteOn
// ----------------------------------------------------------------
void handleNoteOn(byte midiIn, byte channel, byte note, byte velocity, byte source) {
    for (int out = 0; out < NUM_OUTPUTS; out++) {
        uint16_t mask = route_matrix[midiIn-1][channel-1][out];
        for (uint16_t m = mask; m; m &= m-1) {
            byte dc = __builtin_ctz(m) + 1;
            SEND_NOTE_ON(out, note, velocity, dc, source);
            rec_push(source, out+1, 0x90, dc, note, velocity);
        }
    }
}

// ----------------------------------------------------------------
// NoteOff
// ----------------------------------------------------------------
void handleNoteOff(byte midiIn, byte channel, byte note, byte velocity, byte source) {
    for (int out = 0; out < NUM_OUTPUTS; out++) {
        uint16_t mask = route_matrix[midiIn-1][channel-1][out];
        for (uint16_t m = mask; m; m &= m-1) {
            byte dc = __builtin_ctz(m) + 1;
            SEND_NOTE_OFF(out, note, velocity, dc, source);
            rec_push(source, out+1, 0x80, dc, note, velocity);
        }
    }
}

void handleNoteOnA(byte ch, byte note, byte vel)  { mon_push(SRC_A,0,vel); handleNoteOn(1,ch,note,vel,SRC_A); }
void handleNoteOffA(byte ch, byte note, byte vel) { mon_push(SRC_A,0,vel); handleNoteOff(1,ch,note,vel,SRC_A); }
void handleNoteOnB(byte ch, byte note, byte vel)  { mon_push(SRC_B,0,vel); handleNoteOn(2,ch,note,vel,SRC_B); }
void handleNoteOffB(byte ch, byte note, byte vel) { mon_push(SRC_B,0,vel); handleNoteOff(2,ch,note,vel,SRC_B); }

// ----------------------------------------------------------------
// ControlChange
// ----------------------------------------------------------------
void handleControlChange(byte midiIn, byte channel, byte number, byte value, byte source) {
    for (int out = 0; out < NUM_OUTPUTS; out++) {
        uint16_t mask = route_matrix[midiIn-1][channel-1][out];
        for (uint16_t m = mask; m; m &= m-1) {
            byte dc = __builtin_ctz(m) + 1;
            switch(out) {
                case 0: MIDI1.sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_1); break;
                case 1: MIDI2.sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_2); break;
                case 2: MIDI3.sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_3); break;
                case 3: MIDI4.sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_4); break;
                case 4: MIDI5.sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_5); break;
                case 5: MIDI6.sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_6); break;
                case 6: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(0); if(_ud&&(bool)*_ud) _ud->sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_7); break; }
                case 7: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(1); if(_ud&&(bool)*_ud) _ud->sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_8); break; }
                case 8: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(2); if(_ud&&(bool)*_ud) _ud->sendControlChange(number,value,dc); usbMIDI.sendControlChange(number,value,dc,USB_OUT_9); break; }
            }
            rec_push(source, out+1, 0xB0, dc, number, value);
        }
    }
}

void handleControlChangeA(byte ch, byte num, byte val) { mon_push(SRC_A,0,val); handleControlChange(1,ch,num,val,SRC_A); }
void handleControlChangeB(byte ch, byte num, byte val) { mon_push(SRC_B,0,val); handleControlChange(2,ch,num,val,SRC_B); }

// ----------------------------------------------------------------
// ProgramChange
// ----------------------------------------------------------------
void handleProgramChange(byte midiIn, byte channel, byte number, byte source) {
    for (int out = 0; out < NUM_OUTPUTS; out++) {
        uint16_t mask = route_matrix[midiIn-1][channel-1][out];
        for (uint16_t m = mask; m; m &= m-1) {
            byte dc = __builtin_ctz(m) + 1;
            switch(out) {
                case 0: MIDI1.sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_1); break;
                case 1: MIDI2.sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_2); break;
                case 2: MIDI3.sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_3); break;
                case 3: MIDI4.sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_4); break;
                case 4: MIDI5.sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_5); break;
                case 5: MIDI6.sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_6); break;
                case 6: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(0); if(_ud&&(bool)*_ud) _ud->sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_7); break; }
                case 7: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(1); if(_ud&&(bool)*_ud) _ud->sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_8); break; }
                case 8: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(2); if(_ud&&(bool)*_ud) _ud->sendProgramChange(number,dc); usbMIDI.sendProgramChange(number,dc,USB_OUT_9); break; }
            }
            rec_push(source, out+1, 0xC0, dc, number, 0);
        }
    }
}

void handleProgramChangeA(byte ch, byte num) { mon_push(SRC_A,0,80); handleProgramChange(1,ch,num,SRC_A); }
void handleProgramChangeB(byte ch, byte num) { mon_push(SRC_B,0,80); handleProgramChange(2,ch,num,SRC_B); }

// ----------------------------------------------------------------
// USB HOST callbacks — dispatch dynamique par slot (port_slot[])
// ----------------------------------------------------------------
// Macro helper : met à jour le VU-mètre entrée dès réception, sans attendre le routage
#define _USB_MON_IN(p, val) mon_push(HC_IN_SRC[p], 0, (val))

static inline void _usb_note_on(uint8_t s, byte ch, byte n, byte v) {
    for (uint8_t p=0; p<3; p++) if (port_slot[p]==s) {
        usbMIDI.sendNoteOn(n,v,ch,HC_IN_CABLE[p]);
        _USB_MON_IN(p, v);
        handleNoteOn(HC_IN_MIDIIN[p],ch,n,v,HC_IN_SRC[p]); return;
    }
}
static inline void _usb_note_off(uint8_t s, byte ch, byte n, byte v) {
    for (uint8_t p=0; p<3; p++) if (port_slot[p]==s) {
        usbMIDI.sendNoteOff(n,v,ch,HC_IN_CABLE[p]);
        _USB_MON_IN(p, v);
        handleNoteOff(HC_IN_MIDIIN[p],ch,n,v,HC_IN_SRC[p]); return;
    }
}
static inline void _usb_cc(uint8_t s, byte ch, byte num, byte val) {
    for (uint8_t p=0; p<3; p++) if (port_slot[p]==s) {
        usbMIDI.sendControlChange(num,val,ch,HC_IN_CABLE[p]);
        _USB_MON_IN(p, val);
        handleControlChange(HC_IN_MIDIIN[p],ch,num,val,HC_IN_SRC[p]); return;
    }
}
static inline void _usb_pc(uint8_t s, byte ch, byte num) {
    for (uint8_t p=0; p<3; p++) if (port_slot[p]==s) {
        usbMIDI.sendProgramChange(num,ch,HC_IN_CABLE[p]);
        _USB_MON_IN(p, 80);
        handleProgramChange(HC_IN_MIDIIN[p],ch,num,HC_IN_SRC[p]); return;
    }
}
static inline void _usb_pb(uint8_t s, byte ch, int bend) {
    for (uint8_t p=0; p<3; p++) if (port_slot[p]==s) {
        usbMIDI.sendPitchBend(bend,ch,HC_IN_CABLE[p]);
        _USB_MON_IN(p, 80);
        processPitchBend(HC_IN_MIDIIN[p]-1,ch,bend,HC_IN_SRC[p]); return;
    }
}
static inline void _usb_at(uint8_t s, byte ch, byte pres) {
    for (uint8_t p=0; p<3; p++) if (port_slot[p]==s) {
        usbMIDI.sendAfterTouch(pres,ch,HC_IN_CABLE[p]);
        _USB_MON_IN(p, pres);
        processAfterTouch(HC_IN_MIDIIN[p]-1,ch,pres,HC_IN_SRC[p]); return;
    }
}

void handleNoteOn_dev0(byte ch,byte n,byte v)  { _usb_note_on(0,ch,n,v); }
void handleNoteOn_dev1(byte ch,byte n,byte v)  { _usb_note_on(1,ch,n,v); }
void handleNoteOn_dev2(byte ch,byte n,byte v)  { _usb_note_on(2,ch,n,v); }
void handleNoteOn_dev3(byte ch,byte n,byte v)  { _usb_note_on(3,ch,n,v); }
void handleNoteOn_dev4(byte ch,byte n,byte v)  { _usb_note_on(4,ch,n,v); }
void handleNoteOn_dev5(byte ch,byte n,byte v)  { _usb_note_on(5,ch,n,v); }
void handleNoteOff_dev0(byte ch,byte n,byte v) { _usb_note_off(0,ch,n,v); }
void handleNoteOff_dev1(byte ch,byte n,byte v) { _usb_note_off(1,ch,n,v); }
void handleNoteOff_dev2(byte ch,byte n,byte v) { _usb_note_off(2,ch,n,v); }
void handleNoteOff_dev3(byte ch,byte n,byte v) { _usb_note_off(3,ch,n,v); }
void handleNoteOff_dev4(byte ch,byte n,byte v) { _usb_note_off(4,ch,n,v); }
void handleNoteOff_dev5(byte ch,byte n,byte v) { _usb_note_off(5,ch,n,v); }
void handleCC_dev0(byte ch,byte num,byte val)  { _usb_cc(0,ch,num,val); }
void handleCC_dev1(byte ch,byte num,byte val)  { _usb_cc(1,ch,num,val); }
void handleCC_dev2(byte ch,byte num,byte val)  { _usb_cc(2,ch,num,val); }
void handleCC_dev3(byte ch,byte num,byte val)  { _usb_cc(3,ch,num,val); }
void handleCC_dev4(byte ch,byte num,byte val)  { _usb_cc(4,ch,num,val); }
void handleCC_dev5(byte ch,byte num,byte val)  { _usb_cc(5,ch,num,val); }
void handlePC_dev0(byte ch,byte num)           { _usb_pc(0,ch,num); }
void handlePC_dev1(byte ch,byte num)           { _usb_pc(1,ch,num); }
void handlePC_dev2(byte ch,byte num)           { _usb_pc(2,ch,num); }
void handlePC_dev3(byte ch,byte num)           { _usb_pc(3,ch,num); }
void handlePC_dev4(byte ch,byte num)           { _usb_pc(4,ch,num); }
void handlePC_dev5(byte ch,byte num)           { _usb_pc(5,ch,num); }
void handlePB_dev0(byte ch,int b)              { _usb_pb(0,ch,b); }
void handlePB_dev1(byte ch,int b)              { _usb_pb(1,ch,b); }
void handlePB_dev2(byte ch,int b)              { _usb_pb(2,ch,b); }
void handlePB_dev3(byte ch,int b)              { _usb_pb(3,ch,b); }
void handlePB_dev4(byte ch,int b)              { _usb_pb(4,ch,b); }
void handlePB_dev5(byte ch,int b)              { _usb_pb(5,ch,b); }
void handleAT_dev0(byte ch,byte p)             { _usb_at(0,ch,p); }
void handleAT_dev1(byte ch,byte p)             { _usb_at(1,ch,p); }
void handleAT_dev2(byte ch,byte p)             { _usb_at(2,ch,p); }
void handleAT_dev3(byte ch,byte p)             { _usb_at(3,ch,p); }
void handleAT_dev4(byte ch,byte p)             { _usb_at(4,ch,p); }
void handleAT_dev5(byte ch,byte p)             { _usb_at(5,ch,p); }

// ----------------------------------------------------------------
// Clock forwarding helper
// ----------------------------------------------------------------
bool destMidiOn(char input, char output) {
    for (int ch = 0; ch < 16; ch++) {
        if (route_matrix[input-1][ch][output-1]) return true;
    }
    return false;
}

// ----------------------------------------------------------------
// PitchBend
// ----------------------------------------------------------------
void processPitchBend(byte portSource, byte channel, int bend, byte source) {
    for (int out = 0; out < NUM_OUTPUTS; out++) {
        uint16_t mask = route_matrix[portSource][channel-1][out];
        for (uint16_t m = mask; m; m &= m-1) {
            byte dc = __builtin_ctz(m) + 1;
            switch(out) {
                case 0: MIDI1.sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_1); break;
                case 1: MIDI2.sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_2); break;
                case 2: MIDI3.sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_3); break;
                case 3: MIDI4.sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_4); break;
                case 4: MIDI5.sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_5); break;
                case 5: MIDI6.sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_6); break;
                case 6: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(0); if(_ud&&(bool)*_ud) _ud->sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_7); break; }
                case 7: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(1); if(_ud&&(bool)*_ud) _ud->sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_8); break; }
                case 8: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(2); if(_ud&&(bool)*_ud) _ud->sendPitchBend(bend,dc); usbMIDI.sendPitchBend(bend,dc,USB_OUT_9); break; }
            }
            uint16_t raw = (uint16_t)(bend + 8192);
            rec_push(source, out+1, 0xE0, dc, raw & 0x7F, (raw>>7) & 0x7F);
        }
    }
}

void handlePitchBendA(byte ch, int bend) { mon_push(SRC_A,0,80); processPitchBend(0,ch,bend,SRC_A); }
void handlePitchBendB(byte ch, int bend) { mon_push(SRC_B,0,80); processPitchBend(1,ch,bend,SRC_B); }

// ----------------------------------------------------------------
// AfterTouch
// ----------------------------------------------------------------
void processAfterTouch(byte portSource, byte channel, byte pressure, byte source) {
    for (int out = 0; out < NUM_OUTPUTS; out++) {
        uint16_t mask = route_matrix[portSource][channel-1][out];
        for (uint16_t m = mask; m; m &= m-1) {
            byte dc = __builtin_ctz(m) + 1;
            switch(out) {
                case 0: MIDI1.sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_1); break;
                case 1: MIDI2.sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_2); break;
                case 2: MIDI3.sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_3); break;
                case 3: MIDI4.sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_4); break;
                case 4: MIDI5.sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_5); break;
                case 5: MIDI6.sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_6); break;
                case 6: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(0); if(_ud&&(bool)*_ud) _ud->sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_7); break; }
                case 7: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(1); if(_ud&&(bool)*_ud) _ud->sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_8); break; }
                case 8: { MIDIDevice_BigBuffer* _ud=_usb_out_dev(2); if(_ud&&(bool)*_ud) _ud->sendAfterTouch(pressure,dc); usbMIDI.sendAfterTouch(pressure,dc,USB_OUT_9); break; }
            }
            rec_push(source, out+1, 0xD0, dc, pressure, 0);
        }
    }
}

void handleAfterTouchA(byte ch, byte pressure) { mon_push(SRC_A,0,pressure); processAfterTouch(0,ch,pressure,SRC_A); }
void handleAfterTouchB(byte ch, byte pressure) { mon_push(SRC_B,0,pressure); processAfterTouch(1,ch,pressure,SRC_B); }
