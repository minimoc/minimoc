#pragma once
#include "preset.h"

// -----------------------------------------------------------------
// STRUCTURES DE DONNÉES DU MENU
// -----------------------------------------------------------------

enum MenuItemType {
  MENU_NAVIGATION,    
  MENU_NUMERIC_VALUE, 
  MENU_TOGGLE_VALUE,  
  MENU_ACTION         
};

struct MenuItem {
  const char* label;          
  MenuItemType type;          
  struct MenuItem* submenu;   
  uint8_t* value;                 
  uint8_t min_val;                
  uint8_t max_val;                
  void (*action)();           
};

// -----------------------------------------------------------------
// VARIABLES D'ÉTAT DU SYSTÈME DE MENU & SCROLLING
// -----------------------------------------------------------------

#define MAX_DEPTH 4
MenuItem* menu_stack[MAX_DEPTH];
int stack_depth = 0;
MenuItem* current_menu;         
int selected_index = 0;         
bool is_editing_value = false;  

int current_input_port = 0;   
int current_input_channel = 0;

int top_visible_index = 0;      
const int MAX_DISPLAY_LINES = 6; 

// -----------------------------------------------------------------
// FONCTIONS D'ACTION (EEPROM)
// -----------------------------------------------------------------
void save_preset() {
  
  silent_save_preset();
  
  // Affichage
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Sauvegarde reussie !");
  char temp_str[30];
  sprintf(temp_str, "Preset #%u.", current_preset);
  u8g2.drawStr(0, 25, temp_str);
  u8g2.sendBuffer();
  delay(1500);
}

void load_preset() {

  silent_load_preset();

  // Affichage
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Chargement reussi !");
  char temp_str[30];
  sprintf(temp_str, "Preset #%u charge.", current_preset);
  u8g2.drawStr(0, 25, temp_str);
  u8g2.sendBuffer();
  delay(1500);

}

void show_info() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Mini Moc");
  char mem_str[30];
  //sprintf(mem_str, "Preset Size: %d octets.", PRESET_SIZE);
  //u8g2.drawStr(0, 25, mem_str);
  u8g2.drawStr(0, 25, "Version 1.1"); // 40 au lieu de 25
  u8g2.sendBuffer();
  delay(1500);
}

// -----------------------------------------------------------------
// DÉFINITION DES MENUS
// -----------------------------------------------------------------

int count_menu_items(MenuItem* menu) {
  int count = 0;
  if (menu == NULL) return 0;
  while (menu[count].label != NULL) {
    count++;
  }
  return count;
}

// Menu 4: Configuration des Plages (Canal 10)
MenuItem output_range_ch10_menu[] = {
  {"MIDI 1 - MIN", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL}, 
  {"MIDI 1 - MAX", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 2 - MIN", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 2 - MAX", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 3 - MIN", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 3 - MAX", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 4 - MIN", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 4 - MAX", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 5 - MIN", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 5 - MAX", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 6 - MIN", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 6 - MAX", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"USB - MIN",    MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"USB - MAX",    MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {NULL}
};

// Menu 3: Configuration des Sorties (Traduction de Canal 1-9 & 11-16)
MenuItem output_config_menu[] = {
  {"MIDI 1 -> Canal", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL}, 
  {"MIDI 2 -> Canal", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 3 -> Canal", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 4 -> Canal", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 5 -> Canal", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"MIDI 6 -> Canal", MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {"USB -> Canal",    MENU_NUMERIC_VALUE, NULL, NULL, 0, 16, NULL},
  {NULL}
};

// Menu 2: Canaux 
MenuItem input_a_menu[17]; 
MenuItem input_b_menu[17]; 

// Menu 1: Presets
MenuItem preset_menu[] = {
  {"Numero de Preset", MENU_NUMERIC_VALUE, NULL, &current_preset, 1, MAX_PRESETS, NULL},
  {"Sauvegarder", MENU_ACTION, NULL, NULL, 0, 0, save_preset},
  {"Charger", MENU_ACTION, NULL, NULL, 0, 0, load_preset},
  {NULL}
};

// Menu 0.5: Routage Principal
MenuItem routing_main_menu[] = {
  {"Routage Input A", MENU_NAVIGATION, input_a_menu, NULL, 0, 0, NULL},
  {"Routage Input B", MENU_NAVIGATION, input_b_menu, NULL, 0, 0, NULL},
  {NULL}
};

// Menu 0: Principal
MenuItem main_menu[] = {
  {"ROUTAGE MIDI", MENU_NAVIGATION, NULL, NULL, 0, 0, NULL}, 
  {"PRESETS -  ", MENU_NAVIGATION, preset_menu, NULL, 0, 0, NULL},
  {"STATUT/INFO", MENU_ACTION, NULL, NULL, 0, 0, show_info},
  {NULL}
};

// -----------------------------------------------------------------
// H. AFFICHAGE
// -----------------------------------------------------------------
void draw_menu() {
  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_6x10_tf); 
  const int LINE_HEIGHT = 10;
  const int START_Y = 10;
  int display_line = 0; 
  int max_items = count_menu_items(current_menu);
  
  for (int i = top_visible_index; i < max_items && display_line < MAX_DISPLAY_LINES; i++) {
    MenuItem* item = &current_menu[i];
    int y_pos = START_Y + (display_line * LINE_HEIGHT);
    
    // Curseur
    if (i == selected_index) {
      u8g2.drawStr(0, y_pos, is_editing_value ? "<" : ">"); 
    }
    
    // Libellé
    u8g2.drawStr(8, y_pos, item->label);


if (current_menu == main_menu && i == 1) { // Si on est sur le menu principal, ligne "PRESETS"
    char p_str[10];
    sprintf(p_str, " %u", current_preset);
    u8g2.drawStr(60, y_pos, p_str); // Affiche "#2" par exemple à côté du libellé
}

    // Valeur/Statut
    if (item->type != MENU_NAVIGATION && item->type != MENU_ACTION) {
      char val_str[10];
      
      if (*item->value == 0 && item->type == MENU_NUMERIC_VALUE) {
          sprintf(val_str, "OFF");
      } else {
          sprintf(val_str, "%u", *item->value);
      }
      
      int text_width = u8g2.getStrWidth(val_str);
      int x_pos_value = u8g2.getDisplayWidth() - text_width;
      
      // Mode Édition (inversion des couleurs)
      if (is_editing_value && i == selected_index) {
        u8g2.drawBox(x_pos_value - 2, y_pos - 9, text_width + 4, LINE_HEIGHT);
        u8g2.setDrawColor(0); 
        u8g2.drawStr(x_pos_value, y_pos, val_str);
        u8g2.setDrawColor(1); 
      } else {
        u8g2.drawStr(x_pos_value, y_pos, val_str);
      }
    }
    
    // Indicateur de sous-menu
    if (item->type == MENU_NAVIGATION) {
       u8g2.drawStr(u8g2.getDisplayWidth() - 5, y_pos, "+");
    }

    display_line++; 
  }

  u8g2.sendBuffer(); 
}


