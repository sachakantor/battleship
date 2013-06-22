#!/bin/bash

#Errores
ERROR_NO_ERROR=0
ERROR_CMD_NO_VALIDO=1
ERROR_DIR_DESCONOCIDA=2
ERROR_BARCO_NO_UBICADO=3
ERROR_I_LOST=4
ERROR_ALREADY_FIGHTING=5
ERROR_I_WON=6
ERROR_STATUS=7
ERROR_ERROR_NO_KNOWN=8
ERROR_CTRL_SIG=100

#Variables
return_code=$ERROR_NO_ERROR
working_dir="."
host="${1}"
port="${2}"
jugador_nombre="${3}"
game_started=false
#comlog="${working_dir}/battlesystemClient.`tr -d [:space:] <<<${jugador_nombre}`.$$.log"
modoUSA=false
compatriotas="${working_dir}/compatriotas.lst"

#Variables que se iran cargando
#al parsear las respuestas del server
jugador_id=""
boardsize=""
boatssize=""
boats_cinco=0
boats_cuatro=0
boats_tres=0
boats_dos=0
boats_uno=0
boats_cant=0

#Variables a carguar una vez comenzado
#el juego
rivales_nombres="" #Array de rivales, el indice es el id
rivales_pos="" #Array de posiciones aun no atacadas por rival
rivales_vivos="" #String de ids de rivales vivos
rivales_cant="" #Cantidad de inscriptos en esta fiesta
cmd=""
data=""

#Comandos
subscribe="Subscribe"
setup="Setup"
shoot="Shoot"
get_update="Get_Update"
nop="Nop"

#Comandos de respuestas
#subscribe="Subscribe"
ok_resp="Ok"
error_resp="Error"
start_resp="Start"
accept_resp="Accept"
event_resp="Event"
decline_resp="Decline"

#Mensajes
msg_perdi="No puedes disparar mas una vez hundidos todos tus barcos"
msg_pos_inv="Posiciones invalidas"
msg_denegado="Denegado"
msg_jugando="Juego en progreso"

#Status
agua="A"
tocado="T"
hundido="H"
incoming="I"
shooting="S"
eta_no_trans="ETA no transcurrido"

#Creamos fifos
fd_input_server="4"
fd_output_server="3"
fd_fifo_server="${working_dir}/fifo_server.$$"
fd_fifo_rivales_muertos="${working_dir}/fifo_rivales_muertos.$$"
mkfifo $fd_fifo_server
mkfifo $fd_fifo_rivales_muertos

#Funciones
function cleanup(){
    #$1= return code
    #Si hubo un error

    #Borramos fifos y matamos procesos del background
    printf '\r\vCerrando procesos hijos\r\n' && pkill -P $$ 2>/dev/null
    printf 'Cerramos el fd\r\n' && exec {fd_output_server}>&-
    printf 'Borrando FIFOs\r\n' && rm -f ${fd_fifo_rivales_muertos} ${fd_fifo_server} 2>/dev/null
    if ${modoUSA}; then
        [[ `ls "${working_dir}"/fifo_server.* 2>/dev/null | wc -l` -eq 0 ]] && rm -f ${compatriotas}
    fi

    exit $1
}

function send_cmd(){
    #Logueamos el comando a enviar siempre que no sea un Nop
    #[[ "${1}" != "${nop}" ]] && echo "Enviando: {\"Name\":\"${1}\",\"Data\":${2}}|" >>${comlog}
    printf "{\"Name\":\"${1}\",\"Data\":${2}}|" >&${fd_input_server}

    return $?
}

