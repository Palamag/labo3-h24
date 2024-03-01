/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 *
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"

int initMemoirePartageeLecteur(const char *identifiant, struct memPartage *zone)
{
    int shm = -1;

    while (shm < 0)
    {
        shm = shm_open(identifiant, O_RDWR, 0666);
    }

    struct stat *taille_Shm = (struct stat *)malloc(sizeof(struct stat));
    while (fstat(shm, taille_Shm) < 0)
        ;

    unsigned char *ptr = (unsigned char *)mmap(NULL, taille_Shm->st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    struct memPartageHeader *entete = (struct memPartageHeader *)ptr;

    unsigned char *data = ptr + sizeof(struct memPartageHeader);

    zone->fd = shm;
    zone->header = entete;
    zone->tailleDonnees = taille_Shm->st_size - sizeof(struct memPartageHeader);
    zone->data = data;
    zone->copieCompteur = 0;

    pthread_mutex_lock(&entete->mutex);

    return 0;
}

/**
 * il faut mettre canaux, fps, largeur et hauteur d'avance dans le headerInfos avant de le passer
 */
int initMemoirePartageeEcrivain(const char *identifiant, struct memPartage *zone, size_t taille, struct memPartageHeader *headerInfos)
{
    int shm = shm_open(identifiant, O_RDWR | O_CREAT, 0666);

    ftruncate(shm, taille + sizeof(struct memPartageHeader));

    unsigned char *ptr = (unsigned char *)mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    struct memPartageHeader *entete = (struct memPartageHeader *)ptr;

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);

    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&entete->mutex, &mutex_attr);

    entete->largeur = headerInfos->largeur;
    entete->hauteur = headerInfos->hauteur;
    entete->canaux = headerInfos->canaux;
    entete->fps = headerInfos->fps;

    zone->data = (((unsigned char *)ptr) + sizeof(struct memPartageHeader));
    zone->header = entete;
    zone->tailleDonnees = taille;
    zone->header->frameReader = 0;
    zone->header->frameWriter = 1;
    zone->copieCompteur = 0;

    pthread_mutex_lock(&entete->mutex);

    return 0;
}

int attenteLecteur(struct memPartage *zone)
{
    while (zone->copieCompteur == zone->header->frameWriter){usleep(1);}
    while(pthread_mutex_trylock(&zone->header->mutex) != 0);
    return 0;
}

int attenteLecteurAsync(struct memPartage *zone)
{
    if (zone->copieCompteur != zone->header->frameWriter)
    {
        return 1;
    }
    else
        return 0;
}

int attenteEcrivain(struct memPartage *zone)
{
    while (zone->copieCompteur == zone->header->frameReader){usleep(1);}
    while(pthread_mutex_trylock(&zone->header->mutex) != 0);
    return 0;
}