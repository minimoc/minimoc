// ============================================================
//  MiniMoc — BOÎTIER FUSIONNÉ (fond + anneau en une pièce)
//  ------------------------------------------------------------
//  Ce fichier NE MODIFIE PAS ta géométrie. Il contient tes deux
//  pièces telles quelles (l'anneau a juste ses noms préfixés
//  "a_" pour éviter les conflits avec le fond), puis les
//  assemble :
//    - fond() à sa place
//    - anneau retourné 180° autour de Y, recalé en X, posé à
//      Z = 2.4 mm (sur la plaque du fond)
//
//  Pour exporter : F6 puis Exporter STL.
// ============================================================

$fn = 64;

// ---------- PIÈCE A : LE FOND (inchangé) --------------------
// ============================================================
//  MiniMoc — Boîtier imprimé en 3D
//  PIÈCE 1/2 : LE FOND (plaque inférieure)
//  ------------------------------------------------------------
//  Repère : coin inférieur-gauche du PCB = origine (0,0),
//  comme dans KiCad. Cotes en millimètres.
//
//  Cotes extraites du PCB MiniMoc v0.2 :
//    - Contour des cartes ......... 99 x 80 mm, coins R5
//    - 4 trous M3 aux coins ....... (5,5) (94,5) (5,75) (94,75)
//
//  IMPORTANT : le fond et l'anneau partagent la MÊME empreinte
//  extérieure (PCB + jeu + murs). Les deux dépassent donc
//  légèrement du PCB, du même montant, pour coïncider bord à
//  bord. Le PCB, lui, reste à 99 x 80 à l'intérieur.
//
//  OpenSCAD : F5 = aperçu, F6 = rendu, puis
//  Fichier > Exporter > Exporter en STL.
// ============================================================


// ---------- PARAMÈTRES À AJUSTER ----------------------------

pcb_l        = 99;    // longueur du PCB (axe X)
pcb_w        = 80;    // largeur du PCB  (axe Y)
coin_r       = 5;     // rayon des coins du PCB (R5)

// -- Murs & jeu : CES DEUX VALEURS définissent l'empreinte --
//    extérieure, partagée avec l'anneau. Garde-les identiques
//    dans les deux fichiers.
mur_ep       = 1.6;   // épaisseur des murs de l'anneau
jeu_pcb      = 0.4;   // jeu entre le PCB et l'intérieur des murs

// => empreinte extérieure de la coque (calculée, ne pas éditer)
ext_l        = pcb_l + 2*(jeu_pcb + mur_ep);   // ~103.0
ext_w        = pcb_w + 2*(jeu_pcb + mur_ep);   // ~84.0
ext_coin_r   = coin_r + jeu_pcb + mur_ep;      // arrondi extérieur

fond_ep      = 2.4;   // épaisseur de la plaque de fond

// -- Colonnes de vis aux 4 coins (positions = repère PCB) --
trou_x1      = 5;
trou_x2      = 94;
trou_y1      = 5;
trou_y2      = 75;

colonne_d    = 8;     // diamètre des bossages de vis
colonne_h    = 2;     // hauteur des bossages (surélève le PCB,
                      //  dégage les soudures du dessous)

// -- Type de fixation --
//   "taraude" = vis M3 taraude le plastique (Ø2.7)
//   "insert"  = trou pour insert thermofusible M3 (Ø4.0)
fixation     = "insert";

trou_d_taraude = 2.9;
trou_d_insert  = 4.0;

// -- Lamage tête de vis sous la plaque --
// -- Lamage tête de vis sous le fond (vis Allen tête bombée M3) --
// La vis monte PAR LE DESSOUS ; sa tête bombée se loge dans ce
// logement cylindrique creusé sous le fond pour ne pas dépasser.
lamage       = true;
lamage_d     = 6.2;   // Ø tête bombée M3 (~5.7) + jeu impression
lamage_h     = 2.0;   // profondeur (tête bombée M3 ~1.65 de haut)

$fn = 48;


// ---------- CONSTRUCTION ------------------------------------

// Profil arrondi générique (l, w, r) centré sur le repère PCB.
// Le décalage place le contour extérieur autour du PCB.
module profil(l, w, r, ox, oy) {
    translate([ox, oy, 0])
    hull() {
        translate([r,       r    ]) circle(r);
        translate([l - r,   r    ]) circle(r);
        translate([r,       w - r]) circle(r);
        translate([l - r,   w - r]) circle(r);
    }
}

