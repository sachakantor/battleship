#include <stdio.h>
#include "RWLock.h"


RWLock :: RWLock() {
  //
  // Configuramos el tipo de atributo para todos los mutexes utilizados.
  // En este caso, el tipo de atributo será 'PTHREAD_MUTEX_ERRORCHECK'
  // para realizar las pruebas de errores de forma automática.
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);

  pthread_mutex_init(&mutex_contador_lectores, &mutex_attr);
  pthread_mutex_init(&mutex_barrera, &mutex_attr);
  pthread_mutex_init(&mutex_recurso_libre, &mutex_attr);
  
  //
  // Inicializamos la variable de condición.
  pthread_cond_init(&cond_recurso_libre, NULL);
  
  contador_lectores = 0;
  recurso_libre = true;
}

void RWLock :: rlock() {
  // Si ya existe algún escritor usando el recurso, entonces seguro tomó
  // la barrera. Debemos esperar hasta que éste la libere.
  pthread_mutex_lock(&mutex_barrera);

  // Liberamos la barrera de inmediato para que otros lectores y
  // escritores puedan intentar el acceso al recurso.
  pthread_mutex_unlock(&mutex_barrera);
  
  //
  // Comprobamos si somos el primer lector en acceder al recurso.
  //
  pthread_mutex_lock(&mutex_contador_lectores);
  
  if (contador_lectores == 0) {
    // Somos el primer lector en acceder al recurso. Lo tomamos. Este
    // recurso deberá ser liberado por el último lector que acceda a él.

    pthread_mutex_lock(&mutex_recurso_libre);
    
    while(!recurso_libre) {
	 // Esperamos por la condición (el recurso está libre).
	 pthread_cond_wait(&cond_recurso_libre, &mutex_recurso_libre);
    }
    
    // Tomamos el recurso.
    recurso_libre = false;
    
    // Liberamos el mutex asociado a la variable de condición.
    pthread_mutex_unlock(&mutex_recurso_libre);
  }

  contador_lectores++;  
  pthread_mutex_unlock(&mutex_contador_lectores);  
}

void RWLock :: wlock() {
  // Tomamos el mutex de la barrera para impedir el intento de acceso al
  // recurso por parte de nuevos lectores o escritores.
  pthread_mutex_lock(&mutex_barrera);

  pthread_mutex_lock(&mutex_recurso_libre);
    
  while(!recurso_libre) {
    // Esperamos por la condición (el recurso está libre).
    pthread_cond_wait(&cond_recurso_libre, &mutex_recurso_libre);
  }
    
  // Tomamos el recurso.
  recurso_libre = false;

  // Liberamos el mutex asociado a la variable de condición.
  pthread_mutex_unlock(&mutex_recurso_libre);
}

void RWLock :: runlock() {
  pthread_mutex_lock(&mutex_contador_lectores);
  contador_lectores--;
  
  // Si somos el último lector en acceder al recurso, lo liberamos.
  if (contador_lectores == 0) {
    pthread_mutex_lock(&mutex_recurso_libre);

    recurso_libre = true;

    // Liberamos el recurso.
    pthread_cond_signal(&cond_recurso_libre);

    pthread_mutex_unlock(&mutex_recurso_libre);
  }

  pthread_mutex_unlock(&mutex_contador_lectores);  
}

void RWLock :: wunlock() {
  pthread_mutex_lock(&mutex_recurso_libre);

  recurso_libre = true;

  // Liberamos el recurso.
  pthread_cond_signal(&cond_recurso_libre);

  pthread_mutex_unlock(&mutex_recurso_libre);

  // Permitimos el ingreso de nuevos lectores o escritores.
  pthread_mutex_unlock(&mutex_barrera);
}

