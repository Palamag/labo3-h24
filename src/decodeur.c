/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 *
 * Fichier implémentant le programme de décodage des fichiers ULV
 ******************************************************************************/

#undef __STRICT_ANSI__

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

    char *videoFilePointer = (char *)mmap(NULL, videoFileStats.st_size, PROT_READ, MAP_PRIVATE, videoFileDescriptor, 0);

    char fileHeader[4];
    memcpy(fileHeader, videoFilePointer, sizeof(char) * 4);
    if (strncmp(fileHeader, header, sizeof(char) * 4) != 0)
    {
        // Invalid file input
        printf("En-tete du fichier d'entree invalide\n");
        printf("Header recu: ");
        printf("%s", fileHeader);
        printf("\n");
        return -1;
    }

    // Obtient hauteur, largeur, nb de canaux et images par seconde
    struct videoInfos videoInfo;
    memcpy(&videoInfo, &videoFilePointer[4], sizeof(struct videoInfos));

    // Creation d'une array de pointer vers chaque frame, pour obtenir facilement chaque frame lors de l'ecriture
    // Commence par obtenir le nombre de frames
    uint32_t numberOfBytesRead = 0;
    uint32_t numberOfFramesInVideo = 0;
    while (1)
    {
        uint32_t numberOfBytesInCurrentFrame;
        memcpy(&numberOfBytesInCurrentFrame, &videoFilePointer[numberOfBytesRead + (numberOfFramesInVideo * 4) + 20], sizeof(uint32_t));
        if (numberOfBytesInCurrentFrame == 0)
        {
            //Fin des frames atteint
            break;
        }
        else
        {
            numberOfFramesInVideo++;
            numberOfBytesRead += numberOfBytesInCurrentFrame;
        }
    }
    // Cree l'array et recommence la boucle pour sauvegarder chaque pointer de frame dans l'array, on sauvegarde aussi la taille de chaque frame
    // On trouve aussi l'image la plus grosse
    uintptr_t *framePointerArray = (uintptr_t *)malloc(sizeof(uintptr_t) * numberOfFramesInVideo);
    uint32_t *frameSizeArray = (uint32_t *)malloc(sizeof(unsigned int) * numberOfFramesInVideo);
    uint32_t largestFrameSize = 0;
    numberOfBytesRead = 0;
    numberOfFramesInVideo = 0;
    while (1)
    {
        uint32_t numberOfBytesInCurrentFrame;
        memcpy(&numberOfBytesInCurrentFrame, &videoFilePointer[numberOfBytesRead + (numberOfFramesInVideo * 4) + 20], sizeof(uint32_t));
        if (largestFrameSize < numberOfBytesInCurrentFrame)
        {
            largestFrameSize = numberOfBytesInCurrentFrame;
        }
        if (numberOfBytesInCurrentFrame == 0)
        {
            //Fin des frames atteint
            break;
        }
        else
        {
            framePointerArray[numberOfFramesInVideo] = (uintptr_t)&videoFilePointer[numberOfBytesRead + (numberOfFramesInVideo * 4) + 24];
            frameSizeArray[numberOfFramesInVideo] = numberOfBytesInCurrentFrame;

            numberOfBytesRead += numberOfBytesInCurrentFrame;
            numberOfFramesInVideo++;
        }
    }

    if (prepareMemoire(largestFrameSize, largestFrameSize) < 0)
    {
        printf("Echec preparation memoire par decodeur\n");
        return -1;
    }

    // Init espace memoire partage
    struct memPartage sharedMemoryZone;
    struct memPartageHeader sharedMemoryHeader;
    size_t sizeOfSharedMemory = sizeof(sharedMemoryHeader) + largestFrameSize;

    if (initMemoirePartageeEcrivain(argv[optind + 1], &sharedMemoryZone, sizeOfSharedMemory, &sharedMemoryHeader) < 0)
    {
        return -1;
    }

    // decodage image et ecriture dans memoire partage
    printf("Decoder started\n");
    while (1)
    {
        // Get the current frame pointer and size
        uintptr_t currentFramePointer = framePointerArray[sharedMemoryHeader.frameWriter - 1];
        uint32_t currentFrameSize = frameSizeArray[sharedMemoryHeader.frameWriter - 1];

        // Write frame data to shared memory zone
        int actualComp;
        int actualWidth;
        int actualHeight;

        sharedMemoryZone.data = jpgd::decompress_jpeg_image_from_memory((const unsigned char *)currentFramePointer, currentFrameSize, &actualWidth, &actualHeight, &actualComp, 3, 0);
        sharedMemoryHeader.hauteur = actualHeight;
        sharedMemoryHeader.largeur = actualWidth;
        sharedMemoryHeader.canaux = actualComp;
        sharedMemoryHeader.fps = videoInfo.fps;
        sharedMemoryHeader.frameWriter = sharedMemoryHeader.frameReader;

        attenteEcrivain(&sharedMemoryZone);
        if(sharedMemoryHeader.frameWriter++ >= numberOfFramesInVideo - 1)
        {
            //Atteint la fin de la video, on recommence au premier frame
            sharedMemoryHeader.frameWriter = 1;
        }
    }
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
/*
struct memPartage{
    int fd;
    struct memPartageHeader *header;
    size_t tailleDonnees;
    unsigned char* data;
    uint32_t copieCompteur;             // Permet de se rappeler le compteur de l'autre processus
};
struct memPartageHeader{
    pthread_mutex_t mutex;
    uint32_t frameWriter;
    uint32_t frameReader;
    uint16_t hauteur;
    uint16_t largeur;
    uint16_t canaux;
    uint16_t fps;
};*/
