#include <modelo.h>
#ifdef DEBUG
#include <cstdio>
#endif
#include <constantes.h>
#include <globales.h>
#include <stdlib.h>
#include <pthread.h>

Modelo::locks_t::locks(){
    this->rwl_jugadores = new RWLock();
    this->rwl_eventos = new RWLock();
    this->rwl_tiros = new RWLock();
}

Modelo::locks_t::~locks(){
    delete this->rwl_jugadores;
    delete this->rwl_eventos;
    delete this->rwl_tiros;
}

Modelo::Modelo(const int njugadores, const int tamtablero, const int tamtotalbarcos){
	max_jugadores = njugadores;
	tamanio_tablero = tamtablero;
	tamanio_total_barcos = tamtotalbarcos;

	this->jugadores = new Jugador*[max_jugadores];
	this->eventos = new std::queue<evento_t *>[max_jugadores];
	this->tiros = new tiro_t*[max_jugadores];
    this->locks = new locks_t*[max_jugadores];
	for (int i = 0; i < max_jugadores; i++) {
		this->jugadores[i] = NULL;
		this->tiros[i] = NULL;
		this->locks[i] = new Modelo::locks_t();
	}
	this->cantidad_jugadores = 0;
	this->jugando = false;

    //Inicializo los RWLocks de los atributos locales
    this->rwl_jugadores = new RWLock();
    this->rwl_tiros = new RWLock();
    this->rwl_locks = new RWLock();
    this->rwl_jugando = new RWLock();
    this->rwl_cantidad_jugadores = new RWLock();

}

Modelo::~Modelo() {
	for (int i = 0; i < max_jugadores; i++) {
		if (this->jugadores[i] != NULL) {
			delete this->jugadores[i];
			delete this->tiros[i];
            delete this->locks[i];
		}
	}
	delete[] this->jugadores;
	delete[] this->tiros;
    delete[] this->locks;
    delete[] this->eventos;

    delete this->rwl_jugadores;
    delete this->rwl_tiros;
    delete this->rwl_locks;
    delete this->rwl_jugando;
    delete this->rwl_cantidad_jugadores;
}

int Modelo::agregarJugador(std::string nombre) {
    //Variables locales
    int return_code = -ERROR_JUEGO_EN_PROGRESO;

    //Pido los locks de lectura
    this->rwl_jugando->rlock(); //Evito que se anule el juego mientras registro al nuevo jugador

    //Comenzamos
	if(!this->jugando){

        //Pido los locks de escritura
        this->rwl_jugadores->wlock();

        int nuevoid = 0;
        for (nuevoid = 0; nuevoid < max_jugadores && this->jugadores[nuevoid] != NULL; nuevoid++);

        if(nuevoid != max_jugadores){ //si el nuevoid es un indice valido
            //Pido locks de escritura
            this->rwl_cantidad_jugadores->wlock();
            this->rwl_tiros->wlock();
            this->rwl_locks->wlock();
            this->locks[nuevoid]->rwl_tiros->wlock();

            //Registro al jugador
            this->tiros[nuevoid] = new tiro_t();
            this->tiros[nuevoid]->t_id = JUGADOR_SINDEFINIR;
            this->tiros[nuevoid]->stamp.tv_sec = 0;
            this->tiros[nuevoid]->stamp.tv_usec = 0;
            this->tiros[nuevoid]->eta = 0;
            this->tiros[nuevoid]->estado = TIRO_LIBRE;

            this->jugadores[nuevoid] = new Jugador(nombre);

            this->cantidad_jugadores++;

            return_code = nuevoid;

            //Suelto los locks de esta instancia
            this->locks[nuevoid]->rwl_tiros->wunlock();
            this->rwl_locks->wunlock();
            this->rwl_tiros->wunlock();
            this->rwl_cantidad_jugadores->wunlock();
        }
        //Suelto los locks de escritura
        this->rwl_jugadores->wunlock();
    }

    //Suelto los locks de lectura
    this->rwl_jugando->runlock();

    return return_code;
}


