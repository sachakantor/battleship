#include <modelo.h>
#ifdef DEBUG
#include <cstdio>
#endif
#include <constantes.h>
#include <globales.h> 
#include <stdlib.h>

Modelo::Modelo(int njugadores, int tamtablero, int tamtotalbarcos){
	max_jugadores = njugadores;
	tamanio_tablero = tamtablero;
	tamanio_total_barcos = tamtotalbarcos;
	
	this->jugadores = new Jugador*[max_jugadores];
	this->eventos = new std::queue<evento_t *>[max_jugadores];
	this->tiros = new tiro_t*[max_jugadores];
	for (int i = 0; i < max_jugadores; i++) {
		this->jugadores[i] = NULL;
		this->tiros[i] = NULL;
	}
	this->cantidad_jugadores = 0;
	this->jugando = false;

}
Modelo::~Modelo() {
	for (int i = 0; i < max_jugadores; i++) {
		if (this->jugadores[i] != NULL) {
			delete this->jugadores[i];
			delete this->tiros[i];
		}
	}
	delete[] this->jugadores;
	delete[] this->tiros;
}

int Modelo::agregarJugador(std::string nombre) {
	if (this->jugando) return -ERROR_JUEGO_EN_PROGRESO;
	int nuevoid = 0;
	for (nuevoid = 0; nuevoid < max_jugadores && this->jugadores[nuevoid] != NULL; nuevoid++);
	
	if (this->jugadores[nuevoid] != NULL) return -ERROR_MAX_JUGADORES;
	
	this->tiros[nuevoid] = new tiro_t();
	this->tiros[nuevoid]->t_id = JUGADOR_SINDEFINIR;
	this->tiros[nuevoid]->stamp.tv_sec = 0;
	this->tiros[nuevoid]->stamp.tv_usec = 0;
	this->tiros[nuevoid]->eta = 0;
	this->tiros[nuevoid]->estado = TIRO_LIBRE;
	
	this->jugadores[nuevoid] = new Jugador(nombre);
	this->cantidad_jugadores++;
	
	return nuevoid;
}


error Modelo::ubicar(int t_id, int * xs, int *  ys, int tamanio) {
	if (this->jugando) return -ERROR_JUEGO_EN_PROGRESO;
	if (this->jugadores[t_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	return this->jugadores[t_id]->ubicar(xs, ys, tamanio);
}

error Modelo::borrar_barcos(int t_id) {
	if (this->jugando) return -ERROR_JUEGO_EN_PROGRESO;
	if (this->jugadores[t_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	return this->jugadores[t_id]->quitar_barcos();
}

error Modelo::empezar() {
	if (this->jugando) return -ERROR_JUEGO_EN_PROGRESO;
	bool completos = this->cantidad_jugadores == max_jugadores;
	for (int i = 0; i < max_jugadores && completos; i++) {
		if (this->jugadores[i] != NULL) {
			completos = completos && this->jugadores[i]->listo();
		}
	}
	if (! completos) return -ERROR_JUGADOR_NO_LISTO;
	
	evento_t * nuevoevento;
	for (int i = 0; i < max_jugadores && completos; i++) {
		if (this->jugadores[i] != NULL) {
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
	return ERROR_NO_ERROR;
	
}
error Modelo::reiniciar() {
	for (int i = 0; i < max_jugadores; i++) {
		if (this->jugadores[i] != NULL) {
			this->jugadores[i]->reiniciar();
			this->tiros[i]->t_id = JUGADOR_SINDEFINIR;
			this->tiros[i]->stamp.tv_sec = 0;
			this->tiros[i]->stamp.tv_usec = 0;
			this->tiros[i]->eta = 0;
			this->tiros[i]->estado = TIRO_LIBRE;
		}
	}
	this->jugando = false;
	
	return ERROR_NO_ERROR;
	
}

error Modelo::quitarJugador(int s_id) {
	if (this->jugando) return -ERROR_JUEGO_EN_PROGRESO;
	if (this->jugadores[s_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	delete this->jugadores[s_id];
	delete this->tiros[s_id];
	
	this->jugadores[s_id] = NULL;
	this->tiros[s_id] = NULL;
	
	return ERROR_NO_ERROR;
}

int Modelo::apuntar(int s_id, int t_id, int x, int y, int *eta) {
	if (!this->jugando) return -ERROR_JUEGO_NO_COMENZADO;
	if (this->jugadores[s_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	if (this->jugadores[t_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	if (! this->jugadores[s_id]->esta_vivo()) return -ERROR_JUGADOR_HUNDIDO;
	int retorno = RESULTADO_APUNTADO_DENEGADO;
	if (this->es_posible_apuntar(this->tiros[s_id])) {
		retorno = this->jugadores[t_id]->apuntar(s_id, x, y);
		if (retorno == RESULTADO_APUNTADO_ACEPTADO) {
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
		}
	}
	
	return retorno;
	
}

int Modelo::dame_eta(int s_id) {
	if (!this->jugando) return -ERROR_JUEGO_NO_COMENZADO;
	if (this->jugadores[s_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	tiro_t * tiro = this->tiros[s_id];
	if (tiro->estado != TIRO_APUNTADO) return -ERROR_ESTADO_INCORRECTO;

	return tiro->eta;
}

int Modelo::tocar(int s_id, int t_id) {
	if (!this->jugando) return -ERROR_JUEGO_NO_COMENZADO;
	if (this->jugadores[s_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;
	if (this->jugadores[t_id] == NULL) return -ERROR_JUGADOR_INEXISTENTE;

	
	int retorno = -ERROR_ETA_NO_TRANSCURRIDO;
	if (this->es_posible_tocar(this->tiros[s_id])) {
		int x = this->tiros[s_id]->x;
		int y = this->tiros[s_id]->y;
		retorno = this->jugadores[t_id]->tocar(s_id, x, y);
		if (retorno == EMBARCACION_RESULTADO_TOCADO ||
			retorno == EMBARCACION_RESULTADO_HUNDIDO ||
			retorno == EMBARCACION_RESULTADO_HUNDIDO_M ||
			retorno == EMBARCACION_RESULTADO_AGUA ||
			retorno == EMBARCACION_RESULTADO_AGUA_H
			) {
			
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
		if (retorno == EMBARCACION_RESULTADO_HUNDIDO) {
			this->jugadores[s_id]->agregar_puntaje(PUNTAJE_HUNDIDO);
		} else if (retorno == EMBARCACION_RESULTADO_HUNDIDO_M) {
			this->jugadores[s_id]->agregar_puntaje(PUNTAJE_HUNDIDO+PUNTAJE_MISMO_JUGADOR);
		} else if (retorno == EMBARCACION_RESULTADO_TOCADO) {
			this->jugadores[s_id]->agregar_puntaje(PUNTAJE_TOCADO);
		} else if (retorno == EMBARCACION_RESULTADO_AGUA_H) {
			this->jugadores[s_id]->agregar_puntaje(PUNTAJE_MAGALLANES);
		} 
	}
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
	return this->eventos[s_id].size();
}

evento_t * Modelo::dameEvento(int s_id) {
	evento_t *retorno = NULL;
	if (! this->eventos[s_id].empty() ) {
		retorno = this->eventos[s_id].front();
		this->eventos[s_id].pop();
	}
    return retorno;

}

evento_t * Modelo::actualizar_jugador(int s_id) {
    evento_t *retorno = NULL;
	tiro_t * t = this->tiros[s_id];
	int tocado = this->tocar(s_id, t->t_id);
    if (! this->eventos[s_id].empty() ) {
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
    return retorno;
}


