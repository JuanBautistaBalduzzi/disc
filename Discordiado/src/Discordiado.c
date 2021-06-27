/*
 ============================================================================
 Name        : Discord.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <TAD_TRIPULANTE.h>
#include <TAD_PATOTA.h>
#include <commons/config.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <semaphore.h>
#include <serializacion.h>
#include <conexion.h>
t_config* config;
sem_t hilosEnEjecucion;
sem_t multiProcesamiento;
pthread_t firstInit;
pthread_mutex_t mutexIO;
pthread_mutex_t listos;
pthread_mutex_t ejecutando;
pthread_mutex_t bloqueadosIo;
pthread_mutex_t finalizados;
pthread_mutex_t socketMongo;
pthread_mutex_t socketMiRam;
pthread_mutex_t trip_comparar;
sem_t pararPlanificacion;
sem_t pararIo;
t_list* listaPatotas;
t_queue* ready;
t_list* execute;
t_list* finalizado;
t_queue* bloqueados;
t_list* bloqueados_sabotaje;
Tripulante* esta_haciendo_IO;
Tripulante* trip_cmp;
char* ipMiRam;
char* puertoMiRam;
char* ipMongoStore;
char* puertoMongoStore;
char* algoritmo;
int a = 0;
int TripulantesTotales = 0;
int multiProcesos;
int quantum;
int retardoCpu;
_Bool primerInicio=true;
uint8_t t_totales=0;
uint8_t p_totales=0;
_Bool correr_programa=true;

/*AVISOS PARROQUIALES

 * ESPERO SEAN DE TU AGRADO MIS COMENTARIOS
 * VAMOS QUE YA LO TENEMOS EL MODULO
 * FALTARIAN EL MANEJO DE STRINGS POR ESO HAY VARIABLES NO INICIALIZADAS
 * Y BUENO DESPUES EL ROUND ROBIN VIENDO EL VIDEO TENGO QUE CAMBIAR UNA COSA POR EL IO
 * Y BUENO SOLO FALTARIA VER EL TEMA DE LOS SABOTAJES DESPUES
 * ESPERO SEAN UTILES MIS COMENTARIOS
 */
//esta funcion es porque no puedo aplicar parcialmente una funcion en c ):

_Bool cmpTripulantes(Tripulante* uno, Tripulante* dos)
{
	return uno->id<dos->id;
}
void* identidad(void* t)
{
	return t;
}
//esta para saber si es el mismo tripulante
bool esElMismoTripulante(Tripulante* ra)
{
	return (trip_cmp->id == ra->id);
}

int calcular_distancia(Tripulante* tripulante, int x, int y)
{
	int retornar=((x-tripulante->posicionX)^2)+((y-tripulante->posicionY)^2);

	return retornar;
}

bool es_tarea_IO(char* tarea)
{
	bool ret= string_contains(tarea, "GENERAR");
	ret=ret||string_contains(tarea,"CONSUMIR");
	return ret;
}


int conectarse_mongo()
{
	pthread_mutex_lock(&socketMongo);
	int socket=crear_conexion(ipMongoStore,puertoMongoStore);
	pthread_mutex_unlock(&socketMongo);
	while(socket==(-1))
	{
		puts("RECONECTANDO CON MONGO_STORE");
		socket=crear_conexion(ipMiRam,puertoMiRam);
	}

	return socket;
}
int conectarse_Mi_Ram()
{
	pthread_mutex_lock(&socketMiRam);
	int socket=crear_conexion(ipMiRam,puertoMiRam);
	pthread_mutex_unlock(&socketMiRam);

		while(socket==(-1))
		{
			puts("RECONECTANDO CON MI_RAM");
			socket=crear_conexion(ipMiRam,puertoMiRam);
		}

	return socket;
}

