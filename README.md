
Projet L3 Info - Codec d'images DIF


Binôme:
  - Oumaouche Taous
  - Zineddine Nour


Compilation


Pour compiler le projet:

    make clean
    make

Ça génère la bibliothèque libdif.so dans le dossier CoDec/ et l'exécutable 
"encodeur" à la racine.


Utilisation


Le programme prend en entrée soit une image (pour l'encoder), soit un fichier
.dif (pour le décoder)
Le programme détecte automatiquement ce qu'il doit faire en fonction de 
l'extension du fichier d'entrée (.dif = décodage, sinon = encodage).

Options disponibles:

    -h        Affiche l'aide
    -v        Mode verbeux (affiche plus d'infos pendant l'exécution)
    -t        Affiche le temps d'exécution
    -d        Force le mode décodage
    -e        Force le mode encodage
    -r        Génère aussi l'image différentielle (voir bonus ci-dessous)

Note: Pour les formats autres que PGM/PPM (comme JPEG, PNG, etc.), le 
programme utilise ImageMagick pour les convertir automatiquement. Il faut 
donc avoir ImageMagick installé sur le système.


Structure du projet

.
├── main.c             
├── Makefile           
├── README              
└── CoDec/              
    ├── Makefile       
    ├── include/
    │   └── codec.h     
    └── src/
        └── codec.c  


Fonctionnalités implémentées


✓ Encodage PNM (PGM et PPM) vers format DIF
✓ Décodage DIF vers PNM
✓ Support des images en niveaux de gris (PGM)
✓ Support des images couleur RGB (PPM)
✓ Transformation différentielle avec codage par repliement pair/impair
✓ Compression VLC avec quantificateur à 4 niveaux
✓ Gestion d'erreurs (fichiers manquants, formats invalides, etc.)
✓ Support des formats standards via ImageMagick (JPEG, PNG, GIF, etc.)
✓ Affichage des statistiques de compression
✓ Option -t -v -h
✓ Option -r pour générer l'image différentielle (visualisation des variations)
✓ Support de la conversion automatique des formats d'image


Détails techniques


Format DIF:
- Magic number: 0xD1FF (niveaux de gris) ou 0xD3FF (couleur)
- Header: largeur (2 octets) + hauteur (2 octets) + nb_niveaux (1 octet)
- Quantificateur: 4 niveaux avec intervalles [0,2[, [2,6[, [6,22[, [22,256[
- Bits par niveau: 1, 2, 4, 8
- Préfixes VLC: 0, 10, 110, 111

Pipeline d'encodage:
1. Lecture PNM
2. Réduction amplitude (division par 2 pour supprimer le bit de poids faible)
3. Calcul des différences entre pixels consécutifs
4. Repliement pair/impair (négatifs = impairs, positifs = pairs)
5. Compression VLC selon le quantificateur
6. Écriture du fichier DIF

Pipeline de décodage:
1. Lecture du fichier DIF (header + données compressées)
2. Décompression VLC
3. Dépliement pair/impair pour retrouver les deltas signés
4. Reconstruction des pixels par accumulation
5. Restauration amplitude (multiplication par 2)
6. Écriture PNM


Problèmes rencontrés et solutions


1. Gestion des commentaires dans les fichiers PNM
   Solution: Fonction dédiée qui saute les lignes commençant par '#'

2. Ordre des canaux RGB (plan par plan vs entrelacé)
   Solution: Décodage en plan par plan puis réinterleavage avant écriture

3. Reconstruction des pixels avec overflow
   Solution: Fonction de clamp pour limiter les valeurs entre 0 et 255

4. Compatibilité des fichiers .dif entre différents encodeurs
   Solution: Respect strict des spécifications du format (endianness, etc.)



Notes


Le code compile sans warnings avec -Wall et a été testé sur Ubuntu 24.04 et macOS
La bibliothèque est bien séparée de l'application principale, ce qui permet
de la réutiliser facilement dans d'autres projets.

Le mode différentiel (-r) est plutôt cool, ça permet de visualiser comment
le codec voit l'image (zones uniformes en blanc, contours en noir).
