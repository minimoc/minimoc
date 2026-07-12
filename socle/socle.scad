// ============================================================
//  MiniMoc — SOCLE INCLINÉ (profil en CALE)
//  ------------------------------------------------------------
//  Le boîtier se pose DEBOUT dans une gorge inclinée, penché de
//  `angle_vert` degrés depuis la verticale.
//  Jacks 1-6 en haut, entrées A/B à gauche.
//
//  PROFIL EN CALE : fin à l'avant, épais à l'arrière.
//  Gorge asymétrique :
//    - lèvre AVANT basse   -> passe SOUS les têtes de vis du PCB
//    - dossier ARRIÈRE haut -> le boîtier s'y appuie en penchant
//
//  La géométrie est CALCULÉE pour qu'il reste toujours de la
//  matière devant, derrière et sous la gorge (pas de zone
//  fragile, pas de pièce coupée en deux).
// ============================================================


// ---------- PARAMÈTRES ---------------------------------------

angle_vert   = 20;    // inclinaison depuis la VERTICALE (°)

// -- Boîtier --
boitier_l    = 103;   // largeur reposant dans la gorge
boitier_ep   = 22.2;  // épaisseur du boîtier

// -- Gorge --
gorge_jeu    = 0.6;   // jeu d'insertion
gorge_prof   = 14;    // profondeur d'encastrement du boîtier
gorge_h_av   = 6.0;   // hauteur de la lèvre AVANT
                      //  GARDER SOUS 4 mm (vis à 4-5 mm du bord)
gorge_marge  = 6.0;   // débord latéral de chaque côté = épaisseur des
                      //  JOUES qui bloquent le boîtier latéralement
gorge_jeu_lat= 0.8;   // jeu latéral (le boîtier doit glisser sans forcer)

// -- Épaisseurs de matière (solidité) --
ep_sous      = 4.0;   // matière SOUS le fond de la gorge
ep_avant     = 6.0;   // matière DEVANT la gorge (la lèvre)
ep_arriere   = 16.0;  // matière DERRIÈRE la gorge (lest/dossier)
                      //  AUGMENTER si le socle bascule en arrière

dossier_h    = 24;    // hauteur du dossier arrière

// -- Arrondis (confort + finition) --
arrondi_r    = 2.0;   // rayon d'arrondi des arêtes EXTÉRIEURES
arrondi_fn   = 16;    // finesse de l'arrondi (16 = bon compromis).
                      //  Monte à 24-32 pour l'export final si tu veux
                      //  plus lisse ; baisse à 8-12 si le rendu F6
                      //  est trop lent pendant que tu travailles.
                      //  (la gorge, elle, reste à angles VIFS pour
                      //   que le boîtier s'y loge bien)

$fn = 48;


// ---------- CALCULS (géométrie garantie cohérente) -----------

socle_l  = boitier_l + 2*gorge_marge;   // largeur totale
gorge_w  = boitier_ep + gorge_jeu;      // largeur de la fente
dx       = gorge_prof * tan(angle_vert);// décalage dû à l'inclinaison

// Bord AVANT de la gorge, mesuré à son point le plus avancé
// (c'est-à-dire au FOND de la gorge, car elle penche vers l'avant).
// On place ce point à ep_avant du bord avant du socle.
// => position X de l'axe de la gorge, au niveau du DESSUS :
gx       = ep_avant + gorge_w/2 + dx;

// Hauteur du sommet de la gorge (= hauteur utile du socle à cet endroit)
h_gorge  = ep_sous + gorge_prof;

// Bord ARRIÈRE de la gorge au sommet + matière derrière
prof_tot = gx + gorge_w/2 + ep_arriere;

// Hauteur de la lèvre avant (sommet)
h_levre  = ep_sous + gorge_h_av;


// ---------- CONSTRUCTION -------------------------------------

// Silhouette : cale (fin devant, épais derrière)
module silhouette_brute() {
    polygon([
        [0,                    0],           // avant-bas
        [prof_tot,             0],           // arrière-bas
        [prof_tot,             dossier_h],   // arrière-haut
        [gx + gorge_w/2,       h_gorge],     // sommet, derrière la gorge
        [gx - gorge_w/2 - dx,  h_levre],     // sommet de la lèvre avant
        [0,                    h_levre]      // avant-haut
    ]);
}

// Silhouette avec les angles ARRONDIS (offset -r puis +r)
module silhouette() {
    if (arrondi_r > 0)
        offset(r =  arrondi_r)
            offset(r = -arrondi_r)
                silhouette_brute();
    else
        silhouette_brute();
}

// Gorge : fente inclinée, borgne (s'arrête à ep_sous du fond)
module gorge() {
    translate([gx, h_gorge])
        rotate(-angle_vert)
            translate([-gorge_w/2, -gorge_prof])
                square([gorge_w, gorge_prof + 30]);
}

module profil_socle() {
    difference() {
        silhouette();
        gorge();
    }
}

// Le socle : bloc plein extrudé sur toute la largeur, dans lequel
// on creuse la gorge UNIQUEMENT sur la longueur du boîtier.
// Il reste donc une JOUE de `gorge_marge` de chaque côté, qui
// empêche le boîtier de coulisser latéralement.
module socle() {
    gorge_len = boitier_l + gorge_jeu_lat;
    gorge_y0  = (socle_l - gorge_len) / 2;

    difference() {
        // ---- BLOC PLEIN, arêtes extérieures arrondies ----
        // On arrondit les arêtes verticales (coins vus de dessus) en
        // rétrécissant puis re-gonflant le bloc avec minkowski d'un
        // cylindre : cela adoucit tous les bords verticaux.
        minkowski() {
            // bloc réduit du rayon d'arrondi dans TOUTES les directions.
            // IMPORTANT : on le décale de +arrondi_r en Y pour que le
            // regonflement par la sphère retombe SYMÉTRIQUEMENT
            // (sinon le socle se décale d'un côté).
            translate([0, arrondi_r, 0])
                rotate([90, 0, 0])
                    translate([0, 0, -(socle_l - 2*arrondi_r)])
                        linear_extrude(socle_l - 2*arrondi_r)
                            offset(delta = -arrondi_r) silhouette();
            // la "brosse" : une SPHÈRE -> arrondit TOUTES les arêtes,
            // y compris celles des faces latérales gauche et droite.
            sphere(r = arrondi_r, $fn = arrondi_fn);
        }

        // ---- LA GORGE : creusée APRÈS -> angles VIFS ----
        translate([0, gorge_y0, 0])
            rotate([90, 0, 0])
                translate([0, 0, -gorge_len])
                    linear_extrude(gorge_len)
                        gorge();
    }
}

socle();