int string_to_int(char* palabra)
{
	int ret;
	if(strlen(palabra)==2)
	{
		 ret= (palabra[0]-'0')*10+palabra[1]-'0';
	}
	else
	{
		ret=palabra[0]-'0';
	}
	return ret;
}
void obtener_parametros_patota(char* consol, int* ctidad,char* ptareas, t_list* poci)
{
	char** get_parametros= string_split(consol,(char*)" ");
	char** get_posicion=string_split(get_parametros[3], (char*) "|");
	for(int auxi=0; get_posicion[auxi]!=NULL; auxi++)
	{
		list_add(poci,(void*)string_to_int(get_posicion[auxi]));
	}
}
char* obtener_tareasPat(char*consol)
{
	char** get_parametros= string_split(consol,(char*)" ");
	return (char*)get_parametros[2];
}
int obtener_cantidadTrip(char*consol)
{
	char** get_parametros= string_split(consol,(char*)" ");
	return (int)string_to_int(get_parametros[1]);
}
void obtener_parametros_tarea(Tripulante* t, int posX, int posY)
{
	if(es_tarea_IO(t->Tarea))
	{
		char** list = string_split(t->Tarea,(char*)" ");
		if(list[0]==t->Tarea)
		{
			char** parametros=string_split(t->Tarea,(char*)';');
			posX=string_to_int(parametros[1]);
			posY=string_to_int(parametros[2]);
			t->espera=string_to_int(parametros[3]);
		}
		else
		{
			char** parametros=string_split(list[1],(char*)";");
			posX=string_to_int(parametros[2]);
			posY=string_to_int(parametros[3]);
			t->espera=string_to_int(parametros[4]);

		}

	}
	else
	{
		char** parametros=string_split(t->Tarea,(char*)";");
		posX=(int)string_to_int(parametros[1]);
		posY=(int)string_to_int(parametros[2]);
		t->espera=string_to_int(parametros[3]);
	}
}

void elimiarTripulante(Tripulante* tripulante)
{
	int socketMiRam=conectarse_Mi_Ram();
	serializar_eliminar_tripulante(tripulante->id,socketMiRam);
	liberar_conexion(socketMiRam);
	int socketMongo = conectarse_mongo();
	serializar_eliminar_tripulante(tripulante->id,socketMongo);
	liberar_conexion(socketMongo);
	pthread_exit(tripulante->hilo);
}
void* iniciar_Planificacion()
{
	pthread_detach(firstInit);
	while (correr_programa){
		sem_wait(&pararPlanificacion);
		sem_wait(&multiProcesamiento);
		//ESTE MUTEX ES PARA PROTEGER LA COLA DE READY
		pthread_mutex_lock(&listos);
		//este para proteger la lista de ejecutados
		pthread_mutex_lock(&ejecutando);
		//AGREGO A LISTA DE EJECUCION
		Tripulante* tripulante= (Tripulante*)queue_pop(ready);
		tripulante->estado = "Execute";
		int socketM=conectarse_Mi_Ram();
		serializar_cambio_estado(tripulante, socketM);
		liberar_conexion(socketM);
		list_add(execute, queue_pop(ready));
		pthread_mutex_unlock(&listos);
		pthread_mutex_unlock(&ejecutando);
		sem_post(&pararPlanificacion);

	}
}

void ejecutando_a_bloqueado(Tripulante* trp )
{
	pthread_mutex_lock(&bloqueadosIo);
		pthread_mutex_lock(&ejecutando);
		//ACA PUSHEO AL TRIPULANTE A LA COLA DE BLOQUEADOS IO
		pthread_mutex_lock(&trip_comparar);
		trip_cmp=trp;
		queue_push(bloqueados,list_remove_by_condition(execute,esElMismoTripulante));
		pthread_mutex_unlock(&trip_comparar);
		pthread_mutex_unlock(&bloqueadosIo);
		pthread_mutex_unlock(&ejecutando);
		trp->estado = "Bloqueado IO";
		int socketMR=conectarse_Mi_Ram();
		serializar_cambio_estado(trp,socketMR);
		liberar_conexion(socketMR);
}

