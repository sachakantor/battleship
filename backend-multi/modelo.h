#ifndef __MODELO_H__
#define __MODELO_H__

#include <string>
#include <jugador.h>
#include <queue>

#include <sys/time.h>

typedef struct tiro {
	int				t_id;		/* a que jugador se hizo el tiro */
	struct timeval	stamp;		/* cuando se hizo el tiro */
	int				eta;		/* tiempo de espera hasta finalizar el tiro (en milisgeundos) */
	int				estado;		/* Libre o Apuntado */
    int             x;
    int             y;
} tiro_t;

typedef struct evento {
	int				t_id;
	int				x;
	int				y;
	int				status;
	int				s_id;
} evento_t;

class Modelo {
    public:
        Modelo(int njugadores, int tamtablero, int tamtotalbarcos);
        ~Modelo();
        int     agregarJugador(std::string nombre);		/* agrega el jugador y devuelve el id */
    
        error	ubicar(int t_id, int * xs, int *  ys, int tamanio);
														/* ubica el barco para el
                                                         * jugador t_id */
		error	borrar_barcos(int t_id);
	
        error   empezar();								/* pasa a la fase de tiroteo */
		
        error   reiniciar();							/* reinicia el score y los jugadores,
                                                         * pasa a la fase de ubicacion */
    
		error	quitarJugador(int s_id);				/* quita un jugador del juego */
    
		
        int		apuntar(int s_id, int t_id, int x, int y, int *eta);	/* s_id apunta a (x,y)
																		* del jugador t_id */
    
		int		tocar(int s_id, int t_id);    /* s_id toca a (x, y) 
                                                               del jugador t_id */
    
        int     dame_eta(int s_id);
	
		evento_t				*actualizar_jugador(int t_id);	/* devuelve el proximo evento del jugador */
    
        int                     hayEventos(int s_id);
        evento_t                *dameEvento(int s_id);
	
#ifdef DEBUG
		void	print();
#endif
	private:
		int						cantidad_jugadores;				/* cantidad de jugadores inscriptos */
		Jugador					**jugadores;					/* Jugadores */
		tiro_t					**tiros;						/* estado de los tiros */
		std::queue<evento_t*>	*eventos;						/* arreglo de colas de eventos por jugador */

		bool					jugando;						/* si se esta jugando (fase tiroteo) o ubicando */
		bool					es_posible_tocar(tiro_t *);		/* verifica si se cumplio el eta */
		bool					es_posible_apuntar(tiro_t *);	/* verifica si se cumplio el eta */
	
	
		friend class	Jsonificador;					/* para poder acceder a los elementos */
	
};


#endif /* __MODELO_H__ */