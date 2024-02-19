/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 *
 * Fichier implémentant les fonctions de l'allocateur mémoire temps réel
 ******************************************************************************/

#include "allocateurMemoire.h"
#include <cstring>
#include <cstdint>
#include <stdlib.h>
#include <sys/mman.h>

typedef struct
{
    size_t inputImageSize;
    size_t outputImageSize;
    unsigned char inputBlockUsageArray[5];
    unsigned char outputBlockUsageArray[5];
    uint32_t inputMemBlockPointers[5];
    uint32_t outputMemBlockPointers[5];
} memPoolStruct;

memPoolStruct memPool;

int prepareMemoire(size_t tailleImageEntree, size_t tailleImageSortie)
{
    size_t totalInputSize = tailleImageEntree * 5;
    size_t totalOutputSize = tailleImageSortie * 5;
    size_t tailleTotale = totalInputSize + totalOutputSize;
    uint32_t firstBlock = (uint32_t)malloc(tailleTotale);
    if (errno == ENOMEM)
    {
        // memory allocation failed or too much memory requested
        return -1;
    }

    mlock(&firstBlock, tailleTotale); // lock memory buffer

    memset(&memPool, 0, sizeof(memPoolStruct)); // Initialize pool at all 0;
    memPool.inputImageSize = tailleImageEntree;
    memPool.outputImageSize = tailleImageSortie;
    for (int i = 0; i < 4; i++)
    {
        memPool.inputMemBlockPointers[i] = firstBlock + (i * tailleImageEntree);
        memPool.outputMemBlockPointers[i] = firstBlock + (tailleImageEntree * 5) + (i * tailleImageSortie);
    }

    return 0;
}

void *tempsreel_malloc(size_t taille)
{
    uint32_t memPointer = 0;
    if (taille == memPool.inputImageSize)
    {
        for (int i = 0; i < 5; i++)
        {
            if (memPool.inputBlockUsageArray[i] == 0)
            {
                memPool.inputBlockUsageArray[i] = 1;
                memPointer = memPool.inputMemBlockPointers[i];
                break;
            }
        }
    }
    else if (taille == memPool.outputImageSize)
    {
        for (int i = 0; i < 5; i++)
        {
            if (memPool.outputBlockUsageArray[i] == 0)
            {
                memPool.outputBlockUsageArray[i] = 1;
                memPointer = memPool.outputMemBlockPointers[i];
                break;
            }
        }
    }
    else
    {
        // Requested size does not match available memory buffers
        errno = EINVAL;
        return NULL;
    }
    if (memPointer == 0)
    {
        // All blocks are currently in use
        errno = ENOMEM;
        return NULL;
    }
    return (void *)memPointer;
}

void tempsreel_free(void *ptr)
{
    for (int i = 0; i < 5; i++)
    {
        if (ptr == (void *)memPool.inputMemBlockPointers[i])
        {
            memPool.inputBlockUsageArray[i] = 0;
            break;
        }
        if (ptr == (void *)memPool.outputMemBlockPointers[i])
        {
            memPool.outputBlockUsageArray[i] = 0;
            break;
        }
    }
}