error Modelo::ubicar(int t_id, int * xs, int *  ys, int tamanio) {
    //Variables locales
    error return_error;

    //Pido los locks de lectura
    this->rwl_jugando->rlock();

    //Comenzamos
	if(this->jugando)
        return_error = -ERROR_JUEGO_EN_PROGRESO;

    else {
        //Pido lock de jugadores para que no se indefina
        //una vez que se que el juego no empezo
        this->rwl_jugadores->rlock();

        if(this->jugadores[t_id] == NULL)
            return_error = -ERROR_JUGADOR_INEXISTENTE;
        else {
            //Pido los locks de escritura del jugador
            this->rwl_locks->rlock();   //para evitar que alguien borre
                                        //o modifique el RWLock del jugador
            this->locks[t_id]->rwl_jugadores->wlock();

            return_error = this->jugadores[t_id]->ubicar(xs, ys, tamanio);

            //Suelto los locks de escrituras del jugador
            this->locks[t_id]->rwl_jugadores->wunlock();
            this->rwl_locks->runlock();
        }
        //Suelto el lock de jugadores
        this->rwl_jugadores->runlock();
    }

    //Suelto los locks de lectura
    this->rwl_jugando->runlock();

    return return_error;
}

error Modelo::borrar_barcos(int t_id) {
    //Variables locales
    error return_error;

    //Pido los locks de lectura
    this->rwl_jugando->rlock();

    //Comenzamos
	if(this->jugando)
        return_error = -ERROR_JUEGO_EN_PROGRESO;

    else {
        //Pido lock de jugadores para que no se indefina
        //una vez que se que el juego no empezo
        this->rwl_jugadores->rlock();

        if(this->jugadores[t_id] == NULL)
            return_error = -ERROR_JUGADOR_INEXISTENTE;
        else{
            //Pido los locks de escritura
            this->rwl_locks->rlock();
            this->locks[t_id]->rwl_jugadores->wlock();

            return_error = this->jugadores[t_id]->quitar_barcos();

            //Suelto los locks de escritura
            this->locks[t_id]->rwl_jugadores->wunlock();
            this->rwl_locks->runlock();
        }

        //Suelto el lock de jugadores
        this->rwl_jugadores->runlock();
    }

    //Suelto los locks de lectura
    this->rwl_jugando->runlock();

    return return_error;
}