function atacar(){
    #Variables locales
    return_code=$ERROR_NO_ERROR

    #Actualizo la lista de rivales
    while read -t 0.01 muerto; do
        printf "\rBorrando a \"${rivales_nombres[${muerto}]}\" de mi lista de ataque\r\n\n"
        rivales_vivos=`sed -e "s/ \+ / /g" <<<${rivales_vivos/${muerto}}`
    done <>${fd_fifo_rivales_muertos} #Abro el pipe en modo rw asi no es un request bloqueante
    
    #Busco alguien vivo a quien atacar
    pos=""
    until [ ! -z "$pos" ] || [ -z "`tr -d [:space:] <<<${rivales_vivos}`" ]; do
        target_id="`shuf -e $rivales_vivos -n 1`"
        
        #Si estamos en modoUSA, verificamos que no sea un compatriota
        if ${modoUSA}; then
            until [ `grep -wc "${target_id}" ${compatriotas}` -eq 0 ] || \
                  [ -z "`tr -d [:space:] <<<${rivales_vivos}`" ]; do
                rivales_vivos=`sed -e "s/ \+ / /g" <<<${rivales_vivos/${target_id}}`
                target_id="`shuf -e $rivales_vivos -n 1`"
            done

            #Si no hay mas rivales no-compatriotas salgo dejo de buscar enemigos
            [[ -z "$target_id" ]] && break
        fi

        read -d ' ' pos <<<${rivales_pos[${target_id}]}

        #Si no hay posiciones, ya lo ataque todo
        [[ -z "$pos" ]] && rivales_vivos=${rivales_vivos/${target_id}}
    done

    #Si encontre un rival vivo, ataco, sino gane
    if [ -z "`tr -d [:space:] <<<${rivales_vivos}`" ]; then
        printf '\v\t\t¡¡¡VICTORIA!!!\r\v'
        return_code=$ERROR_I_WON
    else
        x=`cut -d',' -f1 <<<${pos}`
        y=`cut -d',' -f2 <<<${pos}`

        #Elimino a la posicion que voy a atacar
        rivales_pos[${target_id}]=${rivales_pos[${target_id}]/${pos}}
        
        #Ataco
        echo "Atacando a ${rivales_nombres[${target_id}]}: x=${x},y=${y}"
        send_cmd "Shoot" "{\"t_id\":${target_id},\"coord\":{\"x\":\"${x}\",\"y\":\"${y}\"},\"status\":\"${shooting}\",\"s_id\":${jugador_id}}"
        return_code=$?
    fi
    
    return $return_code
}

function meter_barco(){
    #Variables locales
    return_code=$ERROR_NO_ERROR
    pos="$1"
    dir="$2"
    size_boat="$3"
    size_limit="$4"
    x=`echo $pos | cut -d',' -f1`
    y=`echo $pos | cut -d',' -f2`

    #Comenzamos... corremos dos veces,
    #si no entra en una direccion, vemos en la otra
    entro=false
    tries=0
    until $entro || [ $tries -eq 2 ] ; do
        entro=true
        posiciones_a_usar="${pos}"
        posiciones_a_quitar=""

        case "$dir" in
        "vertical")
            for ((i=1;i<${size_boat} && ${size_boat}+${y}-1<${size_limit};i++)); do
                if [ `grep -c "${x},$((${y}+${i}))" <<<${posiciones_xy_disponibles}` -ne 1 ]; then
                    entro=false
                    dir="horizontal"
                    break
                else
                    #El que vimos que esta libre
                    posiciones_a_usar+=" ${x},$((${y}+${i}))"
                    #sus aledanos
                    posiciones_a_quitar+=" ${x},$((${y}+${i})) $((${x}+1)),$((${y}+${i})) $((${x}-1)),$((${y}+${i}))"
                fi
            done

            #Verifico que hayamos metido el barco entero
            [[ `wc -w <<<$posiciones_a_usar` -ne ${size_boat} ]] && entro=false

            #Si entro agrego los aledanos de las puntas
            if ${entro}; then
                posiciones_a_quitar+=" ${x},$((${y}-1)) $((${x}+1)),$((${y}-1)) $((${x}-1)),$((${y}-1))" #puntas iniciales
                posiciones_a_quitar+=" ${x},$((${y}+${size_boat})) $((${x}+1)),$((${y}+${size_boat})) $((${x}-1)),$((${y}+${size_boat}))" #puntas finales
            fi
            ;;
        "horizontal")
            for ((i=1;i<${size_boat} && ${size_boat}+${x}-1<${size_limit};i++)); do
                if [ `grep -c "$((${x}+${i})),${y}" <<<${posiciones_xy_disponibles}` -ne 1 ]; then
                    entro=false
                    dir="vertical"
                    break
                else
                    #El que vimos que esta libre
                    posiciones_a_usar+=" $((${x}+${i})),${y}"
                    #sus aledanos
                    posiciones_a_quitar+=" $((${x}+${i})),${y} $((${x}+${i})),$((${y}+1)) $((${x}+${i})),$((${y}-1))"
                fi
            done

            #Verifico que hayamos metido el barco entero
            [[ `wc -w <<<$posiciones_a_usar` -ne ${size_boat} ]] && entro=false

            #Si entro agrego los aledanos de las puntas
            if ${entro}; then
                posiciones_a_quitar+=" $((${x}-1)),${y} $((${x}-1)),$((${y}+1)) $((${x}-1)),$((${y}-1))" #puntas iniciales
                posiciones_a_quitar+=" $((${x}+${size_boat})),${y} $((${x}+${size_boat})),$((${y}+1)) $((${x}+${size_boat})),$((${y}-1))" #puntas finales
            fi
            ;;
        *)
            echo "Direccion desconocida: $dir" >&2
            return_code=$ERROR_DIR_DESCONOCIDA
            ;;
        esac

        let tries++
    done

    [[ "$entro" == "false" ]] &&  return_code=$ERROR_BARCO_NO_UBICADO

    return $return_code
}

