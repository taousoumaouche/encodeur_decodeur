#include "../include/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* Structure pour la gestion des flux binaires */
typedef struct {
    unsigned char *buffer;
    size_t taille;
    size_t position;
    unsigned char accumulateur;
    int bits_accumules;
} FluxBits;

/* Fonction pour limiter une valeur entre 0 et 255 */
static unsigned char limiter_octet(int valeur){
    if (valeur < 0)   return 0;
    if (valeur > 255) return 255;
    return (unsigned char)valeur;
}

/* Lecture d'un bit depuis le flux */
static int lire_un_bit(FluxBits *flux, int *bit){
    if (flux->bits_accumules == 0) {
        if (flux->position >= flux->taille) return 0;
        flux->accumulateur = flux->buffer[flux->position++];
        flux->bits_accumules = 8;
    }
    *bit = (flux->accumulateur >> 7) & 1;
    flux->accumulateur <<= 1;
    flux->bits_accumules--;
    return 1;
}

/* Lecture de n bits depuis le flux */
static int lire_n_bits(FluxBits *flux, int nombre, unsigned int *valeur){
    *valeur = 0;
    for (int i = 0, bit; i < nombre; i++) {
        if (!lire_un_bit(flux, &bit)) return 0;
        *valeur = (*valeur << 1) | bit;
    }
    return 1;
}

/* Repliement pair/impair d'un delta */
unsigned char replier_delta(int delta) {
    return (delta < 0) ? (unsigned char)(-2 * delta - 1)
                       : (unsigned char)(2 * delta);
}

/* Dépliement pair/impair d'une valeur */
int deplier_delta(unsigned char y) {
    return (y & 1) ? -((int)y + 1) / 2 : (int)y / 2;
}

/* Ignorer les commentaires dans un fichier PNM */
static void ignorer_commentaires(FILE *fichier){
    int caractere;
    while ((caractere = fgetc(fichier)) != EOF) {
        if (caractere == '#')
            while ((caractere = fgetc(fichier)) != EOF && caractere != '\n');
        else if (!isspace(caractere)) {
            ungetc(caractere, fichier);
            return;
        }
    }
}