void bloqueado_a_ready(Tripulante* bloq)
{
	pthread_mutex_lock(&listos);
	queue_push(ready,bloq);
	pthread_mutex_unlock(&listos);
	bloq->estado = "Ready";
	int socketMR=conectarse_Mi_Ram();
	serializar_cambio_estado(bloq,socketMR);
	liberar_conexion(socketMR);


}
// esta va a avanzar el tripulante paso a paso Y Enviar a miram
void* moverTripulante(Tripulante* tripu, int tarea_x,int tarea_y)
{
	int* ret = 0;

	if (tarea_x > tripu->posicionX) {
		tripu->posicionX++;
		int socket=conectarse_Mi_Ram();
		serializar_id_and_pos(tripu, socket);
		liberar_conexion(socket);

		return ret;

	}
	if (tarea_x < tripu->posicionX) {
		tripu->posicionX--;
		int socket=conectarse_Mi_Ram();
		serializar_id_and_pos(tripu, socket);
		liberar_conexion(socket);


		return ret;
	}
	if (tarea_y < tripu->posicionY) {
		tripu->posicionX--;
		int socket=conectarse_Mi_Ram();
		serializar_id_and_pos(tripu, socket);
		liberar_conexion(socket);
		return ret;
	}
	if (tarea_y > tripu->posicionY) {
		tripu->posicionX++;
		int socket=conectarse_Mi_Ram();
		serializar_id_and_pos(tripu, socket);
		liberar_conexion(socket);
		return ret;
	}
	return NULL;
}



void enviarMongoStore(Tripulante* enviar) {

	//envia tarea al MONGO STORE
	esta_haciendo_IO=enviar;
	int socketMongo= conectarse_mongo();
	serializar_tarea(enviar->Tarea, socketMongo);
	liberar_conexion(socketMongo);
	while(enviar->espera!=0)
	{
		//semaforo para parar ejecucion
		sem_wait(&pararIo);
		sleep(retardoCpu);
		enviar->espera--;
		sem_post(&pararIo);
	}
	//lo paso a cola ready
	bloqueado_a_ready(enviar);


}
void hacerTareaIO(Tripulante* io) {
	//ACA ME PASE UN POQUITO CON LOS SEMAFOROS REVISARRRR
	ejecutando_a_bloqueado(io);
	//libero el recurso de multiprocesamiento porque me voy a io
	sem_post(&multiProcesamiento);
	pthread_mutex_lock(&mutexIO);
	pthread_mutex_lock(&bloqueadosIo);
	//LO ENVIOPARA QUE HAGA SUS COSAS CON MONGOSTORE
	enviarMongoStore((void*) queue_pop(bloqueados));
	pthread_mutex_unlock(&bloqueadosIo);
	pthread_mutex_unlock(&mutexIO);

}
void hacerFifo(Tripulante* tripu) {
	//tu tarea es es transformar la taera de sstring en dos int posicion y un int de espera
	// mover al 15|65 20
	int tarea_x;
	int tarea_y;
	obtener_parametros_tarea(tripu,&tarea_x,&tarea_y);
	while (tripu->posicionX != tarea_x || tripu->posicionY != tarea_y) {
		//este es el semaforo para pausar laejecucion
		sem_wait(&hilosEnEjecucion);
		sleep(retardoCpu);
		moverTripulante(tripu, tarea_x, tarea_y);

		//le tiroun post al semaforo que me permite frenar la ejecucion
		sem_post(&hilosEnEjecucion);

	}

	if (es_tarea_IO(tripu->Tarea))
	{
		hacerTareaIO(tripu);
	}
	else {
		//una vez que llego donde tenia que llegar espera lo que tenia que esperar
		while(tripu->espera !=0)
		{
			sem_wait(&hilosEnEjecucion);
			sleep(retardoCpu);
			sem_post(&hilosEnEjecucion);
			tripu->espera --;
		}

		tripu->Tarea = "";
	}

}
void hacerRoundRobin(Tripulante* tripulant) {
	int tarea_x;
	int tarea_y;
	int contadorQuantum = 0;
	obtener_parametros_tarea(tripulant, &tarea_x,&tarea_y);

	while (contadorQuantum < quantum)
	{
		if (tripulant->posicionX == tarea_x && tripulant->posicionY == tarea_y)
		{
			break;
		}
		//este es el semaforo para pausar laejecucion
		sem_wait(&hilosEnEjecucion);
		sleep(retardoCpu);
		moverTripulante(tripulant, tarea_x, tarea_y);
		contadorQuantum++;
		sem_post(&hilosEnEjecucion);
		//le tiroun post al semaforo que me permite frenar la ejecucion

	}
	if(contadorQuantum<quantum && es_tarea_IO(tripulant->Tarea))
	{
		hacerTareaIO(tripulant);
	}
	while (contadorQuantum < quantum) {
		sem_wait(&hilosEnEjecucion);
		sleep(retardoCpu);
		tripulant->espera--;
		contadorQuantum++;
		sem_post(&hilosEnEjecucion);
	}


	if (tripulant->posicionX == tarea_x && tripulant->posicionY == tarea_y && tripulant->espera==0)
	{
		tripulant->Tarea = "";
	}else
	{
		//protejo las colas o listas
		pthread_mutex_lock(&listos);
		pthread_mutex_lock(&ejecutando);
			//termino su quantum lo agrego a ready
		pthread_mutex_lock(&trip_comparar);
		trip_cmp=tripulant;
		queue_push(ready,list_remove_by_condition(execute,esElMismoTripulante));
		pthread_mutex_unlock(&trip_comparar);
		pthread_mutex_unlock(&listos);
		pthread_mutex_unlock(&ejecutando);
		tripulant->estado = "Ready";
	//le aviso al semaforo que libere un recurso para que mande otro tripulante
	sem_post(&multiProcesamiento);
	}
}