function cargar_botes(){
    #Variables locales
    barcos=""
    direcciones=( "vertical" "horizontal" )
    posiciones_xy_disponibles=""

    #Inicializamos las posiciones disponibles del tablero
    for ((x=0;x<${boardsize};x++)); do
        for ((y=0;y<${boardsize};y++)); do
            posiciones_xy_disponibles+=" ${x},${y}"
        done
    done

    #Parametros recibidos
    #echo "Cantidad de barcos s|#: 1:$1|2:$2|3:$3|4:$4|5:$5"
    #printf "Tam\t| Pos1\t| Pos2\t| Pos3\t| Pos4\t| Pos5\t|\r\n"

    #Comenzamos
    #Iteramos por los parametros
    for size in {5..1}; do
        if [ ${!size} -gt 0 ]; then

            #Inserto ${!size} barcos de tamaño ${size}
            for ((boat=0;boat<${!size};boat++)); do
                #Busco una posicion donde entre
                for pos in `shuf -e $posiciones_xy_disponibles`; do
                    posiciones_a_usar=""
                    posiciones_a_quitar=""
                    meter_barco "$pos" "${direcciones[`shuf -i 0-1 -n 1`]}" "$size" "$boardsize"
                    [[ "$?" -eq "0" ]] && break
                done

                #printf "%u\t| %b\t| %b\t| %b\t| %b\t| %b\t|\r\n" $size $posiciones_a_usar

                #Guardamos el barco
                barcos+="["
                for i in $posiciones_a_usar; do
                    #Guardamos el barco posicionado
                    x=`echo $i | cut -d',' -f1`
                    y=`echo $i | cut -d',' -f2`
                    barcos+="{\"x\":${x},\"y\":${y}},"
                done

                #Eliminamos las zonas del barco y aledanas
                for i in $posiciones_a_quitar; do
                    posiciones_xy_disponibles=${posiciones_xy_disponibles/${i}}
                done

                #Pasamos al siguiente barco (la coma la debemos borrar despues)
                barcos=${barcos%?} #para borra la coma
                barcos+="],"
            done
        fi
    done

    barcos=${barcos%?} #borrramos la ultima coma
    send_cmd "$setup" "{\"s_id\":${jugador_id}, \"boats\":[${barcos}]}"

    return $ERROR_NO_ERROR
}

function subscribe(){
    #$1 = {"Name":"Subscribe","Data":{"Boardsize":12,"Boatssize":1,"Id":0}}
    
    #Variables locales
    jugador_id=`jshon -eId <<<$1`
    boardsize=`jshon -eBoardsize <<<$1`
    boatssize=`jshon -eBoatssize <<<$1`

    #Calculamos la cantidad de botes de cada tamaño
    boats_cinco=0
    boats_cuatro=0
    boats_tres=0
    boats_dos=0
    boats_uno=0
    boats_cant=0
    casilleros_tot=$boatssize
    while [ $casilleros_tot -gt 0 ]; do

        if [ $casilleros_tot -ge 5 ]; then
            let boats_cinco++
            let boats_cant++
            casilleros_tot=$((${casilleros_tot}-5))
        fi

        if [ $casilleros_tot -ge 4 ]; then
            let boats_cuatro++
            let boats_cant++
            casilleros_tot=$((${casilleros_tot}-4))
        fi

        if [ $casilleros_tot -ge 3 ]; then
            let boats_tres++
            let boats_cant++
            casilleros_tot=$((${casilleros_tot}-3))
        fi

        if [ $casilleros_tot -ge 2 ]; then
            let boats_dos++
            let boats_cant++
            casilleros_tot=$((${casilleros_tot}-2))
        fi

        if [ $casilleros_tot -ge 1 ]; then
            let boats_uno++
            let boats_cant++
            casilleros_tot=$((${casilleros_tot}-1))
        fi
    done

    #Lanzamos el setup
    cargar_botes $boats_uno $boats_dos $boats_tres $boats_cuatro $boats_cinco

    return $?
}