// Décalage du contour extérieur par rapport à l'origine PCB
off = -(jeu_pcb + mur_ep);

trou_d = (fixation == "insert") ? trou_d_insert : trou_d_taraude;

positions = [
    [trou_x1, trou_y1], [trou_x2, trou_y1],
    [trou_x1, trou_y2], [trou_x2, trou_y2]
];

module fond() {
    difference() {
        union() {
            // plaque à l'empreinte EXTÉRIEURE (partagée avec l'anneau)
            linear_extrude(fond_ep)
                profil(ext_l, ext_w, ext_coin_r, off, off);
            // colonnes de vis
            for (p = positions)
                translate([p[0], p[1], 0])
                    cylinder(d = colonne_d, h = fond_ep + colonne_h);
        }
        // perçage des trous de vis
        for (p = positions)
            translate([p[0], p[1], -1])
                cylinder(d = trou_d, h = fond_ep + colonne_h + 2);
        // lamage optionnel
        if (lamage)
            for (p = positions)
                translate([p[0], p[1], -0.01])
                    cylinder(d = lamage_d, h = lamage_h);
    }
}



// ---------- PIÈCE B : L'ANNEAU (noms préfixés a_, inchangé) --
// ============================================================
//  MiniMoc — Boîtier imprimé en 3D
//  PIÈCE 2/2 : L'ANNEAU (cadre + 2 épaulements + découpes)
//  ------------------------------------------------------------
//  Repère KiCad : coin HAUT-gauche du PCB = origine (0,0),
//  axe X vers la droite, axe Y vers le BAS. Cotes en mm.
//
//  STRUCTURE (coupe en "H" couché) :
//    - débord extérieur = ton liseré autour des PCB
//    - épaulement BAS  : repose SUR le PCB du bas
//    - ouverture centrale : hauteur = entretoises (11 mm)
//    - épaulement HAUT : reçoit le PCB du haut
//
//  FIXATION : 4 trous ronds laissent passer les entretoises
//  hexagonales aux coins -> bloquent l'a_anneau en X/Y. Les deux
//  épaulements + les PCB le bloquent en Z.
//
//  Murs (repère) :
//    - bord y=0 -> FACE AVANT : 6 jacks de sortie
//    - bord x=0 -> FACE GAUCHE : 2 entrées + USB-A + micro-USB Teensy
//
//  Positions connecteurs & entretoises : extraites du PCB. NE PAS toucher.
//  Cotes à confirmer = bloc MESURES.
//  OpenSCAD : F5 aperçu, F6 rendu, export STL.
// ============================================================


// réglages fins FJA
a_decaltrs=0;
a_agrandir_anneau= 5.6;



// ---------- DOIT CORRESPONDRE AU FICHIER DU FOND ------------
a_pcb_l        = 99;
a_pcb_w        = 80;
a_coin_r       = 5;
a_mur_ep       = 1.6;    // épaisseur du mur extérieur (liseré)
a_jeu_pcb      = 0.4;    // jeu pour que le PCB entre sans forcer

// empreinte extérieure (identique au fond)
a_ext_l        = a_pcb_l + 2*(a_jeu_pcb + a_mur_ep);
a_ext_w        = a_pcb_w + 2*(a_jeu_pcb + a_mur_ep);
a_ext_coin_r   = a_coin_r + a_jeu_pcb + a_mur_ep;
a_off          = -(a_jeu_pcb + a_mur_ep);


// ---------- MESURES (confirmées avec toi) -------------------
a_entretoise_h = 11.0+a_agrandir_anneau;   // hauteur des entretoises = ouverture centrale
a_pcb_ep       = 1.6;    // épaisseur de tes PCB (feuillures)

// --- PATINS D'APPUI (au lieu d'un rebord continu) ---
// Chaque a_patin = petit plot sur lequel un PCB vient reposer.
// Format : [x, y, largeur, profondeur]  (x,y = centre, repère PCB)
// ÉDITE CETTE LISTE LIBREMENT : ajoute/déplace/supprime des lignes.
// Pré-rempli avec les 4 coins (zones dégagées). Décale-les vers
// l'intérieur si un composant gêne, réduis largeur/profondeur, etc.
//