//CLASIFICA LA TAREA DEL TRIPULANTE
void hacerTarea(Tripulante* trip)
{
	if (string_contains(algoritmo,"FIFO"))
	{
		hacerFifo(trip);
	} else {
		hacerRoundRobin(trip);
	}

}
// LARGA VIDA TRIPULANTE ESPEREMOS CADA TRIPULANTE VIVA UNA VIDA FELIZ Y PLENA
void* vivirTripulante(Tripulante* tripulante)
{
	int socketMr;
	int socketMongo;
	while (tripulante->vida)
	{
		//BUENO ACA ESTA LA MAGIC
		//SI PLANIFICAR LO AGREGO A LA LISTA DE EJECUTAR VA A HACER UNA TAREA

		while (string_contains(tripulante->estado,"Execute"))
		{
			if (!string_contains(tripulante->estado,"Ready"))
			{
				//LE CAMBIO EL ESTADO A EJECUTANDO MUITO IMORTANTE

			}
			sem_wait(&hilosEnEjecucion);

		if (string_is_empty(tripulante->Tarea))
		{
				//ACA LE PIDE LA TAREA
				socketMr=conectarse_Mi_Ram();
				serializar_tarea_tripulante(tripulante, socketMr);

				//AHORA RECIBIMOS TAREA
				t_paquete* paquete = malloc(sizeof(t_paquete));
				paquete->buffer = malloc(sizeof(t_buffer));

				// Primero recibimos el codigo de operacion
				recv(socketMr, &(paquete->codigo_operacion), sizeof(uint8_t),
						0);

				// Después ya podemos recibir el buffer. Primero su tamaño seguido del contenido
				recv(socketMr, &(paquete->buffer->size), sizeof(uint32_t),
						0);
				paquete->buffer->stream = malloc(paquete->buffer->size);
				if (paquete->codigo_operacion == 5)
				{
					//ACA LE AGREGO LA TAREA AL TRIPULANTE CARITA FACHERA FACHERITA (:

					char* recibido = deserializar_tarea(paquete->buffer);

					tripulante->Tarea = recibido;
					free(recibido);

				}
				liberar_conexion(socketMr);
				// Liberamos memoria
				free(paquete->buffer->stream);
				free(paquete->buffer);
				free(paquete);
			}
			sem_post(&hilosEnEjecucion);
			//ES HORA DE QUE EL VAGO DEL TRIPULANTE SE PONGA A LABURAR
			if(tripulante->Tarea!=NULL)
			{
			hacerTarea(tripulante);
			}
			else
			{
				pthread_mutex_lock(&ejecutando);
				pthread_mutex_lock(&finalizados);
				list_add(finalizado,list_remove_by_condition(execute,(void*)esElMismoTripulante));
				pthread_mutex_unlock(&ejecutando);
				pthread_mutex_unlock(&finalizados);
				tripulante->vida=false;
				sem_post(&multiProcesamiento);

			}
		}

	}

	elimiarTripulante(tripulante);

	//ESTE RETURN NULL ES PARA CASTEARLA EN EL CREATE UNA PEQUEÑA BOLUDEZ
	return NULL;

}
void* atender_sabotaje(char* instruccion_sabotaje)
{
	//separo los parametros del sabotaje
	int posx;
	int posy;
	Tripulante* mas_cerca;
	//busco el tripulante mas cerca
	pthread_mutex_lock(&ejecutando);
	for(int i=0;i<list_size(execute);i++)
	{
		Tripulante* iterado=(Tripulante*)list_get(execute,i);

		iterado->estado="Bloqueado Sabotaje";
		if(i==0)
		{
			Tripulante* mas_cerca=iterado;
		}
		if(calcular_distancia(iterado, posx, posy)<calcular_distancia(mas_cerca, posx, posy))
		{
			mas_cerca=iterado;
		}


	}
	pthread_mutex_unlock(&ejecutando);
	int in=0;
			Tripulante * auxiliar;
			pthread_mutex_lock(&listos);
			while(in<queue_size(ready))
			{
				Tripulante* trip_agregar=queue_pop(ready);
				trip_agregar->estado="Bloqueado Sabotaje";
				if(in==0)
				{
					auxiliar= trip_agregar;
				}
				if(calcular_distancia(trip_agregar,posx,posy)<calcular_distancia(auxiliar,posx,posy))
				{
					auxiliar=trip_agregar;
				}
				queue_push(ready,trip_agregar);
				in++;
			}
			pthread_mutex_unlock(&listos);
			in=0;
			pthread_mutex_lock(&bloqueadosIo);
			while(in<queue_size(bloqueados))
				{
				   Tripulante* trip_agregar=queue_pop(bloqueados);
				   trip_agregar->estado="Bloqueado Sabotaje";
					queue_push(bloqueados,trip_agregar);
					in++;
				}
			pthread_mutex_unlock(&bloqueadosIo);
			esta_haciendo_IO->estado="Bloqueado Sabotaje";
		if(calcular_distancia(mas_cerca, posx, posy)<calcular_distancia(auxiliar,posx,posy))
		{
			while(mas_cerca->posicionX!=posx||mas_cerca->posicionY!=posy)
			sleep(retardoCpu);
			moverTripulante(mas_cerca,posx,posy);
		}
		else
		{
			while(auxiliar->posicionX!=posx||auxiliar->posicionY!=posy)
			sleep(retardoCpu);
			moverTripulante(auxiliar,posx,posy);
		}
		pthread_mutex_lock(&ejecutando);
			for(int i=0;i<list_size(execute);i++)
			{
				Tripulante* iterado=(Tripulante*)list_get(execute,i);

				iterado->estado="Ejecutando";
			}
		pthread_mutex_unlock(&ejecutando);

		pthread_mutex_unlock(&ejecutando);
		 in=0;
				pthread_mutex_lock(&listos);
				while(in<queue_size(ready))
				{
					Tripulante* trip_agregar=queue_pop(ready);
					trip_agregar->estado="Ready";
					queue_push(ready,trip_agregar);
					in++;
				}
				pthread_mutex_unlock(&listos);


				in=0;
				pthread_mutex_lock(&bloqueadosIo);
				while(in<queue_size(bloqueados))
					{
					   Tripulante* trip_agregar=queue_pop(bloqueados);
					   trip_agregar->estado="Bloqueado IO";
						queue_push(bloqueados,trip_agregar);
						in++;
					}
				pthread_mutex_unlock(&bloqueadosIo);
				esta_haciendo_IO->estado="Bloqueado IO";


	return NULL;
}

