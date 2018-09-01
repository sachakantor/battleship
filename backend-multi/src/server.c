//
//  server.c
//  Batalla Naval
//
//  Created by Federico Raimondo on 4/24/13.
//  Copyright (c) 2013 ar.dc.uba.so. All rights reserved.
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <modelo.h>
#include <decodificador.h>
#include <globales.h>

#define MAX_MSG_LENGTH 4096
#define MAX_JUGADORES 100
#define MAX_CONTROLADORES 5

//Setea un socket como no bloqueante
int no_bloqueante(int fd) {
    int flags;
    //Toma los flags del fd y agrega O_NONBLOCK
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1 )
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


/* Variables globales del server */
struct sockaddr_in name_jugadores, name_controlador;// Direcciones
int sock_jugadores, sock_controlador;               // Socket donde se escuchan las conexiones entrantes
int s_jugadores[MAX_JUGADORES];                     // Sockets de los jugadores
int s_controladores[MAX_CONTROLADORES];             // Sockets de los controladores
int ids[MAX_JUGADORES];                             // Ids de los jugadores
int n,tamanio,tamanio_barcos,port;                  // Variables de configuracion del juego.
bool sale;                                          // Boleano para dejar de escuchar conexiones
Modelo * model = NULL;                              // Puntero al modelo del juego

//Resetea el juego
void reset() {
	if (model != NULL) delete model;
	model = new Modelo(n, tamanio, tamanio_barcos);
}

//Inicializamos los sockets de jugadores y controlador
void inicializar_socket(int &sock,struct sockaddr_in &name,int port,int max_conn){
    //Crear socket para los jugadores
    sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0) {
        perror("abriendo socket");
        exit(1);
    }
    //Crear nombre, usamos INADDR_ANY para indicar que cualquiera puede enviar aquí.
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(port);
    if (bind(sock,(const struct sockaddr*)&name,sizeof(name))) {
        perror("binding socket");
        exit(1);
    }

    //Escuchar en el socket y permitir n conexiones en espera.
    if (listen(sock,max_conn) == -1) {
        perror("escuchando socket");
        exit(1);
    }
}


//Para atender al i-esimo jugador
void* atender_jugador(void* sock_like_ptr) {
    //Casteamos el parametro a su valor correspondiente
    int i = (int)
        #ifdef __x86_64__
            //En caso de que estemos en una pc de 64 bits, para que no tire errores
            //de distintos tamaños entre int y void*
            (long int)
        #endif
        sock_like_ptr;
    int recibido;
    std::string resp;
    char buf[MAX_MSG_LENGTH]; // Buffer de recepción de mensajes
    Decodificador* decoder  = new Decodificador(model);

    //Comenzamos
    while(true && !sale){
        recibido = recv(s_jugadores[i], buf, MAX_MSG_LENGTH, 0);
        if (recibido < 0) {
            printf("Indice recibido: %d\n",i);
            exit(1);
            perror("Recibiendo ");

        } else if (recibido > 0) {
            buf[recibido]='\0';
            // Separo los mensajes por el caracter |
            char * pch = strtok(buf, "|");
            while (pch != NULL) {

                // No muestro por pantalla los NOP, son muchos
                if (strstr(pch, "Nop") == NULL) {
                    printf("Recibido: %s\n", pch);
                }

                //Decodifico el mensaje y obtengo una respuesta
                resp = decoder->decodificar(pch);

                // Si no se cual es el ID de este jugador, trato de obtenerlo de la respuesta
                if (ids[i] == -1) {
                    ids[i] = decoder->dameIdJugador(resp.c_str());
                }

                // Envio la respuesta
                send(s_jugadores[i],resp.c_str(), resp.length() +1, 0);

                // No muestro por pantalla los NOP, son muchos
                if (strstr(pch, "Nop") == NULL) {
                    printf("Resultado %s\n", resp.c_str());
                }

                // Si ya se cual es el jugador
                if (ids[i] != -1) {
                    // Busco si hay eventos para enviar y los mando
                    int eventos = model->hayEventos(ids[i]);
                    if (eventos != 0) {
                        printf("Agregando %d eventos\n", eventos);
                    }
                    for (int ev = 0; ev < eventos; ev++) {
                        resp = decoder->encodeEvent(ids[i]);
                        printf("Enviando evento %s", resp.c_str());
                        send(s_jugadores[i],resp.c_str(), resp.length() +1, 0);
                    }
                }
                pch = strtok(NULL, "|");
            }
        }
    }

    delete decoder;
    return NULL;
}

