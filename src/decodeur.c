/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 *
 * Fichier implémentant le programme de décodage des fichiers ULV
 ******************************************************************************/

// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

#include "jpgd.h"
#include "getopt.h"

#include <sys/mman.h>
#include <sys/types.h>

// Définition de diverses structures pouvant vous être utiles pour la lecture d'un fichier ULV
#define HEADER_SIZE 4
const char header[] = "SETR";

struct videoInfos
{
    uint32_t largeur;
    uint32_t hauteur;
    uint32_t canaux;
    uint32_t fps;
};

/******************************************************************************
 * FORMAT DU FICHIER VIDEO
 * Offset     Taille     Type      Description
 * 0          4          char      Header (toujours "SETR" en ASCII)
 * 4          4          uint32    Largeur des images du vidéo
 * 8          4          uint32    Hauteur des images du vidéo
 * 12         4          uint32    Nombre de canaux dans les images
 * 16         4          uint32    Nombre d'images par seconde (FPS)
 * 20         4          uint32    Taille (en octets) de la première image -> N
 * 24         N          char      Contenu de la première image (row-first)
 * 24+N       4          uint32    Taille (en octets) de la seconde image -> N2
 * 24+N+4     N2         char      Contenu de la seconde image
 * 24+N+N2    4          uint32    Taille (en octets) de la troisième image -> N2
 * ...                             Toutes les images composant la vidéo, à la suite
 *            4          uint32    0 (indique la fin du fichier)
 ******************************************************************************/

int main(int argc, char *argv[])
{
    // On desactive le buffering pour les printf(), pour qu'il soit possible de les voir depuis votre ordinateur
    setbuf(stdout, NULL);

    // Écrivez le code de décodage et d'envoi sur la zone mémoire partagée ici!
    // N'oubliez pas que vous pouvez utiliser jpgd::decompress_jpeg_image_from_memory()
    // pour décoder une image JPEG contenue dans un buffer!
    // N'oubliez pas également que ce décodeur doit lire les fichiers ULV EN BOUCLE

    // identify file to read and shared memory folder
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "")) != -1)
    {
        switch (c)
        {
        default:
            continue;
        }
    }

    if (argc - optind < 2)
    {
        printf("Arguments manquants (fichier_entree flux_sortie)\n");
        return -1;
    }

    int videoFileDescriptor = open(argv[optind], O_RDONLY);
    struct stat videoFileStats;
    fstat(videoFileDescriptor, &videoFileStats);

    char* videoFilePointer = (char*) mmap(NULL, videoFileStats.st_size, PROT_READ, MAP_PRIVATE, videoFileDescriptor, 0);//Crash avec MAP_POPULATE, mais fonctionne avec MAP_PRIVATE


    char fileHeader[4];
    memcpy(fileHeader, videoFilePointer, sizeof(char) * 4);
    if (strncmp(fileHeader, header, sizeof(char)*4) != 0)
    {
        // Invalid file input
        printf("En-tete du fichier d'entree invalide\n");
        printf("Header recu: ");
        printf("%s", fileHeader);
        printf("\n");
        return -1;
    }

    //Obtient hauteur, largeur, nb de canaux et images par seconde
    struct videoInfos videoInfo;
    memcpy(&videoInfo, &videoFilePointer[4], sizeof(struct videoInfos));

    //obtient taille des images
    uint32_t numberOfBytesInFrame;
    memcpy(&numberOfBytesInFrame, &videoFilePointer[20], sizeof(uint32_t));

    // essaie decodage image
    int actualComp;
    int actualWidth;
    int actualHeight;

    char* frame = (char *) malloc(numberOfBytesInFrame);
    memcpy(frame, &videoFilePointer[24], numberOfBytesInFrame);

    unsigned char *jpegImage = jpgd::decompress_jpeg_image_from_memory((const unsigned char *)frame, numberOfBytesInFrame, &actualWidth, &actualHeight, &actualComp, 3, 0);

    const char *nomImage = {"Test_Image.ppm"};
    enregistreImage((const unsigned char *)jpegImage, actualHeight, actualWidth, actualComp, nomImage);

    free(frame);

    /*struct stat st = {0};
    if(stat(argv[optind+1], &st) == -1)
    {
        mkdir(argv[optind+1], 0777);
    }*/

    return 0;
}

/******************************************************************************
 * FORMAT DU FICHIER VIDEO
 * Offset     Taille     Type      Description
 * 0          4          char      Header (toujours "SETR" en ASCII)
 * 4          4          uint32    Largeur des images du vidéo
 * 8          4          uint32    Hauteur des images du vidéo
 * 12         4          uint32    Nombre de canaux dans les images
 * 16         4          uint32    Nombre d'images par seconde (FPS)
 * 20         4          uint32    Taille (en octets) de la première image -> N
 * 24         N          char      Contenu de la première image (row-first)
 * 24+N       4          uint32    Taille (en octets) de la seconde image -> N2
 * 24+N+4     N2         char      Contenu de la seconde image
 * 24+N+N2    4          uint32    Taille (en octets) de la troisième image -> N2
 * ...                             Toutes les images composant la vidéo, à la suite
 *            4          uint32    0 (indique la fin du fichier)
 ******************************************************************************/