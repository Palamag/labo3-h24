/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"

int initMemoirePartageeLecteur(const char* identifiant, struct memPartage *zone) 
{
    int shm = -1;

    while (shm < 0) 
    {
        shm = shm_open(identifiant, O_RDWR, 0666);
    }

    struct stat taille_Shm;
    while (fstat(shm, &taille_Shm) < 0);

    unsigned char *ptr = (unsigned char*)mmap(NULL, taille_Shm.st_size, PROT_READ, MAP_SHARED, shm, 0);

    struct memPartageHeader *entete = (struct memPartageHeader*) ptr;

    unsigned char *data = ptr + sizeof(&entete);

    zone->fd = shm;
    zone->header = entete;
    zone->tailleDonnees = taille_Shm.st_size - sizeof(struct memPartageHeader);
    zone->data = data;

    while(pthread_mutex_trylock(&zone->header->mutex) < 0) sched_yield();

    return 0;
}


/**
 * il faut mettre canaux, fps, largeur et hauteur d'avance dans le headerInfos avant de le passer
*/
int initMemoirePartageeEcrivain(const char* identifiant, struct memPartage *zone, size_t taille, struct memPartageHeader* headerInfos)
{
    int shm = shm_open(identifiant, O_RDWR | O_CREAT, 0666);

    ftruncate(shm, taille);

    unsigned char *ptr = (unsigned char*)mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&mut, NULL);

    headerInfos->mutex = mut;
    zone->fd = shm;
    zone->tailleDonnees = taille - sizeof(struct memPartageHeader);
    zone->header = headerInfos;
    zone->data = ptr + sizeof(struct memPartageHeader);

    pthread_mutex_lock(&mut);
    zone->header->frameReader = 0;
    zone->header->frameWriter = 1;

    return 0;
}

int attenteLecteur(struct memPartage *zone)
{
    while (zone->header->frameWriter == zone->copieCompteur) sched_yield();
    while(pthread_mutex_trylock(&zone->header->mutex) < 0) sched_yield();
    return 0;
}

int attenteLecteurAsync(struct memPartage *zone)
{
    return 0;
}

int attenteEcrivain(struct memPartage *zone)
{
    while (zone->header->frameReader == zone->copieCompteur) sched_yield();
    while(pthread_mutex_trylock(&zone->header->mutex) < 0) sched_yield();
    return 0;
}