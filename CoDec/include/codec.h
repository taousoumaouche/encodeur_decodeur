#ifndef CODEC_H
#define CODEC_H
#include <stddef.h>
#include <stdint.h>

#define DIF_MAGIC_GRAY  0xD1FFu
#define DIF_MAGIC_COLOR 0xD3FFu
#define DIF_OK               0
#define DIF_ERR_IO            1
#define DIF_ERR_FORMAT        2
#define DIF_ERR_ALLOC         3
#define DIF_ERR_UNIMPLEMENTED 10

int pnmtodif(const char *chemin_image_pnm, const char *chemin_dif);
int diftopnm(const char *chemin_dif, const char *chemin_image_pnm);
int diftopnm_raw(const char *chemin_dif, const char *chemin_image_pnm);

typedef struct {
    uint16_t largeur;
    uint16_t hauteur;
    uint8_t type; 
    unsigned char *donnees; 
} ImagePNM;
unsigned char replier_delta(int delta);
int deplier_delta(unsigned char y);
int lire_pnm(const char *chemin, ImagePNM *out);
void liberer_pnm(ImagePNM *img);                                
#endif