error Modelo::empezar() {
    //Variables locales
    error return_error = ERROR_NO_ERROR;
    bool jugadores_listos = true;
    int ultimoJugador;

    //Pido los locks
    this->rwl_jugando->wlock();

    //Comenzamos
	if(this->jugando)
        return_error = -ERROR_JUEGO_EN_PROGRESO;

    else {
        //Pido locks una vez que el juego no esta en progreso
        this->rwl_jugadores->rlock();
        this->rwl_cantidad_jugadores->rlock();

        //Verifico si ya tengo inscriptos a todos los jugadores
        if(this->cantidad_jugadores != max_jugadores)
            return_error = -ERROR_JUGADOR_NO_LISTO;

        else{
            //Pido locks sabiendo que hay ya se inscribieron los jugadores
            this->rwl_locks->rlock();

            //Verifico si todos los jugadores estan listos
            for(ultimoJugador = 0;ultimoJugador<max_jugadores && jugadores_listos;++ultimoJugador){
                if (this->jugadores[ultimoJugador] != NULL) {
                    //Pedimos el lock aqui pues antes podria haberse indefinido
                    this->locks[ultimoJugador]->rwl_jugadores->rlock();

                    jugadores_listos = this->jugadores[ultimoJugador]->listo();
                }
            }

            if(!jugadores_listos)
                return_error = -ERROR_JUGADOR_NO_LISTO;

            else{
                //Si los jugadores estan listos
                evento_t * nuevoevento;
                for (int i = 0; i < ultimoJugador; i++) {
                    if (this->jugadores[i] != NULL) {
                        //Pido el lock de escritura de los eventos del jugador
                        this->locks[i]->rwl_eventos->wlock();

                        nuevoevento = (evento_t*)malloc(sizeof(evento_t));
                        nuevoevento->t_id = i;
                        nuevoevento->s_id = 0;
                        nuevoevento->x = 0;
                        nuevoevento->y = 0;
                        nuevoevento->status = EVENTO_START;
                        this->eventos[i].push(nuevoevento);
                    }
                }
                this->jugando = true;

                //Suelto los locks tomados en esta instancia
                for(int i=0;i<ultimoJugador;++i)
                    if (this->jugadores[i] != NULL)
                        this->locks[i]->rwl_eventos->wunlock();
            }
            //Suelto los locks tomados en esta instancia
            for(int i=0;i<ultimoJugador;++i)
                if(this->jugadores[i] != NULL)
                    this->locks[i]->rwl_jugadores->runlock();
            this->rwl_locks->runlock();
        }
        //Suelto los locks tomados en esta instancia
        this->rwl_cantidad_jugadores->runlock();
        this->rwl_jugadores->runlock();
    }
    //Suelto los locks iniciales
    this->rwl_jugando->wunlock();

	return return_error;

}
error Modelo::reiniciar() {
    //Pido los locks
    this->rwl_jugando->wlock();
    this->rwl_jugadores->rlock();
    this->rwl_tiros->rlock();
    this->rwl_locks->rlock();

    //Comenzamos
	for (int i = 0; i < max_jugadores; i++) {
		if (this->jugadores[i] != NULL) {
            //Pido el write lock del jugador
            this->locks[i]->rwl_jugadores->wlock();
            this->locks[i]->rwl_tiros->wlock();

			this->jugadores[i]->reiniciar();
			this->tiros[i]->t_id = JUGADOR_SINDEFINIR;
			this->tiros[i]->stamp.tv_sec = 0;
			this->tiros[i]->stamp.tv_usec = 0;
			this->tiros[i]->eta = 0;
			this->tiros[i]->estado = TIRO_LIBRE;
		}
	}
	this->jugando = false;

    //Suelto los locks
	for(int i = 0;i < max_jugadores;i++)
		if(this->jugadores[i] != NULL){
            this->locks[i]->rwl_tiros->wunlock();
            this->locks[i]->rwl_jugadores->wunlock();
    }
    this->rwl_locks->runlock();
    this->rwl_tiros->runlock();
    this->rwl_jugadores->runlock();
    this->rwl_jugando->wunlock();

	return ERROR_NO_ERROR;

}

error Modelo::quitarJugador(int s_id) {
    //Variables locales
    error return_error = ERROR_NO_ERROR;

    //Pedimos los locks
    this->rwl_jugando->rlock();

    //Comenzamos
	if(this->jugando)
        return_error = -ERROR_JUEGO_EN_PROGRESO;

    else{
        //Pido los locks para borrar al jugador
        this->rwl_jugadores->wlock();

        if(this->jugadores[s_id] == NULL)
            return_error = -ERROR_JUGADOR_INEXISTENTE;
        else{
            //Pido los locks de escritura
            this->rwl_tiros->wlock();
            this->rwl_locks->wlock();

            delete this->jugadores[s_id];
            delete this->tiros[s_id];
            delete this->locks[s_id];

            this->jugadores[s_id] = NULL;
            this->tiros[s_id] = NULL;
            this->locks[s_id] = NULL;

            //Suelto los locks de escritura
            this->rwl_locks->wunlock();
            this->rwl_tiros->wunlock();
        }
        //Suelto los locks pedidos en esta instancia
        this->rwl_jugadores->wunlock();
    }

    //Liberamos los locks
    this->rwl_jugando->runlock();

	return return_error;
}