function start_batalla(){
    #$1 = [{"Id":0, "Name": "jugador_0"},{"Id":1, "Name": "jugador_1"}]
    #$2 = boardsize

    #Como al ultimo jugador en inscribirse le mandan
    #dos eventos "Start"
    if ! ${game_started}; then
        game_started=true

        #Verificamos el modoUSA (los robots no se atacan entre si)
        if ${modoUSA}; then
            echo "#=====================#"
            echo "|  Modo USA activado. |"
            echo "#=====================#"
            echo "$jugador_id" >>${compatriotas}
        fi

        #Reunimos informacion para atacar rivales
        posiciones_xy=""
        for ((x=0;x<${2};x++)); do
            for ((y=0;y<${2};y++)); do
                posiciones_xy+=" ${x},${y}"
            done
        done

        rivales_cant=`jshon -l <<<$1`
        rivales_vivos=`seq -s' ' 0 $((${rivales_cant}-1))`
        rivales_vivos=${rivales_vivos/${jugador_id}}

        #Arrays con los nombres y posiciones shuffleadas a atacar
        for((i=0;i < ${rivales_cant};i++)); do
            rivales_nombres[${i}]=`jshon -e${i} -eName -u <<<$1`
            rivales_pos[${i}]=" `shuf -e $posiciones_xy`"
        done

        #Iniciamos el ataque
        for ((i=3;i>0;i--)); do
            printf '\rIniciando holocausto mundial en '"${i}"' segundos...'
            sleep 1
        done
        printf '\rIniciando holocausto mundial en '"${i}"' segundos...\r\n\n'

        atacar
    fi

    return $?
}

function procesar_disparo(){
    #$1 = { "Name": "Accept", "Data": {"eta": "1786"}}

    #Hago la cuenta regresiva del eta
    for((eta=`jshon -e eta -u <<<$1`;eta>0;eta=eta-10)); do
        printf '\rImpacto en:\t'"$eta"' ms   '
        sleep 0.01 #el eta esta en ms
    done
    printf '\rImpacto en:\t 0 ms   '

    #Pido el resultado
    send_cmd "$get_update" "{\"s_id\":${jugador_id}}"

    return $?
}

function procesar_evento(){
    #$1 = {"t_id": 1,"s_id": 0,"coords": { "x":1, "y":6},"status": "I"}

    #Variables locales
    return_code=$ERROR_NO_ERROR

    #Recibi un evento, debo ver si es la respuesta de mi ataque
    if [ `jshon -e s_id <<<$1` -eq ${jugador_id} ]; then
        resultado=`jshon -e status -u <<<$1`
        case "${resultado}" in
        $hundido)
            printf '\t Resultado: ENHORABUENA, HUNDIDO!\r\n\n'
            atacar
            return_code=$?
            ;;
        $incoming)
            return_code=$ERROR_STATUS
            ;;
        $tocado)
            printf '\t Resultado: TOCADO POR D10S!\r\n\n'
            atacar
            return_code=$?
            ;;
        $agua)
            printf '\t Resultado: FRAC-AGUA\r\n\n'
            atacar
            return_code=$?
            ;;
        $shooting)
            return_code=$ERROR_STATUS
            ;;
        $eta_no_trans)
            #Espero un toque y pido el resultado
            printf '\r Aun no llego el misil, esperando...'
            sleep 1
            send_cmd "$get_update" "{\"s_id\":${jugador_id}}"
            return_code=$?
            ;;
        esac
    #else
        #No hago nada, pues soy el target (no soy el s_id)
    fi

    return $return_code
}

function procesar_decline(){
    #$1 = {"Msg": "No puedes disparar mas una vez hundidos todos tus barcos"}

    #Variables locales
    return_code=$ERROR_NO_ERROR

    #Rechazaron mi ataque, por que?
    case "`jshon -e Msg -u <<<$1`" in
    $msg_perdi)
        printf '\v\t¡¡¡ME HUNDIERON LOS VELEROS!!!\r\v'

        #Le aviso a los demas jugadores que perdi asi no me atacan mas
        rm -f ${fd_fifo_rivales_muertos}
        pids=""
        for rival in `ls ${working_dir}/fifo_rivales_muertos.* 2>/dev/null`; do
            echo "${jugador_id}" >${rival} &
            pids+=" $!"
        done
        [[ ! -z "$pids" ]] && wait $pids
        return_code=$ERROR_I_LOST
        ;;
    $msg_denegado)
        printf '\rAtaque denegado. La region ya a sido bombardeada.\r\n\n'
        atacar
        return_code=$?
        ;;
    esac

    return $return_code
}