//Acepta todas las conexiones entrantes de los jugadores
void accept_jugadores() {
    //Variables locales
    int size_remote = sizeof(struct sockaddr_in);
    pthread_t threads_jugadores[MAX_JUGADORES];

    //Creamos el socket
    inicializar_socket(sock_jugadores,name_jugadores,port,n);

    //Comenzamos
	for(unsigned int cant_conex_jug=0;cant_conex_jug<(unsigned int)n && !sale;++cant_conex_jug){
        struct sockaddr_in name_jugador; //Se genera uno distinto por cada conexion
        s_jugadores[cant_conex_jug] = accept(sock_jugadores,(struct sockaddr*)&name_jugador,(socklen_t*)&size_remote);
        if (s_jugadores[cant_conex_jug] == -1){
            perror("aceptando la conexión entrante");
            exit(1);
        }

        //Configuramos el socket recien creado
        ids[cant_conex_jug] = -1;
        int flag = 1;
        setsockopt(s_jugadores[cant_conex_jug],        /* socket affected */
                IPPROTO_TCP,    /* set option at TCP level */
                TCP_NODELAY,    /* name of option */
                (char *) &flag, /* the cast is historical */
                sizeof(int));   /* length of option value */

        //Creamos el thread con el socket s
        //Notar que como el parametro de atender_jugador es (en verdad)
        //un int, podemos pasarlo como si fuera un puntero en lugar de
        //tener que pasarle un puntero a int (ya que sizeof(void*) == sizeof(int))
        pthread_create(
                &threads_jugadores[cant_conex_jug],
                NULL,
                atender_jugador,
                (void*)
                    #ifdef __x86_64__
                        //En caso de que estemos en una pc de 64 bits, para que no tire errores
                        //de distintos tamaños entre int y void*
                        (long)
                    #endif
                    cant_conex_jug //se lo paso por copia, ya que despues otro thread lo incrementa
        );
	}

    //Termino los jugadores y el socket de jugadores
    for(unsigned int i=0;i<(unsigned int)n;++i){
        pthread_join(threads_jugadores[i],NULL);
        close(s_jugadores[i]);
    }
    close(sock_jugadores);
}


//Para anteder a los requests del controlador
void* atender_controlador(void* sock_like_ptr) {
    //Variables locales
    int sock_i = (int)
        #ifdef __x86_64__
            //En caso de que estemos en una pc de 64 bits, para que no tire errores
            //de distintos tamaños entre int y void*
            (long int)
        #endif
        sock_like_ptr;
    char buf[MAX_MSG_LENGTH]; // Buffer de recepción de mensajes
    int recibido;
    std::string resp;
    Decodificador* decoder  = new Decodificador(model);

    //Comenzamos
    while(true && !sale){
        recibido = recv(s_controladores[sock_i],buf,MAX_MSG_LENGTH,0);
        if (recibido < 0) {
            perror("Recibiendo ");
        } else if (recibido > 0) {
            buf[recibido]='\0';
            char * pch = strtok(buf, "|");
            while (pch != NULL) {
                //Ejecutar y responder
                resp = decoder->decodificar(pch);
                send(s_controladores[sock_i],resp.c_str(), resp.length() +1, 0);
                pch = strtok(NULL, "|");
            }
        }
    }

    delete decoder;
    return NULL;
}

