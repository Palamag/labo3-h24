/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"

int initMemoirePartageeLecteur(const char* identifiant,
                                struct memPartage *zone) 
{
    int shm = -1;
    int trunc = -1;

    while (shm < 0 && trunc < 0) 
    {
        shm = shm_open(identifiant, O_RDWR | O_CREAT, 0666);
        trunc = ftruncate(shm, zone->tailleDonnees);
    }

    zone->data = (unsigned char*)mmap(NULL, zone->tailleDonnees, PROT_READ, MAP_SHARED, shm, 0);

    if (zone->data == MAP_FAILED) 
    {
        perror("erreur in mmap");
        return -1;
    }

    while (zone->header->frameWriter == 0) sched_yield();
    while(pthread_mutex_trylock(&zone->header->mutex) < 0) sched_yield();

    return 0;
}

int initMemoirePartageeEcrivain(const char* identifiant,
                                struct memPartage *zone,
                                size_t taille,
                                struct memPartageHeader* headerInfos)
{
    int shm = shm_open(identifiant, O_RDWR | O_CREAT, 0666);

    ftruncate(shm, taille);

    zone->data = (unsigned char*)mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    if (zone->data == MAP_FAILED) 
    {
        printf("erreur in mmap");
        return -1;
    }

    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&mut, NULL);

    headerInfos->mutex = mut;
    zone->tailleDonnees = taille;
    zone->header = headerInfos;

    pthread_mutex_lock(&mut);
    zone->header->frameWriter++;

    return 0;
}

int attenteLecteur(struct memPartage *zone)
{
    while (zone->header->frameWriter == zone->header->frameReader) sched_yield();
    while(pthread_mutex_trylock(&zone->header->mutex) < 0) sched_yield();
    return 0;
}

int attenteLecteurAsync(struct memPartage *zone)
{
    return 0;
}

int attenteEcrivain(struct memPartage *zone)
{
    while (zone->header->frameWriter == zone->header->frameReader) sched_yield();
    while(pthread_mutex_trylock(&zone->header->mutex) < 0) sched_yield();
    return 0;
}