function procesar_error(){
    #$1 = {"Msg": "Posiciones Invalidas"}"

    #Variables locales
    return_code=$ERROR_NO_ERROR

    #Comenzamos
    case "`jshon -e Msg -u <<<$1`" in
    $msg_pos_inv)
        echo "Elegi alguna posicion incorrecta, trato de vuelta."
        cargar_botes $boats_uno $boats_dos $boats_tres $boats_cuatro $boats_cinco
        return_code=$?
        ;;
    $msg_jugando)
        echo "El holocausto ya comenzo. Llegaste tarde, ya se vendieron todas las entradas"
        return_code=$ERROR_ALREADY_FIGHTING
        ;;
    *)
        echo "Error desconocido: $data" >&2
        return_code=$ERROR_ERROR_NO_KNOWN
        ;;
    esac

    return $return_code
}

function atender(){
    return_code=$ERROR_NO_ERROR
    if [ ! -z "$1" ]; then
        #Si el server no nos devolvio fruta
            cmd=`jshon -eName -u <<<$1`
            data=`jshon -eData <<<$1`
    fi

    case "$cmd" in
    $subscribe)
        #data = {"Name":"Subscribe","Data":{"Boardsize":12,"Boatssize":1,"Id":0}}
        subscribe "$data"
        return_code=$?
        ;;
    $setup)
        #data = {"Name":"Setup","Data":"Ok"}
        echo "Posiciones Validas, esperando rivales"
        #Tiramos nops en background constantemente
        while true; do
            send_cmd "$nop" "\"\""
            sleep 0.5
        done &
        ;;
    $start_resp)
        #data = [{"Id":0, "Name": "jugador_0"},{"Id":1, "Name": "jugador_1"}]
        start_batalla "$data" "$boardsize"
        return_code=$?
        ;;
    $accept_resp)
        #data = { "Name": "Accept", "Data": {"eta": "1786"}}
        procesar_disparo "$data"
        return_code=$?
        ;;
    $event_resp)
        #data = {"t_id": 1,"s_id": 0,"coords": { "x":1, "y":6},"status": "I"}
        procesar_evento "$data"
        return_code=$?
        ;;
    $decline_resp)
        #data = {"Msg": "No puedes disparar mas una vez hundidos todos tus barcos"}
        procesar_decline "$data"
        return_code=$?
        ;;
    $error_resp)
        #data = {"Msg": "Posiciones Invalidas""
        procesar_error "$data"
        return_code=$?
        ;;
    #"")
    #    echo "Me mandaron fruta" >&2
    #    #atacar
    #    return_code=$ERROR_CMD_NO_VALIDO
    #    ;;
    *)
        echo "Comando desconocido: $cmd" >&2
        return_code=$ERROR_CMD_NO_VALIDO
        ;;
    esac

    return $return_code;
}

#######################################################
#MAIN

#Atrapamos el ctrl+c
trap "cleanup $ERROR_CTRL_SIG" INT TERM QUIT

#Limpiamos el log viejo si existe
[[ -f ${comlog} ]] && rm -f ${comlog} 2>/dev/null

#Asumimos que el server esta corriendo y nos conectamos
#con el netcat de manera tal de poder mandar comandos 
#mediante las redirecciones a los fds
exec {fd_output_server}<>${fd_fifo_server} #uso el fd_output_server para leer del server
exec {fd_input_server}> >(netcat $host $port >&${fd_output_server} 2>/dev/null) #>>${comlog}) #uso el fd_input_server para escribir al server

#Nos inscribimos
send_cmd "$subscribe" "{\"Name\":\"${jugador_nombre}\"}"

#Actuamos segun las respuestas del server
xargs --no-run-if-empty --null -E '\200' -Iresp echo resp <&${fd_output_server} 2>/dev/null | \
    while read -d '|' resp; do 
        resp=$(tr -c -d [:print:] <<<$resp)
        #echo "Recibido: ${resp}" >>${comlog}
        atender "$resp"
        return_code=$?
        [[ $return_code -ne $ERROR_NO_ERROR ]] && pkill -P $$ "xargs"
    done

#Limpiamos
cleanup "$return_code"

#Nos vamos
exit $return_code
