#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "Codec/include/codec.h"

/* ============================================================
 * Affiche l'aide
 * ============================================================ */
static void afficher_aide(const char *prog){
    printf("Usage: %s [options] entree sortie\n", prog);
    printf("Options:\n");
    printf("  -h   afficher cette aide\n");
    printf("  -v   mode verbeux\n");
    printf("  -t   afficher le temps d'execution\n");
    printf("  -d   forcer le decodage DIF -> PNM\n");
    printf("  -e   forcer l'encodage IMAGE -> DIF\n");
    printf("  -r   generer aussi l'image differentielle (raw)\n");
    printf("\n");
}

/* ============================================================
 * Retourne la taille d'un fichier
 * ============================================================ */
static long taille_fichier(const char *chemin){
    struct stat st;
    if (stat(chemin, &st) != 0)
        return -1;
    return st.st_size;
}

/* ============================================================
 * Teste une extension
 * ============================================================ */
static int a_extension(const char *nom, const char *ext){
    size_t ln = strlen(nom);
    size_t le = strlen(ext);
    if (ln < le)
        return 0;
    return strcmp(nom + ln - le, ext) == 0;
}

/* ============================================================
 * Teste si le fichier est un PNM
 * ============================================================ */
static int est_pnm(const char *nom){
    return a_extension(nom, ".pgm") ||
           a_extension(nom, ".ppm") ||
           a_extension(nom, ".pnm");
}

int main(int argc, char *argv[]){
    // options
    int opt_verbose = 0;
    int opt_temps = 0;
    int opt_raw = 0;
    int opt_force_decode = 0;
    int opt_force_encode = 0;
    // fichiers 
    const char *fichier_entree = NULL;
    const char *fichier_sortie = NULL;

    /* ========================================================
     * Lecture des arguments
     * ======================================================== */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            afficher_aide(argv[0]);
            return 0;
        }
        else if (!strcmp(argv[i], "-v")) {
            opt_verbose = 1;
        }
        else if (!strcmp(argv[i], "-t")) {
            opt_temps = 1;
        }
        else if (!strcmp(argv[i], "-d")) {
            opt_force_decode = 1;
        }
        else if (!strcmp(argv[i], "-e")) {
            opt_force_encode = 1;
        }
        else if (!strcmp(argv[i], "-r")) {
            opt_raw = 1;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Option inconnue : %s\n", argv[i]);
            afficher_aide(argv[0]);
            return 1;
        }
        else {
            if (!fichier_entree)
                fichier_entree = argv[i];
            else if (!fichier_sortie)
                fichier_sortie = argv[i];
            else {
                fprintf(stderr, "Trop d'arguments\n");
                return 1;
            }
        }
    }

    /* ========================================================
     * Verifications
     * ======================================================== */
    if (!fichier_entree || !fichier_sortie) {
        fprintf(stderr, "Fichier d'entree ou de sortie manquant\n");
        afficher_aide(argv[0]);
        return 1;
    }

    if (opt_force_decode && opt_force_encode) {
        fprintf(stderr, "Options -d et -e incompatibles\n");
        return 1;
    }

    /* ========================================================
     * MODE DECODAGE DIF -> PNM
     * ======================================================== */
    if (opt_force_decode || (!opt_force_encode && a_extension(fichier_entree, ".dif"))) {
        if (opt_verbose)
            printf("Decodage : %s -> %s\n", fichier_entree, fichier_sortie);
        clock_t debut = clock();
        int err = diftopnm(fichier_entree, fichier_sortie);
        clock_t fin = clock();
        if (err != DIF_OK) {
            fprintf(stderr, "Erreur lors du decodage DIF (%d)\n", err);
            return 1;
        }
        // image differentielle
        if (opt_raw) {
            char nom_raw[512];
            snprintf(nom_raw, sizeof nom_raw, "%s_raw.pnm", fichier_sortie);
            if (opt_verbose)
                printf("Image differentielle : %s\n", nom_raw);
            if (diftopnm_raw(fichier_entree, nom_raw) != DIF_OK) {
                fprintf(stderr, "Erreur generation image differentielle\n");
                return 1;
            }
        }
        if (opt_temps) {
            double t = (double)(fin - debut) / CLOCKS_PER_SEC;
            printf("Temps de decodage : %.3f s\n", t);
        }
        if (opt_verbose)
            printf("Decodage termine\n");
    }

    /* ========================================================
     * MODE ENCODAGE IMAGE -> DIF
     * ======================================================== */
    else {
        char fichier_pnm[256];
        const char *entree_pnm = fichier_entree;
        int pnm_temp = 0;
        // conversion si besoin 
        if (!est_pnm(fichier_entree)) {
            snprintf(fichier_pnm, sizeof fichier_pnm, "tmp_convert.pnm");
            char cmd[512];
            snprintf(cmd, sizeof cmd,
                     "convert \"%s\" \"%s\" 2>/dev/null",
                     fichier_entree, fichier_pnm);
            if (system(cmd) != 0) {
                fprintf(stderr, "Conversion impossible\n");
                return 1;
            }
            entree_pnm = fichier_pnm;
            pnm_temp = 1;
        }
        long taille_in = taille_fichier(entree_pnm);
        if (opt_verbose)
            printf("Encodage : %s -> %s\n", entree_pnm, fichier_sortie);
        clock_t debut = clock();
        int err = pnmtodif(entree_pnm, fichier_sortie);
        clock_t fin = clock();
        if (err != DIF_OK) {
            fprintf(stderr, "Erreur encodage PNM (%d)\n", err);
            if (pnm_temp) remove(entree_pnm);
            return 1;
        }
        long taille_out = taille_fichier(fichier_sortie);
        if (opt_temps) {
            double t = (double)(fin - debut) / CLOCKS_PER_SEC;
            printf("Temps d'encodage : %.3f s\n", t);
        }
        if (taille_in > 0 && taille_out > 0) {
            double ratio = 100.0 * taille_out / taille_in;
            printf("Taille brute : %ld octets\n", taille_in);
            printf("Taille DIF   : %ld octets\n", taille_out);
            printf("Compression  : %.2f %%\n", ratio);
        }
        if (pnm_temp)
            remove(entree_pnm);
    }
    return 0;
}
