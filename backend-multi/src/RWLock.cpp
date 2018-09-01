#include <stdio.h>
#include <RWLock.h>
#include <assert.h>


RWLock :: RWLock() {
    //Inicializo un mutexattr que usare luego para instanciar los mutex
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr,PTHREAD_MUTEX_ERRORCHECK);

    //Inicializo las variables
    pthread_cond_init(&this->c_no_readers,NULL);
    pthread_mutex_init(&this->m_readers_counter,&mutex_attr);
    pthread_mutex_init(&this->m_resource,&mutex_attr);
    this->readers_counter = 0;
}

void RWLock :: rlock() {
    /*Bloqueamos el recurso tanto para escritura como lectura*/
    pthread_mutex_lock(&this->m_resource);

    /*Si estamos acá, seguro que no hay una escritura andando*/
    pthread_mutex_lock(&this->m_readers_counter);
    this->readers_counter++;
    assert(this->readers_counter > 0);

    /*Liberamos el recurso para que sigan entrando lecturas o escrituras*/
    pthread_mutex_unlock(&this->m_readers_counter);
    pthread_mutex_unlock(&this->m_resource);
}

void RWLock :: wlock() {
    /*Bloqueamos el recurso para que no entren mas lecturas (ni escrituras)*/
    pthread_mutex_lock(&this->m_resource);

    /*Esperamos que terminen todas las lecturas actuales*/
    pthread_mutex_lock(&this->m_readers_counter);
    while(this->readers_counter > 0) pthread_cond_wait(&this->c_no_readers,&this->m_readers_counter);
    /*pthread_mutex_unlock(&this->m_readers_counter); *ningun lector nuevo llegara a pedir este mutex,
                                                     *pues aun tengo lockeado el m_resource, pero
                                                     *una lectura actual que termina debera tomarlo
                                                     *para decrementarlo.
                                                     */
    assert(this->readers_counter == 0);
}

void RWLock :: runlock() {
    /*Es un unlock de un lector, no debo pedir el recurso, pues
     * se asume que ya lo estabamos leyendo
     */
    pthread_mutex_lock(&this->m_readers_counter);
    assert(this->readers_counter > 0);
    this->readers_counter--;

    /*Si fui el ultimo lector, mando un "signal" al escritor.
     *Como no es acumulativo este "semaforo" de pthreads, no hay
     *problema si no hay ningun escritor esperando
     */
    if(this->readers_counter == 0) pthread_cond_signal(&this->c_no_readers);

    /*Libero el mutex*/
    pthread_mutex_unlock(&this->m_readers_counter);
}

void RWLock :: wunlock() {
    /*Asumimos que tenemos ya el mutex del recurso, que se hizo
     *en el wlock()
     */
    assert(this->readers_counter == 0);
    pthread_mutex_unlock(&this->m_readers_counter);
    pthread_mutex_unlock(&this->m_resource);
}