int Modelo::apuntar(int s_id, int t_id, int x, int y, int *eta) {
    //Variables locales
	int retorno;

    //Pido locks iniciales
    this->rwl_jugando->rlock();

    //Comenzamos
	if(!this->jugando)
        retorno = -ERROR_JUEGO_NO_COMENZADO;

    else{
        //Pido locks
        this->rwl_jugadores->rlock();

        if(this->jugadores[s_id] == NULL || this->jugadores[t_id] == NULL)
            retorno = -ERROR_JUGADOR_INEXISTENTE;

        else {
            //Pido locks
            this->rwl_tiros->rlock();
            this->rwl_locks->rlock();
            if(s_id == t_id)
                this->locks[s_id]->rwl_jugadores->wlock();
            else
                this->locks[s_id]->rwl_jugadores->rlock();

            if(!this->jugadores[s_id]->esta_vivo())
                retorno = -ERROR_JUGADOR_HUNDIDO;
            else {
                //Pido locks
                this->locks[s_id]->rwl_tiros->wlock();

                if(!this->es_posible_apuntar(this->tiros[s_id]))
                    retorno = RESULTADO_APUNTADO_DENEGADO;
                else {
                    //Pido locks
                    if(s_id != t_id)this->locks[t_id]->rwl_jugadores->wlock();

                    retorno = this->jugadores[t_id]->apuntar(s_id, x, y);
                    if(retorno == RESULTADO_APUNTADO_ACEPTADO){
                        //Pido locks
                        this->locks[t_id]->rwl_eventos->wlock();

                        this->tiros[s_id]->t_id = t_id;
                        gettimeofday(&this->tiros[s_id]->stamp, NULL);
                        this->tiros[s_id]->eta = rand() % (MAX_ETA) + MIN_ETA;
                        //dprintf("ETA %d ms", this->tiros[s_id]->eta);
                        *eta = this->tiros[s_id]->eta;
                        this->tiros[s_id]->estado = TIRO_APUNTADO;
                        this->tiros[s_id]->x = x;
                        this->tiros[s_id]->y = y;
                        evento_t * nuevoevento = (evento_t*)malloc(sizeof(evento_t));
                        nuevoevento->t_id = t_id;
                        nuevoevento->s_id = s_id;
                        nuevoevento->x = x;
                        nuevoevento->y = y;
                        nuevoevento->status = CASILLA_EVENTO_INCOMING;
                        this->eventos[t_id].push(nuevoevento);

                        //Suelto locks de la instancia
                        this->locks[t_id]->rwl_eventos->wunlock();
                    }
                    //Suelto locks de la instancia
                    if(s_id != t_id)this->locks[t_id]->rwl_jugadores->wunlock();
                }
                //Suelto los locks de esta instancia
                this->locks[s_id]->rwl_tiros->wunlock();
            }
            //Suelto los locks de esta instancia
            if(s_id == t_id)
                this->locks[s_id]->rwl_jugadores->wunlock();
            else
                this->locks[s_id]->rwl_jugadores->runlock();
            this->rwl_locks->runlock();
            this->rwl_tiros->runlock();

        }
        //Suelto los locks de esta instancia
        this->rwl_jugadores->runlock();
    }

    //Suelto los locks iniciales
    this->rwl_jugando->runlock();

	return retorno;
}

int Modelo::dame_eta(int s_id) {
    //Variables locales
    int retorno;
    //Pido locks iniciales
    this->rwl_jugando->rlock();

	if(!this->jugando)
        retorno = -ERROR_JUEGO_NO_COMENZADO;
	else {
        //Pido locks
        this->rwl_jugadores->rlock();

        if(this->jugadores[s_id] == NULL)
            retorno = -ERROR_JUGADOR_INEXISTENTE;
        else{
            //Pido locks
            this->rwl_tiros->rlock();
            this->rwl_locks->rlock();
            this->locks[s_id]->rwl_tiros->rlock();

            if (this->tiros[s_id]->estado != TIRO_APUNTADO)
                retorno = -ERROR_ESTADO_INCORRECTO;
            else
                retorno = this->tiros[s_id]->eta;

            //Suelto locks de la instancia
            this->locks[s_id]->rwl_tiros->runlock();
            this->rwl_locks->runlock();
            this->rwl_tiros->runlock();
        }
        //Suelto locks de la instancia
        this->rwl_jugadores->runlock();
    }

    //Suelto los locks iniciales
    this->rwl_jugando->runlock();

	return retorno;
}

