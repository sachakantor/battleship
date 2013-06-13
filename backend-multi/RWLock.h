#ifndef RWLock_h
#define RWLock_h
#include <iostream>
#include <pthread.h>

class RWLock {
  public:
    RWLock();
    void rlock();
    void wlock();
    void runlock();
    void wunlock();

  private:

    //
    // El mecanismo que determina cuándo el recurso está disponible se
    // basa en una variable de condición. El mismo está compuesto por la
    // variable booleana que determina cuándo el recurso está disponible,
    // junto a la variable de condición propiamente dicha, y el mutex que
    // ésta lleva asociado.
    //
    bool            recurso_libre;
    pthread_cond_t  cond_recurso_libre;
    pthread_mutex_t mutex_recurso_libre;

    //
    // Controla el paso de escritores y lectores.
    pthread_mutex_t mutex_barrera;
    
    //
    // Controla el acceso a la variable 'contador_lectores'.
    pthread_mutex_t mutex_contador_lectores;
    
    //
    // Lleva la cuenta de la cantidad de lectores accediendo al recurso.
    int contador_lectores;
};

#endif