// Attntion BAS = ceux du haut et HAUT = plus utilisés
// Patins du BAS (sous le PCB du bas — ATTENTION aux composants) :
a_patins_bas = [
    [12, 0, 6, 2],   // coin bas-gauche
    [99, 10, 2, 6],   // coin bas-droite
    [12, 80, 6, 2],   // coin haut-gauche
    [85, 80, 6, 2],   // coin haut-droite
];
// Patins du HAUT (sous le PCB du haut — carte nue, peu de contraintes) :
a_patins_haut = [
    [12, 2, 6, 6],
    [98, 10, 6, 6],
    [12, 78, 6, 6],
    [85, 78, 6, 6],
];

// hauteur totale de l'a_anneau = PCB bas + entretoises + PCB haut
a_anneau_h     = a_pcb_ep + a_entretoise_h + a_pcb_ep;   // = 14.2

// -- Entretoises hexagonales (passage aux 4 coins) --
a_entre_plats  = 4.0;    // dimension entre plats mesurée (4 mm)
a_ent_jeu      = 0.4;    // jeu d'impression
// largeur max de l'hexagone (pointe à pointe) = a_entre_plats / cos(30)
a_ent_trou_d   = a_entre_plats / cos(30) + a_ent_jeu;   // ~5.02


// ---------- MESURES connecteurs (au pied à coulisse) --------
a_jack_axe_z   = 6.0+a_agrandir_anneau;    // hauteur axe jack au-dessus du PCB bas
a_jack_trou_d  = 6.5;    // Ø trou fût du jack + jeu

a_usb_larg     = 14.0;   // ouverture USB-A
a_usb_haut     = 7.0;
a_usb_axe_z    = 6.0+a_agrandir_anneau;

a_tusb_larg    = 11.0;   // ouverture micro-USB Teensy (large pour la fiche)
a_tusb_haut    = 5.0;
a_tusb_axe_z   = 4.0+a_agrandir_anneau;

// Hauteur de référence : le PCB du bas est posé à z = a_pcb_ep
// (l'épaulement bas fait a_pcb_ep de haut). Les connecteurs sont
// au-dessus du PCB bas, donc leur z = a_pcb_ep + axe_z.
a_pcb_bas_z    = a_pcb_ep;


// ---------- POSITIONS (extraites du PCB) --------------------
a_jacks_out_x  = [35.12, 44.70, 54.28, 63.87, 73.45, 83.04]; // face avant
a_jacks_in_y   = [30.03, 21.53];                              // face gauche
a_usb_y        = 61.0;                                        // USB-A
a_tusb_y       = 47.12;                                       // micro-USB Teensy
a_ent_pos      = [[5,5],[94,5],[5,75],[94,75]];               // entretoises

$fn = 64;


// ---------- CONSTRUCTION ------------------------------------

module a_profil(l, w, r, ox, oy) {
    translate([ox, oy, 0])
    hull() {
        translate([r,     r    ]) circle(r);
        translate([l - r, r    ]) circle(r);
        translate([r,     w - r]) circle(r);
        translate([l - r, w - r]) circle(r);
    }
}

// Contour extérieur (liseré)
module a_ext_profil()  { a_profil(a_ext_l, a_ext_w, a_ext_coin_r, a_off, a_off); }
// Contour intérieur = PCB + jeu (l'ouverture où passent les composants)
module a_pcb_profil()  { a_profil(a_pcb_l + 2*a_jeu_pcb, a_pcb_w + 2*a_jeu_pcb,
                              a_coin_r + a_jeu_pcb, -a_jeu_pcb, -a_jeu_pcb); }
// Un a_patin = petit pavé. On le pose à la bonne hauteur via z.
module a_patin(p, z) {
    translate([p[0]-p[2]/2, p[1]-p[3]/2, z])
        cube([p[2], p[3], a_pcb_ep]);
}

// Anneau : mur extérieur creux (ouverture = taille PCB sur toute la
// hauteur), PLUS des patins d'appui en bas et en haut. Pas de rebord
// continu : l'intérieur reste dégagé sauf aux patins.
module a_anneau_brut() {
    union() {
        // 1) cadre extérieur creux : ouverture de la taille du PCB,
        //    traversante sur toute la hauteur
        difference() {
            linear_extrude(a_anneau_h) a_ext_profil();
            translate([0,0,-1])
                linear_extrude(a_anneau_h + 2) a_pcb_profil();
        }
        // 2) patins du bas (de z=0 à a_pcb_ep) : le PCB bas repose dessus
        for (p = a_patins_bas) a_patin(p, a_pcb_ep);
        // 3) patins du haut (juste sous le sommet) : le PCB haut repose dessus
        //for (p = a_patins_haut) a_patin(p, a_anneau_h - 2*a_pcb_ep-2);
    }
}