int hacerConsola() {
	//SIEMPRE HAY QUE SER CORTEZ Y SALUDAR
	puts("Bienvenido a A-MongOS de Cebollita Subcampeon \n");
	char* linea=string_new();
	while (1) {
//leo los comandos
		linea =readline(">");
		if (string_contains(linea, "INICIAR_PATOTA")) {
			//PD ESTE MANEJO DE STRINS TE TOCA A VOS PILAR
			char* tarea=obtener_tareasPat(linea);
			int cantidad=obtener_cantidadTrip(linea);
			t_list* list_posicion=list_create();
			obtener_parametros_patota(linea,cantidad,tarea,list_posicion);
			//AHORA SI PA INICIAMOS LA PATOTA
			puts("anda piola hasta aca\n");
			Patota* pato = iniciarPatota((uint8_t)cantidad,p_totales, list_posicion,tarea,t_totales);
			puts("hola");
			p_totales++;
			t_totales +=cantidad;
			puts("llega hasta aca");
			iniciar_patota* enviar=malloc(sizeof(iniciar_patota));
			enviar->idPatota = pato->id;
			puts("anda piola hasta aca");
			enviar->Tareas = fopen(tarea,"w+r");
			enviar->cantTripulantes=cantidad;
			puts("logra generar la patota enviar");
			int socketMiram=conectarse_Mi_Ram();
			puts("genera socket?");
			serializar_iniciar_patota(enviar,socketMiram );
			liberar_conexion(socketMiram);
			//free(enviar);
			puts("mando bien");
			//LE PASO A MI RAM CADA TRIPULANTE PARA QUE MUESTRE EN PANTALLA
			for (int i = 0; pato->tripulacion[i] != NULL; i++) {
				//SERIALIZO Y EN VIO
				int socket=conectarse_Mi_Ram();
				serializar_tripulante(pato->tripulacion[i], socket);
				liberar_conexion(socket);
				puts("se mano tripulante");
				//UNA VEZ QUE SE CARGA EN MEMORIA PASA A READY
				pato->tripulacion[i]->estado = "READY";
				//LO AGREGO A LA COLA
				queue_push(ready, pato->tripulacion[i]);
				//CORRO EL HILO DEL TRIPULANTE
				pthread_create(&(pato->tripulacion[i]->hilo), NULL,(void*)vivirTripulante, (void*)pato->tripulacion[i]);

			}
			//LIBERO MEMORIA BORRANDO EL ARRAY FEO DE TRIPULANTES
			//free(pato->tripulacion);
			free(pato->tareas);
			free(pato);

		}
		if (string_contains(linea,"INICIAR_PLANIFICACION"))
		{
			puts("jeje entro");
			if(primerInicio)
			{
				//pthread_detach(firstInit);
				pthread_create(&firstInit,NULL,(void*) iniciar_Planificacion,NULL);

				primerInicio=false;
			}
			puts("jejepaso");
			a = 0;
			//DEFINO UN SEM CONTADOR QUE NOS VA A SERVIR PARA PAUSAR LA PLANIFICACION DONDE QUERAMOS DESPUES
			while (a < multiProcesos) {
				sem_post(&hilosEnEjecucion);
				a++;
			}
			//ESTE SEM NOS VA A PERMITIR FRENAR LOS PROCESOS IO CUANDO NECESITEMOS
			sem_post(&pararIo);
			puts("viene muy bien");
			sem_post(&pararPlanificacion);
			puts("demasiado");
			//SEM CONTADOR QUE NOS PERMITE PONER EN EJECECUCION SEGUN CUANTO MULTIPROCESAMIENTO TENGAMOS
		/*	while (correr_programa)
			{
				sem_wait(&pararPlanificacion);
				sem_wait(&multiProcesamiento);
				//ESTE MUTEX ES PARA PROTEGER LA COLA DE READY
				pthread_mutex_lock(&listos);
				//este para proteger la lista de ejecutados
				pthread_mutex_lock(&ejecutando);
				//AGREGO A LISTA DE EJECUCION
				Tripulante* tripulante= (Tripulante*)queue_pop(ready);
				tripulante->estado = "Execute";
				int socketM=conectarse_Mi_Ram();
				serializar_cambio_estado(tripulante, socketM);
				liberar_conexion(socketM);
				list_add(execute, queue_pop(ready));
				pthread_mutex_unlock(&listos);
				pthread_mutex_unlock(&ejecutando);
				sem_post(&pararPlanificacion);

			}
			*/

		}
		if (string_contains(linea,"LISTAR_TRIPULANTES") )
		{

			t_list* get_all_tripulantes=list_create();
			int in=0;
			pthread_mutex_lock(&finalizados);
			while(in<list_size(finalizado))
			{
				list_add(get_all_tripulantes,list_get(finalizado,in));
				in++;
			}
			pthread_mutex_unlock(&finalizados);
			in=0;
			pthread_mutex_lock(&ejecutando);
			while(in<list_size(execute))
			{
				list_add(get_all_tripulantes,list_get(execute,in));
				in++;
			}
			pthread_mutex_unlock(&ejecutando);

			in=0;
			pthread_mutex_lock(&listos);
			while(in<queue_size(ready))
			{
				Tripulante* trip_agregar=queue_pop(ready);
				list_add(get_all_tripulantes,trip_agregar);
				queue_push(ready,trip_agregar);
				in++;
			}
			pthread_mutex_unlock(&listos);

			in=0;
			pthread_mutex_lock(&bloqueadosIo);
			while(in<queue_size(bloqueados))
			{
				Tripulante* trip_agregar=queue_pop(bloqueados);
				list_add(get_all_tripulantes,trip_agregar);
				queue_push(bloqueados,trip_agregar);
				in++;
			}
			pthread_mutex_unlock(&bloqueadosIo);
			list_add(get_all_tripulantes,esta_haciendo_IO);
			list_sort(get_all_tripulantes, (void*)cmpTripulantes);

			while(!list_is_empty(get_all_tripulantes))
			{
			Tripulante* trip_mostrar=(Tripulante*)list_remove(get_all_tripulantes,NULL);
			mostrarTripulante(trip_mostrar);
			}
			list_destroy(get_all_tripulantes);
			puts("TODO PIOLA HASTA ACA AMEO");
		}
		if (string_contains(linea,"PAUSAR_PLANIFICACION")) {
			///ver como lo hacemos
			// YA LO HICE LOL BASUCAMENTE LES TIRAS UN WAIT HASTA QUE LLEGUEN A 0 PARA QUE NO PUEDAN EJECUTAR
			a = 0;
			while (a < multiProcesos) {
				sem_wait(&hilosEnEjecucion);
				a++;
			}
			sem_wait(&pararPlanificacion);
			sem_wait(&pararIo);
			puts("Entra piolax ameo");
		}
		if (string_contains(linea, "EXPULSAR_TRIPULANTE"))
		{
			//BUENO ACA UN PEQUEÑO INTENTO DE TU TAREA DE MANEJO DE STRINGS PILI
			// FIJATE QUE SOLO SIRVE SI ES DE UN DIGIITO VAS A TENER QUE DIVIDIR EL ESTRING EN EL ESPACIO
			// Y FIJARTE SI EL SUBSTRING TIENE 1 O 2 CARACTERES
			char** obtener_id_trip=string_split(linea, (char*)' ');
			int id_trip_expulsar=string_to_int(obtener_id_trip[1]);
			//envio a MIRAM QUE BORREEL TRIPULANTE DEL MAPA
			int socketito= conectarse_Mi_Ram();
			serializar_eliminar_tripulante(id_trip_expulsar,socketito);
			liberar_conexion(socketito);

		}

	}

}

