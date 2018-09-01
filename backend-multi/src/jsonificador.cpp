#include <jsonificador.h>
#include <globales.h>
#include <iostream>
#include <sstream>
#include <string>
#include <assert.h>
#include <stdlib.h>

Jsonificador::Jsonificador(Modelo * mod) {
	assert(mod != NULL);
	this->modelo = mod;
}

Jsonificador::~Jsonificador() {

}

std::string Jsonificador::subscribe_resp(int id) {
	std::stringstream ss;
	ss << "{\"Name\":\"Subscribe\",\"Data\":{";
	ss << "\"Boardsize\":" << tamanio_tablero << ",";
	ss << "\"Boatssize\":" << tamanio_total_barcos << ",";
	ss << "\"Id\":"<< id << "}}|";
	return ss.str();
}

std::string Jsonificador::setup_resp() {
	std::stringstream ss;
 	ss << "{\"Name\":\"Setup\",\"Data\":\"Ok\"}|" ;
	return ss.str();
}

std::string Jsonificador::error(int err) {
	std::stringstream ss;
	if (err == ERROR_NO_ERROR) {
		ss << "{ \"Name\": \"Ok\", \"Data\": {\"Msg\": \"No pasa naranja, está todo liso...\"}}|";
	} else {
		int newerr = -err;
		ss << "{ \"Name\": \"Error\", \"Data\": { \"Msg\": \"" << traducciones[newerr] << "\"}}|";
	}
	return ss.str();
}

std::string Jsonificador::start() {
    //Variables locales
	std::stringstream ss;

    //Pido locks
    this->modelo->rwl_jugadores->rlock();
    this->modelo->rwl_locks->rlock();

    //Comenzamos
	ss << "{ \"Name\": \"Start\", \"Data\": [";
	bool primero = true;
	for (int i = 0; i < max_jugadores; i++) {
		if (this->modelo->jugadores[i]!= NULL) {
			if (!primero) {
				ss << ",";
			}
            //Pido lock sabiendo el indice
            this->modelo->locks[i]->rwl_jugadores->rlock();

			ss << "{\"Id\":" << i << ", \"Name\": \""<< this->modelo->jugadores[i]->dame_nombre() << "\"}";
			if (primero) {
				primero = false;
			}

            //Suelto el lock pedido en esta instancia
            //pues no lo necesitare mas
            this->modelo->locks[i]->rwl_jugadores->runlock();
		}
	}
	ss << "]}|";

    //Suelto los locks
    this->modelo->rwl_locks->runlock();
    this->modelo->rwl_jugadores->runlock();

    //Finalizo
	return ss.str();
}



std::string Jsonificador::player_info(int id) {
    //Variables locales
	std::stringstream ss;

    //Pido locks
    this->modelo->rwl_jugadores->rlock();
    this->modelo->rwl_locks->rlock();
    this->modelo->locks[id]->rwl_jugadores->rlock();

    //Comenzamos
	Tablero * tab = this->modelo->jugadores[id]->tablero;
	int st;
	Casilla * cas;
	ss << "{ \"Name\": \"Player_Info\", \"Data\": {\"t_id\": "<< id <<", \"board\": [";
    for (int i = 0; i < tamanio_tablero; i++) {
        if (i != 0) { ss << ","; }
        ss << "[";
        for (int j = 0; j < tamanio_tablero; j++) {
            if (j != 0) { ss << ","; }
            cas = &tab->casillas[tab->traducir_cord(i, j)];
            st = cas->dameEstado();
            if (cas->dameEstado() == CASILLA_ESTADO_TOCADO) {
                st = cas->dameResultado();
            }
            ss << '"' << traducir_estado_casilla(st) << '"';
        }
        ss << "]";
    }

	ss <<"]}}|";

    //Suelto los locks
    this->modelo->locks[id]->rwl_jugadores->runlock();
    this->modelo->rwl_locks->runlock();
    this->modelo->rwl_jugadores->runlock();

    //Finalizo
	return ss.str();
}


std::string Jsonificador::shoot_resp(int resp, int eta) {
	std::stringstream ss;
	if (resp == RESULTADO_APUNTADO_ACEPTADO) {
		ss << "{ \"Name\": \"Accept\", \"Data\": {\"eta\": \""<< eta <<"\"}}|";
	} else {
		int newresp = abs(resp);
		ss << "{ \"Name\": \"Decline\", \"Data\": {\"Msg\": \""<< traducciones[newresp] <<"\"}}|";
	}
	return ss.str();
}

std::string Jsonificador::update(evento_t * event) {
	std::stringstream ss;
	if (event->status == EVENTO_START) {
		ss << this->start();
	} else {
		ss << "{ \"Name\": \"Event\", \"Data\": {";
		ss << "\"t_id\": "<< event->t_id <<",";
		ss << "\"s_id\": "<< event->s_id <<",";
		ss << "\"coords\": { \"x\":"<< event->x <<", \"y\":"<< event->y <<"},";
		ss << "\"status\": \""<< traducciones[event->status] <<"\"";
		ss << "}}";
	}
	ss << "|";

	return ss.str();
}

std::string Jsonificador::finish(void) {
	std::stringstream ss;
	ss << "{ \"Name\": \"Finish\", \"Data\": {}}";
	ss << "|";
	return ss.str();
}

std::string Jsonificador::traducir_estado_casilla(int estado) {
	std::string retorno = "";
	switch	(estado) {
		case CASILLA_ESTADO_LIBRE:
		case CASILLA_ESTADO_APUNTADO:
			retorno = "U";
			break;
		case EMBARCACION_RESULTADO_TOCADO:
			retorno = "T";
			break;
		case EMBARCACION_RESULTADO_HUNDIDO:
		case EMBARCACION_RESULTADO_HUNDIDO_M:
			retorno = "H";
			break;
		case EMBARCACION_RESULTADO_AGUA:
		case EMBARCACION_RESULTADO_AGUA_H:
			retorno = "A";
			break;
	}

	return retorno;
}


std::string Jsonificador::scores() {
    //Variables locales
    std::stringstream ss;
    bool primero = true;

    //Pido locks
    this->modelo->rwl_jugadores->rlock();
    this->modelo->rwl_locks->rlock();

    ss << "{ \"Name\": \"Scores\", \"Data\": [";
    for (int i = 0; i < max_jugadores; i++) {
        if (this->modelo->jugadores[i]!= NULL) {
            //Pido lock del usuario
            this->modelo->locks[i]->rwl_jugadores->rlock();

            if (!primero) {
                ss << ",";
            }
            ss << "{\"Id\":" << i << ", \"Name\": \""<< this->modelo->jugadores[i]->dame_nombre() << "\", \"Score\":" <<
            this->modelo->jugadores[i]->dame_puntaje() << ", \"Vivo\":" << (this->modelo->jugadores[i]->esta_vivo() ? 1 : 0) << "}";
            if (primero) {
                primero = false;
            }

            //Suelto locks
            this->modelo->locks[i]->rwl_jugadores->runlock();
        }
    }
    ss << "]}|";

    //Suelto locks
    this->modelo->rwl_locks->runlock();
    this->modelo->rwl_jugadores->runlock();

    return ss.str();
}