// --- découpes connecteurs ---
module a_trou_jack_avant(x) {
    translate([x, a_off - 1, a_pcb_bas_z + a_jack_axe_z])
        rotate([-90,0,0]) cylinder(d=a_jack_trou_d, h=(a_mur_ep+a_jeu_pcb)+3);
}
module a_trou_jack_gauche(y) {
    translate([a_off - 1, y, a_pcb_bas_z + a_jack_axe_z])
        rotate([0,90,0]) cylinder(d=a_jack_trou_d, h=(a_mur_ep+a_jeu_pcb)+3);
}
module a_trou_usb(y) {
    translate([a_off - 1, y - a_usb_larg/2, (a_pcb_bas_z+a_usb_axe_z) - a_usb_haut/2])
        cube([(a_mur_ep+a_jeu_pcb)+3, a_usb_larg, a_usb_haut]);
}
module a_trou_teensy_usb(y) {
    translate([a_off - 1, y - a_tusb_larg/2, (a_pcb_bas_z+a_tusb_axe_z) - a_tusb_haut/2])
        cube([(a_mur_ep+a_jeu_pcb)+3, a_tusb_larg, a_tusb_haut]);
}
// passage d'entretoise : trou rond vertical traversant TOUT l'a_anneau
module a_trou_entretoise(p) {
    translate([p[0], p[1], -1])
        cylinder(d=a_ent_trou_d, h=a_anneau_h + 2);
}

module a_anneau() {
    difference() {
        a_anneau_brut();
        for (x = a_jacks_out_x) a_trou_jack_avant(x+a_jack_trou_d/2-1);
        for (y = a_jacks_in_y)  a_trou_jack_gauche(y-a_jack_trou_d/2+1);
        a_trou_usb(a_usb_y+3);
        a_trou_teensy_usb(a_tusb_y);
        for (p = a_ent_pos) a_trou_entretoise(p);
    }
}

// ============================================================
//  COUPON DE TEST — décommente a_coupon() et commente a_anneau()
//  pour imprimer ~5 min : un bout de la face gauche avec un
//  jack ET le micro-USB, pour valider les hauteurs critiques.
// ============================================================
//
// module a_coupon() {
//     L = 45;
//     difference() {
//         cube([a_mur_ep + a_jeu_pcb, L, a_anneau_h]);
//         // un jack d'entrée (centré sur le a_coupon)
//         translate([-1, L*0.30, a_pcb_bas_z + a_jack_axe_z])
//             rotate([0,90,0]) cylinder(d=a_jack_trou_d, h=a_mur_ep+a_jeu_pcb+2);
//         // le micro-USB Teensy
//         translate([-1, L*0.68 - a_tusb_larg/2, (a_pcb_bas_z+a_tusb_axe_z)-a_tusb_haut/2])
//             cube([a_mur_ep+a_jeu_pcb+2, a_tusb_larg, a_tusb_haut]);
//     }
// }
// a_coupon();


// ---------- ASSEMBLAGE -------------------------------------
// ---------- ASSEMBLAGE -------------------------------------

// ---------- POSITION DE L'ANNEAU SUR LE FOND ---------------
// L'anneau est retourné 180° autour de Y (pour mettre les
// patins en haut). Ajuste ces 3 valeurs en regardant l'aperçu
// (F5) jusqu'à ce que l'anneau tombe pile sur le fond.
pos_x = 99;     // décalage horizontal (compense le retournement)
pos_y = 0;      // décalage en profondeur
pos_z = 2.4;    // hauteur : pose l'anneau sur la plaque du fond

union() {
    fond();
    translate([pos_x, pos_y, pos_z + a_anneau_h])
        rotate([0,180,0])
            anneau_assemble();
}

module anneau_assemble() {
    difference() {
        a_anneau_brut();
        for (x = a_jacks_out_x) a_trou_jack_avant(x + a_jack_trou_d/2 - 1);
        for (y = a_jacks_in_y)  a_trou_jack_gauche(y - a_jack_trou_d/2 + 1);
        a_trou_usb(a_usb_y + 3);
        a_trou_teensy_usb(a_tusb_y);
        for (p = a_ent_pos) a_trou_entretoise(p);
    }
}
