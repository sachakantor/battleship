#include <stdio.h>
#include <pthread.h>
#include "../backend/RWLock.h"

#include <time.h>

//
// Creamos el lock de lectura/escritura.
//
RWLock rwlock;

//
//
//
long tiempo_inicio;

//
// Prototipos para las pruebas.
//
void test1();  // 5 lectores.
void test2();  // 5 escritores.
void test3();  // 3 lectores, 1 escritor, 2 lectores, 3 escritores, 1 lector.

//
// Punto de entrada.
//
int main() {

  // Guardamos el tiempo de inicio de ejecución.
  tiempo_inicio = time(NULL);

  printf("\n   ***  PRUEBA 1: 5 lectores  ***\n");
  test1();
  
  printf("\n   ***  PRUEBA 2: 5 escritores  ***\n");
  test2();
  
  printf("\n   ***  PRUEBA 3: 3 lectores, 1 escritor, 2 lectores, 3 escritores, 1 lector  ***\n");
  test3();

  return 0;
}

//
// Función de entrada hilo Lector.
//
void* reader(void* tid) {
  int t = 5, rc;
  int id = *(int*) tid;

  printf("(%03ld segs)  HILO %i: obteniendo lock de LECTURA...\n", (time(NULL) - tiempo_inicio), id);
  
  rwlock.rlock();
  
  printf("(%03ld segs)  HILO %i: lock de LECTURA obtenido! Voy a mantenerlo durante %i segundo(s).\n", (time(NULL) - tiempo_inicio), id, t);
  sleep(t);
  
  printf("(%03ld segs)  HILO %i: lock de LECTURA liberado!\n", (time(NULL) - tiempo_inicio), id);
  rwlock.runlock();

  return NULL;
}

//
// Función de entrada hilo Escritor.
//
void* writer(void* tid) {
  int t = 4, rc;
  int id = *(int*) tid;

  printf("(%03ld segs)  HILO %i: obteniendo lock de ESCRITURA...\n", (time(NULL) - tiempo_inicio), id);
  rwlock.wlock();

  printf("(%03ld segs)  HILO %i: lock de ESCRITURA obtenido! Voy a mantenerlo durante %i segundo(s).\n", (time(NULL) - tiempo_inicio), id, t);
  sleep(t);
  
  printf("(%03ld segs)  HILO %i: lock de ESCRITURA liberado!\n", (time(NULL) - tiempo_inicio), id);
  rwlock.wunlock();
  
  return NULL;
}


//
// Creamos 5 lectores.
//
void test1() {
  int tid = 0;
  pthread_t thread_readers[5];
  pthread_t tids_readers[5];

  // Creamos los hilos 'lectores'.
  for (tid = 0; tid < 5; ++tid) {
    tids_readers[tid] = tid;
    pthread_create(&thread_readers[tid], NULL, reader, &tids_readers[tid]);
  }

  for (tid = 0; tid < 5; ++tid)
    pthread_join(thread_readers[tid], NULL);
}


//
// Creamos 5 escritores.
//
void test2() {
  int tid = 0;
  pthread_t thread_writers[5];
  pthread_t tids_writers[5];

  // Creamos los hilos 'lectores'.
  for (tid = 0; tid < 5; ++tid) {
    tids_writers[tid] = tid;
    pthread_create(&thread_writers[tid], NULL, writer, &tids_writers[tid]);
  }

  for (tid = 0; tid < 5; ++tid)
    pthread_join(thread_writers[tid], NULL);  
}


//
// Creamos 3 lectores, 1 escritor, 2 lectores, 3 escritores, 1 lector.
//
void test3() {
  int tid = 0;

  //
  // Lectores.
  //
  pthread_t thread_readers[6];
  pthread_t tids_readers[6];
  int cr = 0;

  //
  // Escritores.
  //
  pthread_t thread_writers[4];  
  pthread_t tids_writers[4];
  int cw = 0;

  for (tid = 0; tid < 6; ++tid) {
    tids_readers[tid] = tid;
  }
  
  for (tid = 0; tid < 4; ++tid) {
    tids_writers[tid] = tid + 6;
  }
  
  //
  // 3 lectores.
  //
  pthread_create(&thread_readers[cr], NULL, reader, &tids_readers[cr]);
  cr++;
  pthread_create(&thread_readers[cr], NULL, reader, &tids_readers[cr]);
  cr++;
  pthread_create(&thread_readers[cr], NULL, reader, &tids_readers[cr]);  
  cr++;

  //
  // 1 escritor.
  //
  pthread_create(&thread_writers[cw], NULL, writer, &tids_writers[cw]);
  cw++;
  
  //
  // 2 lectores.
  //
  pthread_create(&thread_readers[cr], NULL, reader, &tids_readers[cr]);
  cr++;
  pthread_create(&thread_readers[cr], NULL, reader, &tids_readers[cr]);
  cr++;
  
  //
  // 3 escritores.
  //
  pthread_create(&thread_writers[cw], NULL, writer, &tids_writers[cw]);
  cw++;
  pthread_create(&thread_writers[cw], NULL, writer, &tids_writers[cw]);
  cw++;
  pthread_create(&thread_writers[cw], NULL, writer, &tids_writers[cw]);
  cw++;

  //
  // 1 lector.
  //
  pthread_create(&thread_readers[cr], NULL, reader, &tids_readers[cr]);
  cr++;
  
  for (tid = 0; tid < cr; ++tid) {
    pthread_join(thread_readers[tid], NULL);
  }

  for (tid = 0; tid < cw; ++tid) {
    pthread_join(thread_writers[tid], NULL);
  }
}