//Thread que atiende las conexiones entrantes del controlador
void* controller_manager(void*){
    //Variables locales
    int size_remote = sizeof(struct sockaddr_in);
    pthread_t threads_controladores[MAX_CONTROLADORES]; // Array de threads de controladores

    //Configuramos el socket
    inicializar_socket(sock_controlador,name_controlador,CONTROLLER_PORT,MAX_CONTROLADORES);

    //Comenzamos a aceptar conexiones
    for(unsigned int cant_conex_ctrls=0;cant_conex_ctrls<MAX_CONTROLADORES && !sale;++cant_conex_ctrls){
        //Por cada iteracion, genero una nueva direccion para la conexion
        struct sockaddr_in remote_controlador;

        //Por cada conexion recibida, configuramos un nuevo socket
        s_controladores[cant_conex_ctrls] = accept(sock_controlador,(struct sockaddr*)&remote_controlador,(socklen_t*)&size_remote);
        if (s_controladores[cant_conex_ctrls] == -1) {
            perror("aceptando la conexión entrante de controlador");
        } else {
            //Configuramos el socket
            int flag = 1;
            setsockopt(s_controladores[cant_conex_ctrls],    /* socket affected */
                IPPROTO_TCP,                    /* set option at TCP level */
                TCP_NODELAY,                    /* name of option */
                (char *) &flag,                 /* the cast is historical */
                sizeof(int));                   /* length of option value */

            //Creamos el thread con el socket
            //Notar que como el parametro de atender_jugador es (en verdad)
            //un int, podemos pasarlo como si fuera un puntero en lugar de
            //tener que pasarle un puntero a int (ya que sizeof(void*) == sizeof(int))
            pthread_create(&threads_controladores[cant_conex_ctrls],
                NULL,
                atender_controlador,
                (void*)
                    #ifdef __x86_64__
                        //En caso de que estemos en una pc de 64 bits, para que no tire errores
                        //de distintos tamaños entre int y void*
                        (long)
                    #endif
                    cant_conex_ctrls);
        }
    }

    //Termino los controladores y el socket de controladores
    for(unsigned int i=0;i<(unsigned int)n;++i){
        pthread_join(threads_controladores[i],NULL);
        close(s_controladores[i]);
    }
    close(sock_controlador);

    //Termino
    return NULL;
}


/**********************************************/
/******************* MAIN *********************/
/**********************************************/
/*
 * Recibe 4 parametros:
 * argv[1]: Puerto
 * argv[2]: Cantidad de jugadores (N)
 * argv[3]: Tamanio del tablero
 * argv[4]: Tamanio total de los barcos
 */
int main(int argc, char * argv[]) {
    //Variables locales
    int return_code = 0;
    sale=false;
    pthread_t thread_controlador;

    //Comenzamos
    if (argc < 5) {
        printf("Faltan parametros\n");
        printf("Se espera ./server puerto jugadores tamanio_tablero tamanio_barcos\n");
        exit(1);
    }
    port = atoi(argv[1]);
    n = atoi(argv[2]);
    tamanio = atoi(argv[3]);
    tamanio_barcos = atoi(argv[4]);

    inicializar();

    printf("Escuchando en el puerto %d - controlador en %d\n", port, CONTROLLER_PORT);
    printf("Jugadores %d - Tamanio %d - Tamanio Barcos %d\n", n, tamanio, tamanio_barcos);
    reset();

    printf("Corriendo...\n");

    //Creamos el thread que va a escuchar y crear un thread para cada comunicacion del controlador
    pthread_create(&thread_controlador,NULL,controller_manager,NULL);

    //Comenzamos a aceptar jugadores
    accept_jugadores();

    //Esperamos que termine el thread de los controladores
    pthread_join(thread_controlador,NULL);

    //Termino
	return return_code;
}
