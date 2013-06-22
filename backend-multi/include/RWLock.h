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
    unsigned int readers_counter; /*Cuantos lectores quedan*/
    pthread_cond_t c_no_readers; /*Para esperar que termine la escritura*/
    pthread_mutex_t m_resource,m_readers_counter; /*mutexs*/
};

#endif