int main() {
	config = config_create("/home/utnso/workspace/Discordiador/TP.config");
	ipMiRam = config_get_string_value(config, "IP_MI_RAM_HQ");
	puertoMiRam = config_get_string_value(config, "PUERTO_MI_RAM_HQ");
	multiProcesos = config_get_int_value(config, "GRADO_MULTITAREA");
	retardoCpu = config_get_int_value(config, "RETARDO_CICLO_CPU");
	algoritmo = config_get_string_value(config, "ALGORITMO");
	puertoMongoStore = config_get_string_value(config, "PUERTO_I_MONGO_STORE");
	ipMongoStore = config_get_string_value(config, "IP_I_MONGO_STORE");
	quantum = config_get_int_value(config, "QUANTUM");
	t_list* execute=list_create();
	//Patota* pat= iniciarPatota(1,1,execute,"TAREAD:TXT",t_totales);
	//INICIALIAZAMOS LOS SEMAFOROS
	sem_init(&multiProcesamiento, 0, multiProcesos);
	sem_init(&hilosEnEjecucion, 0, multiProcesos);
	sem_init(&pararIo, 0, 1);
	sem_init(&pararPlanificacion,0,0);
	pthread_mutex_init(&listos, NULL);
	pthread_mutex_init(&ejecutando, NULL);
	pthread_mutex_init(&bloqueadosIo, NULL);
	pthread_mutex_init(&socketMiRam, NULL);
	pthread_mutex_init(&socketMongo, NULL);

	//INICIALIZAMOS LAS PILAS Y COLAS RARAS QUE CREAMOS
	t_list* listaPatotas=list_create();
	t_queue* ready=queue_create();

	t_list* finalizado=list_create;
	t_queue* bloqueados=queue_create;
	t_list* bloqueados_sabotaje=list_create();

	printf("hola mundo\n");
	pthread_t consola;
	pthread_create(&consola, NULL, (void *) hacerConsola(), NULL);

	int socket_mongo_store_sabotaje=crear_conexion(ipMongoStore,puertoMongoStore);
	char* sabotaje;
	while(correr_programa)
	{
		t_paquete* paquete = malloc(sizeof(t_paquete));
		paquete->buffer = malloc(sizeof(t_buffer));

		// Primero recibimos el codigo de operacion
		recv(socket_mongo_store_sabotaje, &(paquete->codigo_operacion), sizeof(uint8_t), 0);

		// Después ya podemos recibir el buffer. Primero su tamaño seguido del contenido
		recv(socket_mongo_store_sabotaje, &(paquete->buffer->size), sizeof(uint32_t), 0);
		paquete->buffer->stream = malloc(paquete->buffer->size);
		recv(socket_mongo_store_sabotaje, paquete->buffer->stream, paquete->buffer->size, 0);

		// Ahora en función del código recibido procedemos a deserializar el resto

		if(paquete->codigo_operacion==10)
		{
		  sabotaje=deserializar_sabotaje(paquete->buffer);
		  int parar_todo_sabotaje = 0;
		  while (parar_todo_sabotaje< multiProcesos)
		  {
		  sem_wait(&hilosEnEjecucion);
		  parar_todo_sabotaje++;
		  }
		sem_wait(&pararIo);

		}

		// Liberamos memoria
		free(paquete->buffer->stream);
		free(paquete->buffer);
		free(paquete);
		pthread_t hilo_sabotaje;
		pthread_create(&hilo_sabotaje,NULL,(void*)atender_sabotaje,sabotaje);
	}



	printf("hola mundo\n");

	return 0;

	/* ADIO' wachin ! */
}
