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
    char *entree, *sortie;
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;
    unsigned int runtime, deadline, period;
    int deadlineParamIndex = 0;
    char *splitString;
    while ((c = getopt(argc, argv, "s:d:")) != -1)
    {
        switch (c)
        {
        case 's':
            // On selectionne le mode d'ordonnancement
            if (strcmp(optarg, "NORT") == 0)
            {
                modeOrdonnanceur = ORDONNANCEMENT_NORT;
            }
            else if (strcmp(optarg, "RR") == 0)
            {
                modeOrdonnanceur = ORDONNANCEMENT_RR;
            }
            else if (strcmp(optarg, "FIFO") == 0)
            {
                modeOrdonnanceur = ORDONNANCEMENT_FIFO;
            }
            else if (strcmp(optarg, "DEADLINE") == 0)
            {
                modeOrdonnanceur = ORDONNANCEMENT_DEADLINE;
            }
            else
            {
                modeOrdonnanceur = ORDONNANCEMENT_NORT;
                printf("Mode d'ordonnancement %s non valide, defaut sur NORT\n", optarg);
            }
            break;
        case 'd':
            // Dans le cas DEADLINE, on peut recevoir des parametres
            // Si un autre mode d'ordonnacement est selectionne, ces
            // parametres peuvent simplement etre ignores
            splitString = strtok(optarg, ",");
            while (splitString != NULL)
            {
                if (deadlineParamIndex == 0)
                {
                    // Runtime
                    runtime = atoi(splitString);
                }
                else if (deadlineParamIndex == 1)
                {
                    deadline = atoi(splitString);
                }
                else
                {
                    period = atoi(splitString);
                    break;
                }
                deadlineParamIndex++;
                splitString = strtok(NULL, ",");
            }
            break;
        default:
            continue;
        }
    }

    if (argc - optind < 2)
    {
        printf("Arguments manquants (fichier_entree flux_sortie)\n");
        return -1;
    }

    entree = argv[optind];
    sortie = argv[optind + 1];
    printf("Initialisation decodeur, entree=%s, sortie=%s, mode d'ordonnancement=%i\n", entree, sortie, modeOrdonnanceur);

    int videoFileDescriptor = open(entree, O_RDONLY);
    struct stat videoFileStats;
    fstat(videoFileDescriptor, &videoFileStats);

    char *videoFilePointer = (char *)mmap(NULL, videoFileStats.st_size, PROT_READ, MAP_PRIVATE, videoFileDescriptor, 0);
    mlock(videoFilePointer, videoFileStats.st_size);

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
            // Fin des frames atteint
            break;
        }
        else
        {
            numberOfFramesInVideo++;
            numberOfBytesRead += numberOfBytesInCurrentFrame;
        }
    }
    // Cree l'array et recommence la boucle pour sauvegarder chaque pointer de frame dans l'array, on sauvegarde aussi la taille de chaque frame
    uintptr_t *framePointerArray = (uintptr_t *)malloc(sizeof(uintptr_t) * numberOfFramesInVideo);
    uint32_t *frameSizeArray = (uint32_t *)malloc(sizeof(unsigned int) * numberOfFramesInVideo);
    numberOfBytesRead = 0;
    numberOfFramesInVideo = 0;
    while (1)
    {
        uint32_t numberOfBytesInCurrentFrame;
        memcpy(&numberOfBytesInCurrentFrame, &videoFilePointer[numberOfBytesRead + (numberOfFramesInVideo * 4) + 20], sizeof(uint32_t));
        if (numberOfBytesInCurrentFrame == 0)
        {
            // Fin des frames atteint
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

    // Init espace memoire partage
    struct memPartage sharedMemoryZone;
    struct memPartageHeader sharedMemoryHeader;
    size_t sizeOfImages = videoInfo.canaux * videoInfo.largeur * videoInfo.hauteur;
    size_t sizeOfSharedMemory = sizeof(struct memPartageHeader) + sizeOfImages;
    sharedMemoryHeader.hauteur = videoInfo.hauteur;
    sharedMemoryHeader.largeur = videoInfo.largeur;
    sharedMemoryHeader.canaux = videoInfo.canaux;
    sharedMemoryHeader.fps = videoInfo.fps;

    //Init pool de memoire
    /*if (prepareMemoire(sizeOfImages, sizeOfImages) < 0)
    {
        printf("Echec preparation memoire par decodeur\n");
        return -1;
    }*/

    if (initMemoirePartageeEcrivain(sortie, &sharedMemoryZone, sizeOfSharedMemory, &sharedMemoryHeader) < 0)
    {
        return -1;
    }

    // decodage image et ecriture dans memoire partage
    while (1)
    {
        if (sharedMemoryHeader.frameWriter >= numberOfFramesInVideo - 1)
        {
            // Atteint la fin de la video, on recommence au premier frame
            sharedMemoryHeader.frameWriter = 1;
        }
        // Get the current frame pointer and size
        uintptr_t currentFramePointer = framePointerArray[sharedMemoryHeader.frameWriter - 1];
        uint32_t currentFrameSize = frameSizeArray[sharedMemoryHeader.frameWriter - 1];

        // Write frame data to shared memory zone
        int actualComp;
        int actualWidth;
        int actualHeight;

        unsigned char *dataToWrite = jpgd::decompress_jpeg_image_from_memory((const unsigned char *)currentFramePointer, currentFrameSize, &actualWidth, &actualHeight, &actualComp, 3, 0);

        memcpy(sharedMemoryZone.data, dataToWrite, sizeOfImages);
        tempsreel_free(dataToWrite);

        sharedMemoryZone.copieCompteur = sharedMemoryHeader.frameReader;
        pthread_mutex_unlock(&sharedMemoryZone.header->mutex);

        attenteEcrivain(&sharedMemoryZone);
        sharedMemoryHeader.frameWriter++;
    }
    return 0;
}
