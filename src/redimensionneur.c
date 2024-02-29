/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2024
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de redimensionnement d'images
 ******************************************************************************/

// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"


int main(int argc, char* argv[]){
    // On desactive le buffering pour les printf(), pour qu'il soit possible de les voir depuis votre ordinateur
	setbuf(stdout, NULL);
    
    // Écrivez le code permettant de redimensionner une image (en utilisant les fonctions précodées
    // dans utils.c, celles commençant par "resize"). Votre code doit lire une image depuis une zone 
    // mémoire partagée et envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

    // Code lisant les options sur la ligne de commande
    char *entree, *sortie;                          // Zones memoires d'entree et de sortie
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;     // NORT est la valeur par defaut
    unsigned int runtime, deadline, period;         // Dans le cas de l'ordonnanceur DEADLINE
    int new_width, new_height, mode_redimension;

    if(argc < 2){
        printf("Nombre d'arguments insuffisant\n");
        return -1;
    }
    

    if(strcmp(argv[1], "--debug") == 0){
        // Mode debug, vous pouvez changer ces valeurs pour ce qui convient dans vos tests
        printf("Mode debug selectionne pour le convertisseur niveau de gris\n");
        entree = (char*)"/mem1";
        sortie = (char*)"/mem2";
        new_width = 200;
        new_height = 300;
        mode_redimension = 0;
    }

    else{
        int c;
        int deadlineParamIndex = 0;
        char* splitString;

        opterr = 0;

        while ((c = getopt (argc, argv, "s:d:w:h:r:")) != -1){
            switch (c)
                {
                case 's':
                    // On selectionne le mode d'ordonnancement
                    if(strcmp(optarg, "NORT") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                    }
                    else if(strcmp(optarg, "RR") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_RR;
                    }
                    else if(strcmp(optarg, "FIFO") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_FIFO;
                    }
                    else if(strcmp(optarg, "DEADLINE") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_DEADLINE;
                    }
                    else{
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
                        if(deadlineParamIndex == 0){
                            // Runtime
                            runtime = atoi(splitString);
                        }
                        else if(deadlineParamIndex == 1){
                            deadline = atoi(splitString);
                        }
                        else{
                            period = atoi(splitString);
                            break;
                        }
                        deadlineParamIndex++;
                        splitString = strtok(NULL, ",");
                    }
                    break;
                
                case 'w':
                    new_width = atoi(optarg);
                    break;
                
                case 'h':
                    new_height = atoi(optarg);
                    break;
                case 'r':
                    mode_redimension = atoi(optarg);
                    break;
                default:
                    continue;
                }
        }

        // Ce qui suit est la description des zones memoires d'entree et de sortie
        if(argc - optind < 2){
            printf("Arguments manquants (fichier_entree flux_sortie)\n");
            return -1;
        }
        entree = argv[optind];
        sortie = argv[optind+1];
    }

    printf("Initialisation convertisseur, entree=%s, sortie=%s, mode d'ordonnancement=%i\n", entree, sortie, modeOrdonnanceur);

    struct memPartage *mem_partage_in;
    struct memPartage *mem_partage_out;
    mem_partage_in = (struct memPartage*)malloc(sizeof(struct memPartage));
    mem_partage_out = (struct memPartage*)malloc(sizeof(struct memPartage));


    int init_mem_in = initMemoirePartageeLecteur((const char*)entree, mem_partage_in);
    if (init_mem_in < 0){
        printf("Erreur lors de l'initialisation de la memoire");
        return -1;
    }

    //pthread_mutex_unlock(&mem_partage_in->header->mutex);
    while (mem_partage_in->header->frameWriter == 0);

    uint32_t hauteur = mem_partage_in->header->hauteur;
    uint32_t largeur = mem_partage_in->header->largeur;
    uint32_t canaux = mem_partage_in->header->canaux;
    size_t tailleDonnees = hauteur*largeur*canaux;

    int init_mem_out = initMemoirePartageeEcrivain(sortie, mem_partage_out, tailleDonnees, mem_partage_in->header);
    if (init_mem_out < 0){
        printf("Erreur lors de l'initialisation de la memoire");
        return -1;
    }

    ResizeGrid grid;

    if (mode_redimension == 0)
    {
        grid = resizeNearestNeighborInit(new_height, new_width, hauteur, largeur);
    }
    else if(mode_redimension == 1) {
        grid = resizeBilinearInit(new_height, new_width, hauteur, largeur);
    }

    while (1)
    {
        mem_partage_in->header->frameReader++;
        unsigned char *current_data = mem_partage_in->data;
        mem_partage_in->copieCompteur = mem_partage_in->header->frameWriter;
        pthread_mutex_unlock(&mem_partage_in->header->mutex);
        attenteLecteur(mem_partage_in);

        if (mode_redimension == 0)
        {
            resizeNearestNeighbor(current_data, hauteur, largeur, mem_partage_out->data, new_height, new_width, grid, canaux);
        }

        else if(mode_redimension == 1)
        {
            resizeBilinear(current_data, hauteur, largeur, mem_partage_out->data, new_height, new_width, grid, canaux);
        }
        
        mem_partage_out->copieCompteur = mem_partage_out->header->frameReader;
        pthread_mutex_unlock(&mem_partage_out->header->mutex);
        
        attenteEcrivain(mem_partage_out);

        mem_partage_out->header->frameWriter++;
    }
    

    return 0;
}