//Los locks comentados no son necesarios,
//pues la funcion "tocar" es solamente invocada
//por "actualizar_jugador", y la misma ya
//pide los locks necesarios.
int Modelo::tocar(int s_id, int t_id) {
    //Variables locales
    int retorno = -ERROR_ETA_NO_TRANSCURRIDO;

    //Pido locks iniciales
    //this->rwl_jugando->rlock();

    //Comenzamos
	if(!this->jugando)
        retorno = -ERROR_JUEGO_NO_COMENZADO;

	else {
        //Pido locks
        //this->rwl_jugadores->rlock();

        if(this->jugadores[s_id] == NULL || this->jugadores[t_id] == NULL)
            retorno = -ERROR_JUGADOR_INEXISTENTE;

        else{
            //Pido locks
            //this->rwl_tiros->rlock();
            //this->rwl_locks->rlock();
            //this->locks[s_id]->rwl_jugadores->wlock();
            //this->locks[s_id]->rwl_tiros->wlock();
            //this->locks[s_id]->rwl_eventos->wlock();
            this->locks[t_id]->rwl_jugadores->wlock();
            this->locks[t_id]->rwl_eventos->wlock();

            if(this->es_posible_tocar(this->tiros[s_id])) {
                int x = this->tiros[s_id]->x;
                int y = this->tiros[s_id]->y;
                retorno = this->jugadores[t_id]->tocar(s_id, x, y);
                if(retorno == EMBARCACION_RESULTADO_TOCADO ||
                    retorno == EMBARCACION_RESULTADO_HUNDIDO ||
                    retorno == EMBARCACION_RESULTADO_HUNDIDO_M ||
                    retorno == EMBARCACION_RESULTADO_AGUA ||
                    retorno == EMBARCACION_RESULTADO_AGUA_H
                    ){

                    this->tiros[s_id]->estado = TIRO_LIBRE;
                    //Evento para el tirado
                    evento_t * nuevoevento = (evento_t*)malloc(sizeof(evento_t));
                    nuevoevento->t_id = t_id;
                    nuevoevento->s_id = s_id;
                    nuevoevento->x = x;
                    nuevoevento->y = y;
                    nuevoevento->status = retorno;
                    this->eventos[t_id].push(nuevoevento);
                    //Evento para el tirador
                    nuevoevento = (evento_t*)malloc(sizeof(evento_t));
                    nuevoevento->t_id = t_id;
                    nuevoevento->s_id = s_id;
                    nuevoevento->x = x;
                    nuevoevento->y = y;
                    nuevoevento->status = retorno;
                    this->eventos[s_id].push(nuevoevento);
                }
                if(retorno == EMBARCACION_RESULTADO_HUNDIDO) {
                    this->jugadores[s_id]->agregar_puntaje(PUNTAJE_HUNDIDO);
                } else if(retorno == EMBARCACION_RESULTADO_HUNDIDO_M) {
                    this->jugadores[s_id]->agregar_puntaje(PUNTAJE_HUNDIDO+PUNTAJE_MISMO_JUGADOR);
                } else if(retorno == EMBARCACION_RESULTADO_TOCADO) {
                    this->jugadores[s_id]->agregar_puntaje(PUNTAJE_TOCADO);
                } else if(retorno == EMBARCACION_RESULTADO_AGUA_H) {
                    this->jugadores[s_id]->agregar_puntaje(PUNTAJE_MAGALLANES);
                }
            }
            //Suelto locks de esta instancia
            this->locks[t_id]->rwl_eventos->wunlock();
            this->locks[t_id]->rwl_jugadores->wunlock();
            //this->locks[s_id]->rwl_eventos->wunlock();
            //this->locks[s_id]->rwl_tiros->wunlock();
            //this->locks[s_id]->rwl_jugadores->wunlock();
            //this->rwl_locks->runlock();
            //this->rwl_tiros->runlock();
        }
        //Suelto locks de esta instancia
        //this->rwl_jugadores->runlock();
    }
    //Suelto los locks iniciales
    //this->rwl_jugando->runlock();

	return retorno;
}
#ifdef DEBUG
void Modelo::print() {
	printf("MODELO -- NJugadores %d, Jugando %d\n=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n", this->cantidad_jugadores, this->jugando);
	for (int i = 0; i < max_jugadores; i++) {
		if (this->jugadores[i] != NULL) {
			this->jugadores[i]->print();
			printf( "Tiro: id %d, stamp (%lu, %lu), eta %d, estado %d\n", this->tiros[i]->t_id, this->tiros[i]->stamp.tv_sec, (long unsigned int)this->tiros[i]->stamp.tv_usec, this->tiros[i]->eta, this->tiros[i]->estado);
		}
		printf("\n");
	}

	printf("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
}
#endif
bool  Modelo::es_posible_tocar(tiro_t * tiro) {
	bool pasoeleta = tiro->estado == TIRO_APUNTADO;
	if (pasoeleta) {
		struct timeval ahora;
		struct timeval * antes = &tiro->stamp;
		gettimeofday(&ahora, NULL);
		int msec = (ahora.tv_sec - antes->tv_sec) * 1000;
		msec += (ahora.tv_usec - antes->tv_usec) / 1000;

		if (msec < tiro->eta) {
			pasoeleta = false;
		}
	}

	return pasoeleta;
}

bool  Modelo::es_posible_apuntar(tiro_t * tiro) {
	return tiro->estado == TIRO_LIBRE;
}

int Modelo::hayEventos(int s_id) {
    //Variables locales
    int retorno;

    //Pido locks
    this->rwl_locks->rlock();
    this->locks[s_id]->rwl_eventos->rlock();

	retorno = this->eventos[s_id].size();

    //Suelto los locks
    this->locks[s_id]->rwl_eventos->runlock();
    this->rwl_locks->runlock();

    return retorno;
}

evento_t * Modelo::dameEvento(int s_id) {
    //Variables locales
	evento_t *retorno = NULL;

    //Pido locks
    this->rwl_locks->rlock();
    this->locks[s_id]->rwl_eventos->wlock();

	if(!this->eventos[s_id].empty()){
		retorno = this->eventos[s_id].front();
		this->eventos[s_id].pop();
	}

    //Suelto los locks
    this->locks[s_id]->rwl_eventos->wunlock();
    this->rwl_locks->runlock();

    return retorno;
}

evento_t * Modelo::actualizar_jugador(int s_id) {
    //Variables locales
    evento_t *retorno = NULL;

    //Pedimos locks
    this->rwl_jugando->rlock();
    this->rwl_jugadores->rlock();
    this->rwl_tiros->rlock();
    this->rwl_locks->rlock();
    this->locks[s_id]->rwl_jugadores->wlock();
    this->locks[s_id]->rwl_tiros->wlock();
    this->locks[s_id]->rwl_eventos->wlock();

    //Comenzamos
    int t_id = this->tiros[s_id]->t_id;
	int tocado = this->tocar(s_id, t_id);

    if(!this->eventos[s_id].empty()) {
        retorno = this->eventos[s_id].front();
		this->eventos[s_id].pop();
    } else {
		retorno = (evento_t*)malloc(sizeof(evento_t));
		retorno->s_id = s_id;
		retorno->t_id = -1;
		retorno->status = -tocado;
		retorno->x = 0;
		retorno->y = 0;
	}

    //Suelto locks
    this->locks[s_id]->rwl_eventos->wunlock();
    this->locks[s_id]->rwl_tiros->wunlock();
    this->locks[s_id]->rwl_jugadores->wunlock();
    this->rwl_locks->runlock();
    this->rwl_tiros->runlock();
    this->rwl_jugadores->runlock();
    this->rwl_jugando->runlock();

    return retorno;
}
