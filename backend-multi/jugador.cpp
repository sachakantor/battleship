#include <jugador.h>
#ifdef DEBUG
#include <cstdio>
#endif
#include <globales.h>

Jugador::Jugador(const std::string nuevo_nombre) {
	this->nombre = std::string(nuevo_nombre);
	this->tablero = new Tablero(tamanio_tablero, tamanio_total_barcos);
	this->puntaje = 0;
	this->cantidad_barcos_flotando = 0;
}
Jugador::~Jugador() {
	delete this->tablero;
}

int	Jugador::apuntar(int s_id, int x, int y) {
	return this->tablero->apuntar(s_id, x, y);
}

int	Jugador::tocar(int s_id, int x, int y) {
	int retorno = this->tablero->tocar(s_id, x, y);
	if (retorno == EMBARCACION_RESULTADO_HUNDIDO || retorno == EMBARCACION_RESULTADO_HUNDIDO_M) {
		this->cantidad_barcos_flotando--;
	}
	return retorno;

}

error Jugador::ubicar(int * xs, int * ys, int tamanio) {
	error retorno = this->tablero->ubicar(xs, ys, tamanio);
	if (retorno == ERROR_NO_ERROR) {
		this->cantidad_barcos_flotando++;
	}
	return retorno;
}

void Jugador::agregar_puntaje(int mas) {
	this->puntaje += mas;
}
int	Jugador::dame_puntaje() {
	return this->puntaje;
}

bool Jugador::listo() {
	return this->tablero->completo();
}
void Jugador::reiniciar() {
	delete this->tablero;
	this->tablero = new Tablero(tamanio_tablero, tamanio_total_barcos);
	this->puntaje = 0;
	this->cantidad_barcos_flotando = 0;
}

bool Jugador::esta_vivo(){
	return this->cantidad_barcos_flotando != 0;
}


std::string Jugador::dame_nombre() {
    return this->nombre;
}

error Jugador::quitar_barcos() {
	this->tablero = new Tablero(tamanio_tablero, tamanio_total_barcos);
	this->puntaje = 0;
	this->cantidad_barcos_flotando = 0;
	return ERROR_NO_ERROR;
}

#ifdef DEBUG

void Jugador::print() {
	printf("JUGADOR -- Nombre %s, Puntaje %d\n", this->nombre.c_str(), this->puntaje);
	this->tablero->print();
}
#endif