/* Lecture d'un fichier PNM */
int lire_pnm(const char *chemin, ImagePNM *image_sortie) {
    FILE *fichier = fopen(chemin, "rb");
    if (!fichier) 
        return DIF_ERR_IO;
    char nombre_magique[3];
    int largeur, hauteur, valeur_max;
    int nb_canaux;
    ignorer_commentaires(fichier);
    if (fscanf(fichier, "%2s", nombre_magique) != 1) {
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    if (strcmp(nombre_magique, "P5") == 0){
        nb_canaux = 1;
    }
    else if (strcmp(nombre_magique, "P6") == 0){
        nb_canaux = 3;
    }
    else {
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    ignorer_commentaires(fichier);
    if (fscanf(fichier, "%d", &largeur) != 1) {
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    ignorer_commentaires(fichier);
    if (fscanf(fichier, "%d", &hauteur) != 1) {
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    ignorer_commentaires(fichier);
    if (fscanf(fichier, "%d", &valeur_max) != 1) {
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    if (largeur <= 0 || hauteur <= 0 ||
        largeur > 65535 || hauteur > 65535 ||
        valeur_max != 255) {
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    fgetc(fichier);
    size_t taille_totale = (size_t)largeur * hauteur * nb_canaux;
    unsigned char *tampon = malloc(taille_totale);
    if (!tampon) {
        fclose(fichier);
        return DIF_ERR_ALLOC;
    }
    if (fread(tampon, 1, taille_totale, fichier) != taille_totale) {
        free(tampon);
        fclose(fichier);
        return DIF_ERR_FORMAT;
    }
    fclose(fichier);
    image_sortie->largeur = (uint16_t)largeur;
    image_sortie->hauteur = (uint16_t)hauteur;
    image_sortie->type = (uint8_t)nb_canaux;
    image_sortie->donnees = tampon;
    return DIF_OK;
}

/* Libération mémoire d'une image PNM */
void liberer_pnm(ImagePNM *image) {
    if (!image) return;
    free(image->donnees);
    image->donnees = NULL;
}

/* Réduction de l'amplitude (division par 2) */
static void diminuer_amplitude(ImagePNM *image) {
    size_t taille_totale = (size_t)image->largeur * image->hauteur * image->type;
    for (size_t i = 0; i < taille_totale; i++)
        image->donnees[i] >>= 1;
}

/* Calcul des différences entre pixels */
static int generer_differences(const ImagePNM *image,
                                unsigned char **valeurs_initiales,
                                int8_t **differences_sortie,
                                size_t *longueur_differences)
{
    int nb_pixels = image->largeur * image->hauteur;
    int nb_canaux = image->type;
    int taille_diff = (nb_pixels > 1) ? nb_canaux * (nb_pixels - 1) : 0;
    unsigned char *premiers_pixels = malloc(nb_canaux);
    int8_t *tableau_diffs = taille_diff ? malloc(taille_diff) : NULL;
    if (!premiers_pixels || (taille_diff && !tableau_diffs)) {
        free(premiers_pixels); 
        free(tableau_diffs);
        return DIF_ERR_ALLOC;
    }
    int index_diff = 0;
    if (nb_canaux == 1) {
        unsigned char pixel_precedent = image->donnees[0];
        premiers_pixels[0] = pixel_precedent;
        for (int i = 1; i < nb_pixels; i++) {
            unsigned char pixel_actuel = image->donnees[i];
            int difference = pixel_actuel - pixel_precedent;
            if (difference < -127 || difference > 127) {
                free(premiers_pixels); 
                free(tableau_diffs);
                return DIF_ERR_FORMAT;
            }
            tableau_diffs[index_diff++] = (int8_t)difference;
            pixel_precedent = pixel_actuel;
        }
    } else {
        int *valeurs_precedentes = malloc(sizeof(int) * nb_canaux);
        if (!valeurs_precedentes) { 
            free(premiers_pixels); 
            free(tableau_diffs); 
            return DIF_ERR_ALLOC; 
        }
        for (int canal = 0; canal < nb_canaux; canal++) {
            valeurs_precedentes[canal] = image->donnees[canal];
            premiers_pixels[canal] = (unsigned char)valeurs_precedentes[canal];
        }
        for (int i = 1; i < nb_pixels; i++) {
            for (int canal = 0; canal < nb_canaux; canal++) {
                unsigned char pixel_actuel = image->donnees[i * nb_canaux + canal];
                int difference = (int)pixel_actuel - valeurs_precedentes[canal];
                if (difference < -127 || difference > 127) {
                    free(valeurs_precedentes); 
                    free(premiers_pixels); 
                    free(tableau_diffs);
                    return DIF_ERR_FORMAT;
                }
                
                tableau_diffs[index_diff++] = (int8_t)difference;
                valeurs_precedentes[canal] = pixel_actuel;
            }
        }
        free(valeurs_precedentes);
    }

    *valeurs_initiales = premiers_pixels;
    *differences_sortie = tableau_diffs;
    *longueur_differences = taille_diff;
    return DIF_OK;
}

/* Repliement des deltas */
static int transformer_differences(const int8_t *diffs, size_t taille, unsigned char **sortie) {
    unsigned char *tampon = taille ? malloc(taille) : NULL;
    if (taille && !tampon) return DIF_ERR_ALLOC;
    for (size_t i = 0; i < taille; i++)
        tampon[i] = replier_delta(diffs[i]);
    *sortie = tampon;
    return DIF_OK;
}

/* Initialisation du flux d'écriture */
static int initialiser_flux_ecriture(FluxBits *flux, size_t taille) {
    flux->buffer = malloc(taille);
    if (!flux->buffer) return DIF_ERR_ALLOC;
    flux->taille = taille;
    flux->position = 0;
    flux->accumulateur = 0;
    flux->bits_accumules = 0;
    return DIF_OK;
}

/* Libération du flux d'écriture */
static void liberer_flux_ecriture(FluxBits *flux) {
    free(flux->buffer);
}

/* Écriture de bits dans le flux */
static void ecrire_bits(FluxBits *flux, unsigned int code, int nb_bits) {
    for (int bit = nb_bits - 1; bit >= 0; bit--) {
        flux->accumulateur = (flux->accumulateur << 1) | ((code >> bit) & 1);
        flux->bits_accumules++;
        if (flux->bits_accumules == 8) {
            flux->buffer[flux->position++] = flux->accumulateur;
            flux->accumulateur = 0;
            flux->bits_accumules = 0;
        }
    }
}

/* Finalisation du flux (écriture du dernier octet) */
static void finaliser_flux(FluxBits *flux) {
    if (flux->bits_accumules) {
        flux->accumulateur <<= (8 - flux->bits_accumules);
        flux->buffer[flux->position++] = flux->accumulateur;
    }
}

/* Encodage PNM vers DIF */
int pnmtodif(const char *chemin_pnm, const char *chemin_dif) {
    ImagePNM img;
    if (lire_pnm(chemin_pnm, &img) != DIF_OK) return DIF_ERR_IO;
    diminuer_amplitude(&img);
    unsigned char *premiers;
    int8_t *deltas;
    size_t longueur;
    generer_differences(&img, &premiers, &deltas, &longueur);
    unsigned char *valeurs_repliees;
    transformer_differences(deltas, longueur, &valeurs_repliees);
    FluxBits flux_sortie;
    initialiser_flux_ecriture(&flux_sortie, longueur * 2 + 16);
    for (size_t i = 0; i < longueur; i++) {
        unsigned int valeur = valeurs_repliees[i];
        if (valeur < 2) {
            ecrire_bits(&flux_sortie, 0b0, 1);
            ecrire_bits(&flux_sortie, valeur, 1);
        }
        else if (valeur < 6) {
            ecrire_bits(&flux_sortie, 0b10, 2);
            ecrire_bits(&flux_sortie, valeur - 2, 2);
        }
        else if (valeur < 22) {
            ecrire_bits(&flux_sortie, 0b110, 3);
            ecrire_bits(&flux_sortie, valeur - 6, 4);
        }
        else {
            ecrire_bits(&flux_sortie, 0b111, 3);
            ecrire_bits(&flux_sortie, valeur - 22, 8);
        }
    }
    
    finaliser_flux(&flux_sortie);
    FILE *fichier = fopen(chemin_dif, "wb");
    if (!fichier) return DIF_ERR_IO;
    uint16_t numero_magique = (img.type == 3) ? DIF_MAGIC_COLOR : DIF_MAGIC_GRAY;
    uint8_t nombre_niveaux = 4;
    uint8_t bits_par_niveau[4] = {1, 2, 4, 8};
    uint8_t entete[2 + 2 + 2 + 1 + 4];
    memcpy(entete + 0, &numero_magique, 2);
    memcpy(entete + 2, &img.largeur, 2);
    memcpy(entete + 4, &img.hauteur, 2);
    entete[6] = nombre_niveaux;
    memcpy(entete + 7, bits_par_niveau, nombre_niveaux);
    if (fwrite(entete, 1, 7 + nombre_niveaux, fichier) != (size_t)(7 + nombre_niveaux)) {
        fclose(fichier);
        return DIF_ERR_IO;
    }
    fwrite(premiers, 1, img.type, fichier);
    fwrite(flux_sortie.buffer, 1, flux_sortie.position, fichier);
    fclose(fichier);
    liberer_flux_ecriture(&flux_sortie);
    free(valeurs_repliees);
    free(deltas);
    free(premiers);
    liberer_pnm(&img);
    return DIF_OK;
}

/* Décodage DIF vers PNM */
int diftopnm(const char* fichier_dif, const char* fichier_pnm){
    FILE *f = fopen(fichier_dif, "rb");
    if (!f) return DIF_ERR_IO;
    struct stat info_fichier;
    if (stat(fichier_dif, &info_fichier) != 0) { 
        fclose(f); 
        return DIF_ERR_IO; 
    }
    long taille_fichier = (long)info_fichier.st_size;
    if (taille_fichier < 0) { 
        fclose(f); 
        return DIF_ERR_IO; 
    }
    uint16_t num_magique, larg, haut;
    uint8_t nb_niveaux;
    uint8_t entete7[7];
    if (fread(entete7, 1, 7, f) != 7) { 
        fclose(f); 
        return DIF_ERR_FORMAT; 
    }
    memcpy(&num_magique, entete7 + 0, 2);
    memcpy(&larg, entete7 + 2, 2);
    memcpy(&haut, entete7 + 4, 2);
    nb_niveaux = entete7[6];
    if (nb_niveaux != 4) { 
        fclose(f); 
        return DIF_ERR_FORMAT; 
    }
    uint8_t bits_niveaux[4];
    if (fread(bits_niveaux, 1, nb_niveaux, f) != (size_t)nb_niveaux) { 
        fclose(f); 
        return DIF_ERR_FORMAT; 
    }
    
    int nb_canaux;
    if (num_magique == DIF_MAGIC_GRAY) nb_canaux = 1;
    else if (num_magique == DIF_MAGIC_COLOR) nb_canaux = 3;
    else { 
        fclose(f); 
        return DIF_ERR_FORMAT; 
    }
    
    unsigned int decalages[4];
    decalages[0] = 0;
    for (int niveau = 1; niveau < 4; niveau++) 
        decalages[niveau] = decalages[niveau-1] + (1U << bits_niveaux[niveau-1]);
    unsigned char pixels_initiaux[3] = {0,0,0};
    if (fread(pixels_initiaux, 1, nb_canaux, f) != (size_t)nb_canaux) { 
        fclose(f); 
        return DIF_ERR_FORMAT; 
    }
    long taille_entete = 2 + 2 + 2 + 1 + nb_niveaux;
    long taille_premiers = nb_canaux;
    long taille_compresse = taille_fichier - (taille_entete + taille_premiers);
    if (taille_compresse < 0) { 
        fclose(f); 
        return DIF_ERR_FORMAT; 
    }
    
    unsigned char *donnees_compressees = NULL;
    size_t taille_comp = 0;
    if (taille_compresse > 0) {
        donnees_compressees = malloc((size_t)taille_compresse);
        if (!donnees_compressees) { 
            fclose(f); 
            return DIF_ERR_ALLOC; 
        }
        if (fread(donnees_compressees, 1, (size_t)taille_compresse, f) != (size_t)taille_compresse) {
            free(donnees_compressees); 
            fclose(f); 
            return DIF_ERR_FORMAT; 
        }
        
        taille_comp = (size_t)taille_compresse;
    }
    
    fclose(f);
    uint64_t nb_pixels_total = (uint64_t)larg * (uint64_t)haut;
    if (nb_pixels_total == 0 || nb_pixels_total > SIZE_MAX / (size_t)nb_canaux) {
        free(donnees_compressees);
        return DIF_ERR_FORMAT;
    }
    
    size_t total_pixels = (size_t)nb_pixels_total;
    unsigned char *plans = malloc(total_pixels * (size_t)nb_canaux);
    if (!plans) {
        free(donnees_compressees);
        return DIF_ERR_ALLOC;
    }
    
    FluxBits flux_lecture = { donnees_compressees, taille_comp, 0, 0, 0 };
    if (nb_canaux == 1) {
        int valeur_prec = pixels_initiaux[0];
        plans[0] = (unsigned char)valeur_prec;
        for (size_t idx = 1; idx < total_pixels; idx++) {
            int bit, niveau;
            unsigned int val = 0;
            if (!lire_un_bit(&flux_lecture, &bit)) {
                free(donnees_compressees);
                free(plans);
                return DIF_ERR_FORMAT;
            }
            
            if (!bit) niveau = 0;
            else {
                if (!lire_un_bit(&flux_lecture, &bit)) {
                    free(donnees_compressees);
                    free(plans);
                    return DIF_ERR_FORMAT;
                }
                
                if (!bit) niveau = 1;
                else {
                    if (!lire_un_bit(&flux_lecture, &bit)) {
                        free(donnees_compressees);
                        free(plans);
                        return DIF_ERR_FORMAT;
                    }
                    niveau = bit ? 3 : 2;
                }
            }
            
            if (bits_niveaux[niveau] > 0) {
                if (!lire_n_bits(&flux_lecture, bits_niveaux[niveau], &val)) {
                    free(donnees_compressees);
                    free(plans);
                    return DIF_ERR_FORMAT;
                }
            }
            
            unsigned int valeur_diff = decalages[niveau] + val;
            int difference = deplier_delta((unsigned char)valeur_diff);
            int valeur_courante = valeur_prec + difference;
            
            plans[idx] = limiter_octet(valeur_courante);
            valeur_prec = valeur_courante;
        }
    } else {
        int *valeurs_prec = malloc(sizeof(int) * nb_canaux);
        if (!valeurs_prec) {
            free(donnees_compressees);
            free(plans);
            return DIF_ERR_ALLOC;
        }
        
        for (int canal = 0; canal < nb_canaux; canal++) {
            valeurs_prec[canal] = pixels_initiaux[canal];
            plans[canal * total_pixels] = (unsigned char)valeurs_prec[canal];
        }
        
        for (size_t idx = 1; idx < total_pixels; idx++) {
            for (int canal = 0; canal < nb_canaux; canal++) {
                int bit, niveau;
                unsigned int val = 0;
                
                if (!lire_un_bit(&flux_lecture, &bit)) {
                    free(valeurs_prec);
                    free(donnees_compressees);
                    free(plans);
                    return DIF_ERR_FORMAT;
                }
                
                if (!bit) niveau = 0;
                else {
                    if (!lire_un_bit(&flux_lecture, &bit)) {
                        free(valeurs_prec);
                        free(donnees_compressees);
                        free(plans);
                        return DIF_ERR_FORMAT;
                    }
                    
                    if (!bit) niveau = 1;
                    else {
                        if (!lire_un_bit(&flux_lecture, &bit)) {
                            free(valeurs_prec);
                            free(donnees_compressees);
                            free(plans);
                            return DIF_ERR_FORMAT;
                        }
                        niveau = bit ? 3 : 2;
                    }
                }
                
                if (bits_niveaux[niveau] > 0) {
                    if (!lire_n_bits(&flux_lecture, bits_niveaux[niveau], &val)) {
                        free(valeurs_prec);
                        free(donnees_compressees);
                        free(plans);
                        return DIF_ERR_FORMAT;
                    }
                }
                
                unsigned int valeur_diff = decalages[niveau] + val;
                int difference = deplier_delta((unsigned char)valeur_diff);
                int valeur_courante = valeurs_prec[canal] + difference;
                
                plans[canal * total_pixels + idx] = limiter_octet(valeur_courante);
                valeurs_prec[canal] = valeur_courante;
            }
        }
        free(valeurs_prec);
    }

    free(donnees_compressees);

    /* Réinterleavage RGB */
    unsigned char *image_finale = malloc(total_pixels * (size_t)nb_canaux);
    if (!image_finale) {
        free(plans);
        return DIF_ERR_ALLOC;
    }

    for (size_t idx = 0; idx < total_pixels; idx++)
        for (int canal = 0; canal < nb_canaux; canal++)
            image_finale[idx * nb_canaux + canal] = plans[canal * total_pixels + idx];

    free(plans);

    /* Restauration amplitude : multiplier par 2 */
    size_t octets_totaux = total_pixels * (size_t)nb_canaux;
    for (size_t i = 0; i < octets_totaux; i++) {
        unsigned int valeur_amp = ((unsigned int)image_finale[i]) << 1;
        if (valeur_amp > 255u) valeur_amp = 255u;
        image_finale[i] = (unsigned char)valeur_amp;
    }

    /* Écriture PNM */
    FILE *sortie = fopen(fichier_pnm, "wb");
    if (!sortie) {
        free(image_finale);
        return DIF_ERR_IO;
    }

    fprintf(sortie, nb_canaux == 1 ? "P5\n" : "P6\n");
    fprintf(sortie, "%u %u\n255\n", larg, haut);
    
    if (fwrite(image_finale, 1, octets_totaux, sortie) != octets_totaux) {
        fclose(sortie);
        free(image_finale);
        return DIF_ERR_IO;
    }
    
    fclose(sortie);
    free(image_finale);
    
    return DIF_OK;
}

/* Décodage DIF raw (image différentielle) */
int diftopnm_raw(const char* fichier_dif, const char* fichier_pnm)
{
    FILE *f = fopen(fichier_dif, "rb");
    if (!f) return DIF_ERR_IO;

    struct stat info_fichier;
    if (stat(fichier_dif, &info_fichier) != 0) {
        fclose(f);
        return DIF_ERR_IO;
    }
    
    long taille_fichier = (long)info_fichier.st_size;
    if (taille_fichier < 0) {
        fclose(f);
        return DIF_ERR_IO;
    }

    uint16_t num_magique, larg, haut;
    uint8_t nb_niveaux;
    uint8_t entete7[7];
    
    if (fread(entete7, 1, 7, f) != 7) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }
    
    memcpy(&num_magique, entete7 + 0, 2);
    memcpy(&larg, entete7 + 2, 2);
    memcpy(&haut, entete7 + 4, 2);
    nb_niveaux = entete7[6];

    if (nb_niveaux != 4) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    uint8_t bits_niveaux[4];
    if (fread(bits_niveaux, 1, nb_niveaux, f) != (size_t)nb_niveaux) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    int nb_canaux;
    if (num_magique == DIF_MAGIC_GRAY) nb_canaux = 1;
    else if (num_magique == DIF_MAGIC_COLOR) nb_canaux = 3;
    else {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    unsigned int decalages[4];
    decalages[0] = 0;
    for (int niveau = 1; niveau < 4; niveau++) 
        decalages[niveau] = decalages[niveau-1] + (1U << bits_niveaux[niveau-1]);

    unsigned char pixels_initiaux[3] = {0,0,0};
    if (fread(pixels_initiaux, 1, nb_canaux, f) != (size_t)nb_canaux) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    long taille_entete = 2 + 2 + 2 + 1 + nb_niveaux;
    long taille_premiers = nb_canaux;
    long taille_compresse = taille_fichier - (taille_entete + taille_premiers);
    
    if (taille_compresse < 0) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    unsigned char *donnees_comp = NULL;
    size_t taille_comp = 0;
    
    if (taille_compresse > 0) {
        donnees_comp = malloc((size_t)taille_compresse);
        if (!donnees_comp) {
            fclose(f);
            return DIF_ERR_ALLOC;
        }
        
        if (fread(donnees_comp, 1, (size_t)taille_compresse, f) != (size_t)taille_compresse) { 
            free(donnees_comp); 
            fclose(f); 
            return DIF_ERR_FORMAT; 
        }
        
        taille_comp = (size_t)taille_compresse;
    }
    
    fclose(f);

    uint64_t nb_pixels_total = (uint64_t)larg * (uint64_t)haut;
    if (nb_pixels_total == 0 || nb_pixels_total > SIZE_MAX / (size_t)nb_canaux) {
        free(donnees_comp);
        return DIF_ERR_FORMAT;
    }
    
    size_t total_pixels = (size_t)nb_pixels_total;

    unsigned char *image_diff = malloc(total_pixels * (size_t)nb_canaux);
    if (!image_diff) {
        free(donnees_comp);
        return DIF_ERR_ALLOC;
    }

    FluxBits flux_lecture = { donnees_comp, taille_comp, 0, 0, 0 };

    if (nb_canaux == 1) {
        image_diff[0] = 255;
        
        for (size_t idx = 1; idx < total_pixels; idx++) {
            int bit, niveau;
            unsigned int val = 0;
            
            if (!lire_un_bit(&flux_lecture, &bit)) {
                free(donnees_comp);
                free(image_diff);
                return DIF_ERR_FORMAT;
            }
            
            if (!bit) niveau = 0;
            else {
                if (!lire_un_bit(&flux_lecture, &bit)) {
                    free(donnees_comp);
                    free(image_diff);
                    return DIF_ERR_FORMAT;
                }
                
                if (!bit) niveau = 1;
                else {
                    if (!lire_un_bit(&flux_lecture, &bit)) {
                        free(donnees_comp);
                        free(image_diff);
                        return DIF_ERR_FORMAT;
                    }
                    niveau = bit ? 3 : 2;
                }
            }
            
            if (bits_niveaux[niveau] > 0) {
                if (!lire_n_bits(&flux_lecture, bits_niveaux[niveau], &val)) { 
                    free(donnees_comp); 
                    free(image_diff); 
                    return DIF_ERR_FORMAT; 
                }
            }
            
            unsigned int valeur_repliee = decalages[niveau] + val;
            int difference = deplier_delta((unsigned char)valeur_repliee);
            
            /* Amplification du contraste */
            int diff_amplifiee = difference * 4;
            int visualisation = 255 - abs(diff_amplifiee);
            
            if (visualisation < 0) visualisation = 0;
            if (visualisation > 255) visualisation = 255;
            
            image_diff[idx] = (unsigned char)visualisation;
        }
    } else {
        image_diff[0] = 255;
        image_diff[1] = 255;
        image_diff[2] = 255;
        
        size_t position = 3;
        
        for (size_t idx = 1; idx < total_pixels; idx++) {
            for (int canal = 0; canal < nb_canaux; canal++) {
                int bit, niveau;
                unsigned int val = 0;
                
                if (!lire_un_bit(&flux_lecture, &bit)) {
                    free(donnees_comp);
                    free(image_diff);
                    return DIF_ERR_FORMAT;
                }
                
                if (!bit) niveau = 0;
                else {
                    if (!lire_un_bit(&flux_lecture, &bit)) {
                        free(donnees_comp);
                        free(image_diff);
                        return DIF_ERR_FORMAT;
                    }
                    
                    if (!bit) niveau = 1;
                    else {
                        if (!lire_un_bit(&flux_lecture, &bit)) {
                            free(donnees_comp);
                            free(image_diff);
                            return DIF_ERR_FORMAT;
                        }
                        niveau = bit ? 3 : 2;
                    }
                }
                
                if (bits_niveaux[niveau] > 0) {
                    if (!lire_n_bits(&flux_lecture, bits_niveaux[niveau], &val)) { 
                        free(donnees_comp); 
                        free(image_diff); 
                        return DIF_ERR_FORMAT; 
                    }
                }
                
                unsigned int valeur_repliee = decalages[niveau] + val;
                int difference = deplier_delta((unsigned char)valeur_repliee);
                
                /* Amplification du contraste */
                int diff_amplifiee = difference * 4;
                int visualisation = 255 - abs(diff_amplifiee);
                
                if (visualisation < 0) visualisation = 0;
                if (visualisation > 255) visualisation = 255;
                
                image_diff[position++] = (unsigned char)visualisation;
            }
        }
    }

    free(donnees_comp);

    /* Écriture PNM */
    FILE *sortie = fopen(fichier_pnm, "wb");
    if (!sortie) {
        free(image_diff);
        return DIF_ERR_IO;
    }

    fprintf(sortie, nb_canaux == 1 ? "P5\n" : "P6\n");
    fprintf(sortie, "%u %u\n255\n", larg, haut);
    
    size_t octets_totaux = total_pixels * (size_t)nb_canaux;
    if (fwrite(image_diff, 1, octets_totaux, sortie) != octets_totaux) { 
        fclose(sortie); 
        free(image_diff); 
        return DIF_ERR_IO; 
    }
    
    fclose(sortie);
    free(image_diff);
    
    return DIF_OK;
}