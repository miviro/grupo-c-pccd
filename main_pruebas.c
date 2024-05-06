#define _XOPEN_SOURCE 700 // para que vscode no se queje de sa
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>

#define NUMERO_NODOS 10
#define PROCESOS_POR_NODO 100
#define NUMERO_ADELANTAMIENTOS 20

#define NUMERO_PROCESOS (NUMERO_NODOS * PROCESOS_POR_NODO)
#define NUMERO_PRIORIDADES 3

#define CONSULTA 0
#define RESERVA 1
#define PAGO 2
#define ADMINISTRACION 3
#define CANCELACIONES 4

#define NUM_ROLES_PRINCIPALES 5
#define MAX_CHARS_POR_ROL_Y_NUM 3
#define STRING_ROLES "crapx"

#define KEY 1234567
#define REQUEST 0
#define REPLY 1

int msg_ids[NUMERO_NODOS];

int array_ids[NUMERO_NODOS] = {0};

struct msg
{
    long mtype;
    int tipo; // REQUEST o REPLY
    int ticket;
    int id_nodo;
    int rol;
};

// cada proceso
struct detalles_proceso
{
    int rol;
    int prioridad;
    int id_nodo; // Nodo al que pertenezco
};

// cada nodo
struct detalles_nodo
{
    int mi_ticket;
    int mi_id;
    int nodos_pendientes[NUMERO_PRIORIDADES];
    int id_nodos_pendientes[NUMERO_NODOS - 1][NUMERO_PRIORIDADES]; // menos uno por que no nos tenemos en cuenta
    int id_nodos[NUMERO_NODOS - 1];
    int esperando[NUMERO_PRIORIDADES];
    int adelantamientos;
    int dentro;
    int replies_recibidos; //[NUMERO_PRIORIDADES];
    int abortados[NUMERO_PRIORIDADES];
    int consultas_esperando;
    int corta;
    int salir;
};

struct Data
{
    int numConsultas;
    int numReservas;
    int numAdministracion;
    int numPago;
    int numCancelaciones;
};

int var = 0; // comprobador de seccion critica en mutex

struct Data *num_procesos[NUMERO_NODOS];
char roles[NUMERO_NODOS][NUM_ROLES_PRINCIPALES * MAX_CHARS_POR_ROL_Y_NUM + 1]; // +1 para el \0

sem_t sem_paso_prioridad[NUMERO_NODOS][NUMERO_PRIORIDADES];
sem_t sem_paso_consultas[NUMERO_NODOS];
sem_t sem_var_esperando[NUMERO_NODOS][NUMERO_PRIORIDADES];
sem_t sem_var_nodos_pendientes[NUMERO_NODOS][NUMERO_PRIORIDADES];
sem_t sem_var_id_nodos_pendientes[NUMERO_NODOS][NUMERO_PRIORIDADES];
sem_t sem_var_adelantamientos[NUMERO_NODOS][NUMERO_PRIORIDADES];
sem_t sem_var_replies_recibidos[NUMERO_NODOS]; //[NUMERO_PRIORIDADES];
sem_t sem_var_abortado[NUMERO_NODOS][NUMERO_PRIORIDADES];
sem_t sem_var_consultas_esperando[NUMERO_NODOS];
sem_t sem_var_dentro[NUMERO_NODOS];
sem_t sem_total_mensajes[NUMERO_NODOS];

void principal(struct detalles_proceso *);
void receptor(struct detalles_nodo *);
void parseRoles(char *input, struct Data *data);
int *removeInt(int arr[], int size, int target);
struct detalles_proceso detalles_procesos[NUMERO_PROCESOS];
struct detalles_nodo detalles_nodos[NUMERO_NODOS];

int main(int argc, char const *argv[])
{
    /* if (argc == 1) { // aleatoriamente
        srand(time(NULL));
        for (int i = 0; i < NUMERO_NODOS; i++) {
            for (int j = 0; j < NUM_ROLES_PRINCIPALES; j++) {
                int numProcesosRol = rand() % MAX_ROLES_IGUALES_POR_NODO;
                if (numProcesosRol < 10) {
                    sprintf(roles[i] + (j * MAX_CHARS_POR_ROL_Y_NUM), "0%d%c", numProcesosRol, STRING_ROLES[j]); // si no pone un \0 y jode todo
                } else {
                    sprintf(roles[i] + (j * MAX_CHARS_POR_ROL_Y_NUM), "%d%c", numProcesosRol, STRING_ROLES[j]);
                }
            }
            roles[i][NUM_ROLES_PRINCIPALES * MAX_CHARS_POR_ROL_Y_NUM] = '\0';
        }
    //} else if (argc == (NUMERO_NODOS + 1)) { // descrito cada nodo, comprobar que el numero de argumentos suma NUM_NODOS
    //    strcpy(roles, argv); // TODO: sdfsd
    } else {
        printf("Uso incorrecto.\n");
        printf("Ejemplo: %s <número de procesos>\n \n\tCrea %d con ese número de procesos, distribuidos aleatoriamente.", argv[0], NUMERO_NODOS);
        printf("Ejemplo: %s 1c2r3a4p5x \n\tCrea %d con: 1 Consulta, 2 Reservas, 3 Administración, 4 Pagos y 5 Anulaciones.\n", argv[0], NUMERO_NODOS);
        printf("Ejemplo: %s 1c2r3a4p5x 10c0r3a42p1x (...) 6c1r34a66p5x \n\tCrea %d con los tantos procesos en cada nodo como los especificados.\n", argv[0], NUMERO_NODOS);
        exit(EXIT_FAILURE);
    } */
    for (int i = 0; i < NUMERO_NODOS; i++)
    {
        array_ids[i] = i;
        msg_ids[i] = msgget(KEY + i, 0666 | IPC_CREAT);
        if (msg_ids[i] == -1)
        {
            perror("msgget failed");
            exit(EXIT_FAILURE);
        }
    }

    // creamos los detalles_proceso para que no se compartan entre "procesos" distintos
    int cuenta_aux = 0;
    char *roles_temp[NUMERO_NODOS] = {"10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x", "10c20r40a20p10x"};
    for (int j = 0; j < NUMERO_NODOS; j++)
    {
        num_procesos[j] = malloc(sizeof(struct Data));
        parseRoles(roles_temp[j], num_procesos[j]); // Le pasamos 20c20r20a20p20x sumando en total 100 procesos

        // CONSULTAS -----------------------------------------------------------------
        for (int i = cuenta_aux; i < (num_procesos[j]->numConsultas + cuenta_aux); i++)
        {
            detalles_procesos[i].id_nodo = j;
            detalles_procesos[i].rol = CONSULTA;
            detalles_procesos[i].prioridad = 2;
        }
        cuenta_aux += num_procesos[j]->numConsultas;

        // RESERVAS -----------------------------------------------------------------
        for (int i = cuenta_aux; i < (num_procesos[j]->numReservas + cuenta_aux); i++)
        {
            detalles_procesos[i].id_nodo = j;
            detalles_procesos[i].rol = RESERVA;
            detalles_procesos[i].prioridad = 2;
        }
        cuenta_aux += num_procesos[j]->numReservas;

        // ADMINISTRACION -----------------------------------------------------------------
        for (int i = cuenta_aux; i < (num_procesos[j]->numAdministracion + cuenta_aux); i++)
        {
            detalles_procesos[i].id_nodo = j;
            detalles_procesos[i].rol = ADMINISTRACION;
            detalles_procesos[i].prioridad = 1;
        }
        cuenta_aux += num_procesos[j]->numAdministracion;

        // PAGO -----------------------------------------------------------------
        for (int i = cuenta_aux; i < (num_procesos[j]->numPago + cuenta_aux); i++)
        {
            detalles_procesos[i].id_nodo = j;
            detalles_procesos[i].rol = PAGO;
            detalles_procesos[i].prioridad = 1;
        }
        cuenta_aux += num_procesos[j]->numPago;

        // CANCELACIONES -----------------------------------------------------------------
        for (int i = cuenta_aux; i < (num_procesos[j]->numCancelaciones + cuenta_aux); i++)
        {
            detalles_procesos[i].id_nodo = j;
            detalles_procesos[i].rol = CANCELACIONES;
            detalles_procesos[i].prioridad = 0;
        }
        cuenta_aux += num_procesos[j]->numCancelaciones;

        // inicializar el nodo en sí
        detalles_nodos[j].mi_id = j;
        detalles_nodos[j].mi_ticket = 0;
        detalles_nodos[j].adelantamientos = 0;
        detalles_nodos[j].dentro = 0;
        detalles_nodos[j].consultas_esperando = 0;
        detalles_nodos[j].salir = 0;
        for (int i = 0; i < NUMERO_PRIORIDADES; i++)
        {
            detalles_nodos[j].esperando[i] = 0;
            detalles_nodos[j].replies_recibidos = 0; // detalles_nodos[j].replies_recibidos[i] = 0;
            detalles_nodos[j].abortados[i] = 0;
        }

        // bucle que sobreescribe de alguna manera
        for (int i = 0; i < NUMERO_PRIORIDADES; i++)
        {
            for (int k = 0; k < NUMERO_NODOS; k++)
            {
                detalles_nodos[j].id_nodos_pendientes[k][i] = 0;
            }
            detalles_nodos[j].nodos_pendientes[i] = 0;
        }

        // este bucle debe estar por debajo de el de arriba, de alguna manera el de arriba sobreescribe detalles_nodos[j].id_nodos
        int bool_aux = 0;
        for (int i = 0; i < NUMERO_NODOS; i++)
        {
            if (i == detalles_nodos[j].mi_id)
            {
                bool_aux = 1;
                continue;
            }
            detalles_nodos[j].id_nodos[i - bool_aux] = i;
        }
    }

    // inicializar semaforos
    for (int i = 0; i < NUMERO_NODOS; i++)
    {
        for (int j = 0; j < NUMERO_PRIORIDADES; j++)
        {
            sem_init(&sem_paso_prioridad[i][j], 0, 0);
            sem_init(&sem_var_abortado[i][j], 0, 1);
            // sem_init(&sem_var_replies_recibidos[i][j], 0, 1);
            sem_init(&sem_var_esperando[i][j], 0, 1);
            sem_init(&sem_var_nodos_pendientes[i][j], 0, 1);
            sem_init(&sem_var_id_nodos_pendientes[i][j], 0, 1);
            sem_init(&sem_var_adelantamientos[i][j], 0, 1);
        }
        sem_init(&sem_var_replies_recibidos[i], 0, 1);
        sem_init(&sem_var_dentro[i], 0, 1);
        sem_init(&sem_paso_consultas[i], 0, 0);
        sem_init(&sem_var_consultas_esperando[i], 0, 1);
        sem_init(&sem_total_mensajes[i], 0, 1);
    }

    // crear los hilos
    pthread_t tid_principales[NUMERO_PROCESOS];
    for (int i = 0; i < NUMERO_PROCESOS; i++)
    {
        if (pthread_create(&(tid_principales[i]), NULL, (void *)principal, &(detalles_procesos[i])) != 0)
        {
            printf("Error al crear el hilo\n");
            exit(EXIT_FAILURE);
        }
    }
    pthread_t tid_receptores[NUMERO_NODOS];
    for (int i = 0; i < NUMERO_NODOS; i++)
    {
        if (pthread_create(&(tid_receptores[i]), NULL, (void *)receptor, &(detalles_nodos[i])) != 0)
        {
            printf("Error al crear el hilo\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < NUMERO_PROCESOS; i++)
    {
        pthread_join(tid_principales[i], NULL);
    }
    // for (int i = 0; i < NUMERO_NODOS; i++) {
    //     pthread_join(tid_receptores[i], NULL);
    // }

    return 0;
}

void principal(struct detalles_proceso *detalles_proceso)
{
    int total_mensajes = 0;
    // sus cosas
    clock_t inicio, inicio_sc, inicio_ciclo, fin, fin_sc, fin_ciclo;
    double tiempo_transcurrido, tiempo_transcurrido_sc, tiempo_ciclo_vida;

    inicio = clock();  //INICIO DE LA METRICA
    inicio_ciclo = clock();

    srand(time(NULL));

    if (detalles_proceso->rol == CONSULTA)
    {
        sem_wait(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
        detalles_nodos[detalles_proceso->id_nodo].consultas_esperando++;
        sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
    }

    sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
    detalles_nodos[detalles_proceso->id_nodo].esperando[detalles_proceso->prioridad]++;
    if (detalles_nodos[detalles_proceso->id_nodo].esperando[detalles_proceso->prioridad] == 1)
    {                                                                                         // soy el primero?
        sem_post(&sem_var_esperando[detalles_proceso->id_nodo][detalles_proceso->prioridad]); // liberar esperando
        struct msg m = {
            .mtype = 1,
            .tipo = REQUEST,
            .ticket = detalles_nodos[detalles_proceso->id_nodo].mi_ticket + 1,
            .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
            .rol = detalles_proceso->rol};
        for (int i = 0; i < NUMERO_NODOS - 1; i++)
        {
            msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo].id_nodos[i]], &m, sizeof(m), 0);
            sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
            total_mensajes++;
            sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
            // fprintf(stdout, "Enviado a %d\n", detalles_nodos[detalles_proceso->id_nodo].id_nodos[i]);
        }

        if (detalles_proceso->rol == CONSULTA)
        {
            sem_wait(&sem_paso_consultas[detalles_proceso->id_nodo]);

            sem_wait(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
            detalles_nodos[detalles_proceso->id_nodo].consultas_esperando--;
            if (detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0 && detalles_nodos[detalles_proceso->id_nodo].corta == 0)
            {
                sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
            }
            sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
        }
        else
        {
            sem_wait(&sem_paso_prioridad[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
        }

        sem_wait(&sem_var_dentro[detalles_proceso->id_nodo]);
        detalles_nodos[detalles_proceso->id_nodo].dentro = 1;
        sem_post(&sem_var_dentro[detalles_proceso->id_nodo]);

        fin = clock();  //FIN DE LA METRICA

        // SECCION CRITICA

        inicio_sc = clock();  //INICIO DE LA METRICA

        int copia = var++;
        printf("SC %d: entro %d de nodo %d\n", copia, detalles_proceso->rol, detalles_proceso->id_nodo);
        if (detalles_proceso->rol == CONSULTA)
        {
            sleep(0.5); // para que se aprecie que las consultas son concurrentes
        }

        fin_sc = clock();  //INICIO DE LA METRICA

        sem_wait(&sem_var_dentro[detalles_proceso->id_nodo]);
        detalles_nodos[detalles_proceso->id_nodo].dentro = 0;
        sem_post(&sem_var_dentro[detalles_proceso->id_nodo]);
    }
    else
    {
        sem_post(&sem_var_esperando[detalles_proceso->id_nodo][detalles_proceso->prioridad]); // liberar esperando
        if (detalles_proceso->rol == CONSULTA)
        {
            sem_wait(&sem_paso_consultas[detalles_proceso->id_nodo]);
            sem_wait(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
            detalles_nodos[detalles_proceso->id_nodo].consultas_esperando--;
            if (detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0 && detalles_nodos[detalles_proceso->id_nodo].corta == 0)
            {
                sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
            }
            sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
        }
        else
        {
            sem_wait(&sem_paso_prioridad[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
        }

        sem_wait(&sem_var_dentro[detalles_proceso->id_nodo]);
        detalles_nodos[detalles_proceso->id_nodo].dentro = 1;
        sem_post(&sem_var_dentro[detalles_proceso->id_nodo]);

        fin = clock();  //FIN DE LA METRICA

        // SECCION CRITICA

        inicio_sc = clock();  //INICIO DE LA METRICA

        int copia = var++;
        printf("SC %d: entro %d de nodo %d\n", copia, detalles_proceso->rol, detalles_proceso->id_nodo);
        if (detalles_proceso->rol == CONSULTA)
        {
            sleep(0.5); // para que se aprecie que las consultas son concurrentes
        }

        fin_sc = clock();  //INICIO DE LA METRICA

        sem_wait(&sem_var_dentro[detalles_proceso->id_nodo]);
        detalles_nodos[detalles_proceso->id_nodo].dentro = 0;
        sem_post(&sem_var_dentro[detalles_proceso->id_nodo]);
    }

    sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
    detalles_nodos[detalles_proceso->id_nodo].esperando[detalles_proceso->prioridad]--;
    if (detalles_nodos[detalles_proceso->id_nodo].esperando[detalles_proceso->prioridad] == 0)
    {                                                                                         // soy el ultimo?
        sem_post(&sem_var_esperando[detalles_proceso->id_nodo][detalles_proceso->prioridad]); // liberar esperando
        sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][0]);                           // prioridad 0 -> cancelaciones
        if (detalles_nodos[detalles_proceso->id_nodo].esperando[0] > 0)
        { // existen prioridades 0 en este nodo esperando?
            detalles_nodos[detalles_proceso->id_nodo].corta = 1;
            sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][0]); // darle paso a la primera de prioridad 0
            sem_post(&sem_var_esperando[detalles_proceso->id_nodo][0]);  // liberar esperando
        }
        else
        {                                                               // no existen de prioridad 0 en mi nodo
            sem_post(&sem_var_esperando[detalles_proceso->id_nodo][0]); // liberar esperando
            // bloquear variables id nodos pendientes y nodos pendientes
            sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
            sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);

            if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] > 0)
            { // existen prioridades 0 en otros nodos esperando?
                detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                struct msg m = {
                    .mtype = 1,
                    .tipo = REPLY,
                    .ticket = -1,
                    .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                    .rol = detalles_proceso->rol};
                /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0]-- > 0) {
                  //  msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                     //   .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0]][0]], &m, sizeof(m), 0);
                } */
                detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] = 0;
                for (int i; i < NUMERO_NODOS - 1; i++)
                {
                    msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                       .id_nodos[i]],
                           &m, sizeof(m), 0);
                    sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                    total_mensajes++;
                    sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                }

                sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);
            }
            else
            { // no existen prioridades 0 en otros nodos esperando
                sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);

                // comprobar prioridad 1 en este nodo y luego fuera
                sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][1]); // prioridad 1 -> cancelaciones
                if (detalles_nodos[detalles_proceso->id_nodo].esperando[1] > 0)
                { // existen prioridades 1 en este nodo esperando?
                    detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                    sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][1]); // darle paso a la primera de prioridad 1
                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][1]);  // liberar esperando
                }
                else
                {                                                               // no existen de prioridad 1 en mi nodo
                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][1]); // liberar esperando
                    // bloquear variables id nodos pendientes y nodos pendientes
                    sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                    sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);

                    if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] > 0)
                    { // existen prioridades 1 en otros nodos esperando? TODO: no pilla prioridades de los otros nodos
                        detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                        sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                        detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                        sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                        struct msg m = {
                            .mtype = 1,
                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                            .rol = detalles_proceso->rol};
                        /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1]-- > 0) {
                            //msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                              //  .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1]][1]], &m, sizeof(m), 0);
                        } */
                        detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] = 0;
                        for (int i; i < NUMERO_NODOS - 1; i++)
                        {
                            msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                               .id_nodos[i]],
                                   &m, sizeof(m), 0);
                            sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                            total_mensajes++;
                            sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                        }

                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);
                    }
                    else
                    {
                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);

                        // comprobar prioridad 2 en este nodo y luego fuera
                        sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][2]); // prioridad 2 ->
                        if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] > 0)
                        { // existen prioridades 2 en este nodo esperando?
                            detalles_nodos[detalles_proceso->id_nodo].corta = 0;
                            sem_wait(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                            if (detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0 && detalles_nodos[detalles_proceso->id_nodo].esperando[2] - detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0)
                            { // existen consulta y reservas
                                // si existen ambas, aleatorio
                                if (rand() % 2 == 0)
                                {
                                    sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
                                }
                                else
                                {
                                    sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][2]); // darle paso a la primera de prioridad 2
                                }
                            }
                            else if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] - detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0)
                            {                                                                // si solo existen reservas
                                sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][2]); // darle paso a la primera de prioridad 2
                            }
                            else
                            { // si solo existen consultas
                                sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
                            }
                            sem_post(&sem_var_esperando[detalles_proceso->id_nodo][2]); // liberar esperando
                            sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                        }
                        else
                        {                                                               // no existen de prioridad 0 en mi nodo
                            sem_post(&sem_var_esperando[detalles_proceso->id_nodo][2]); // liberar esperando
                            // bloquear variables id nodos pendientes y nodos pendientes
                            sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                            sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);

                            if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] > 0)
                            { // existen prioridades 2 en otros nodos esperando?
                                detalles_nodos[detalles_proceso->id_nodo].corta = 0;
                                sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                                detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                                sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                                struct msg m = {
                                    .mtype = 1,
                                    .tipo = REPLY,
                                    .ticket = -1,
                                    .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                                    .rol = detalles_proceso->rol};
                                /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2]-- > 0) {
                                    //msgsnd(msg_ids[ detalles_nodos[detalles_proceso->id_nodo] // detalles de tu nodo
                                      //      .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2]][2]], &m, sizeof(m), 0);
                                } */
                                detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] = 0;
                                for (int i; i < NUMERO_NODOS - 1; i++)
                                {
                                    msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                                       .id_nodos[i]],
                                           &m, sizeof(m), 0);
                                    sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                    total_mensajes++;
                                    sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                }

                                sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);
                            }
                            else
                            {
                                sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);
                            }
                        }
                    }
                }
            }
        }
    }
    else
    { // no soy el ultimo
        // miro si no hay nadie esperando en otros nodos
        sem_post(&sem_var_esperando[detalles_proceso->id_nodo][detalles_proceso->prioridad]); // liberar esperando
        sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
        sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
        sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
        if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] == 0 &&
            detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] == 0 &&
            detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] == 0)
        {                                                               // si no hay nadie, tengo que ver al primero que le paso segun prioridad
            sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][0]); // miro si hay prioridad 0
            if (detalles_nodos[detalles_proceso->id_nodo].esperando[0] > 0)
            { // existen prioridades 0 en este nodo esperando?
                detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][0]); // darle paso a la primera de prioridad 0
                sem_post(&sem_var_esperando[detalles_proceso->id_nodo][0]);  // liberar esperando
            }
            else
            {
                sem_post(&sem_var_esperando[detalles_proceso->id_nodo][0]); // liberar esperando
                sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][1]); // prioridad 1 -> cancelaciones
                if (detalles_nodos[detalles_proceso->id_nodo].esperando[1] > 0)
                { // existen prioridades 1 en este nodo esperando?
                    detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                    sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][1]); // darle paso a la primera de prioridad 1
                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][1]);  // liberar esperando
                }
                else
                {
                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][1]); // liberar esperando
                    sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][2]); // prioridad 2 -> cancelaciones
                    if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] > 0)
                    { // existen prioridades 2 en este nodo esperando?
                        detalles_nodos[detalles_proceso->id_nodo].corta = 0;
                        sem_wait(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                        if (detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0 && detalles_nodos[detalles_proceso->id_nodo].esperando[2] - detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0)
                        { // existen consulta y reservas
                            // si existen ambas, aleatorio
                            if (rand() % 2 == 0)
                            {
                                sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
                            }
                            else
                            {
                                sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][2]); // darle paso a la primera de prioridad 2
                            }
                        }
                        else if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] - detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0)
                        {                                                                // si solo existen reservas
                            sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][2]); // darle paso a la primera de prioridad 2
                        }
                        else
                        { // si solo existen consultas
                            sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
                        }
                        sem_post(&sem_var_esperando[detalles_proceso->id_nodo][2]); // liberar esperando
                        sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                    }
                    else
                    {
                        sem_post(&sem_var_esperando[detalles_proceso->id_nodo][2]); // liberar esperando
                    }
                }
            }
        }
        else
        { // si hay alguien en los otros nodos contamos adelantamientos
            sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
            sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
            sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
            sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);

            if (detalles_nodos[detalles_proceso->id_nodo].adelantamientos < NUMERO_ADELANTAMIENTOS)
            {
                detalles_nodos[detalles_proceso->id_nodo].adelantamientos++;
                sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][0]); // prioridad 0 -> cancelaciones
                if (detalles_nodos[detalles_proceso->id_nodo].esperando[0] > 0)
                { // existen prioridades 0 en este nodo esperando?
                    detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                    sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][0]); // darle paso a la primera de prioridad 0
                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][0]);  // liberar esperando
                }
                else
                {                                                               // no existen de prioridad 0 en mi nodo
                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][0]); // liberar esperando
                    // bloquear variables id nodos pendientes y nodos pendientes
                    sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                    sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);

                    if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] > 0)
                    { // existen prioridades 0 en otros nodos esperando?
                        detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                        sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                        detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                        sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                        struct msg m = {
                            .mtype = 1,
                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                            .rol = detalles_proceso->rol};
                        /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0]-- > 0) {
                        //  msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                            //   .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0]][0]], &m, sizeof(m), 0);
                        } */
                        detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] = 0;
                        for (int i; i < NUMERO_NODOS - 1; i++)
                        {
                            msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                               .id_nodos[i]],
                                   &m, sizeof(m), 0);
                            sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                            total_mensajes++;
                            sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);                               
                        }

                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);
                    }
                    else
                    { // no existen prioridades 0 en otros nodos esperando
                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);

                        // comprobar prioridad 1 en este nodo y luego fuera
                        sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][1]); // prioridad 1 -> cancelaciones
                        if (detalles_nodos[detalles_proceso->id_nodo].esperando[1] > 0)
                        { // existen prioridades 1 en este nodo esperando?
                            detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                            sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][1]); // darle paso a la primera de prioridad 1
                            sem_post(&sem_var_esperando[detalles_proceso->id_nodo][1]);  // liberar esperando
                        }
                        else
                        {                                                               // no existen de prioridad 1 en mi nodo
                            sem_post(&sem_var_esperando[detalles_proceso->id_nodo][1]); // liberar esperando
                            // bloquear variables id nodos pendientes y nodos pendientes
                            sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                            sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);

                            if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] > 0)
                            { // existen prioridades 1 en otros nodos esperando? TODO: no pilla prioridades de los otros nodos
                                detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                                sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                                detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                                sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                                struct msg m = {
                                    .mtype = 1,
                                    .tipo = REPLY,
                                    .ticket = -1,
                                    .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                                    .rol = detalles_proceso->rol};
                                /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1]-- > 0) {
                                    //msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                    //  .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1]][1]], &m, sizeof(m), 0);
                                } */
                                detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] = 0;
                                for (int i; i < NUMERO_NODOS - 1; i++)
                                {
                                    msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                                       .id_nodos[i]],
                                           &m, sizeof(m), 0);
                                    sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                    total_mensajes++;
                                    sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                }

                                sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                                sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);
                            }
                            else
                            {
                                sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                                sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);

                                // comprobar prioridad 2 en este nodo y luego fuera
                                sem_wait(&sem_var_esperando[detalles_proceso->id_nodo][2]); // prioridad 2 ->
                                if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] > 0)
                                { // existen prioridades 2 en este nodo esperando?
                                    detalles_nodos[detalles_proceso->id_nodo].corta = 0;
                                    sem_wait(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                                    if (detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0 && detalles_nodos[detalles_proceso->id_nodo].esperando[2] - detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0)
                                    { // existen consulta y reservas
                                        // si existen ambas, aleatorio
                                        if (rand() % 2 == 0)
                                        {
                                            sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
                                        }
                                        else
                                        {
                                            sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][2]); // darle paso a la primera de prioridad 2
                                        }
                                    }
                                    else if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] - detalles_nodos[detalles_proceso->id_nodo].consultas_esperando > 0)
                                    {                                                                // si solo existen reservas
                                        sem_post(&sem_paso_prioridad[detalles_proceso->id_nodo][2]); // darle paso a la primera de prioridad 2
                                    }
                                    else
                                    { // si solo existen consultas
                                        sem_post(&sem_paso_consultas[detalles_proceso->id_nodo]);
                                    }
                                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][2]); // liberar esperando
                                    sem_post(&sem_var_consultas_esperando[detalles_proceso->id_nodo]);
                                }
                                else
                                {                                                               // no existen de prioridad 0 en mi nodo
                                    sem_post(&sem_var_esperando[detalles_proceso->id_nodo][2]); // liberar esperando
                                    // bloquear variables id nodos pendientes y nodos pendientes
                                    sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                    sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);

                                    if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] > 0)
                                    { // existen prioridades 2 en otros nodos esperando?
                                        detalles_nodos[detalles_proceso->id_nodo].corta = 0;
                                        sem_wait(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                                        detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                                        sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                                        struct msg m = {
                                            .mtype = 1,
                                            .tipo = REPLY,
                                            .ticket = -1,
                                            .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                                            .rol = detalles_proceso->rol};
                                        /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2]-- > 0) {
                                            //msgsnd(msg_ids[ detalles_nodos[detalles_proceso->id_nodo] // detalles de tu nodo
                                            //      .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2]][2]], &m, sizeof(m), 0);
                                        } */
                                        detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] = 0;
                                        for (int i; i < NUMERO_NODOS - 1; i++)
                                        {
                                            msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                                               .id_nodos[i]],
                                                   &m, sizeof(m), 0);
                                            sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                            total_mensajes++;
                                            sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                        }

                                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                    }
                                    else
                                    {
                                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);
                                    }
                                }
                            }
                        }
                    }
                }
                sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
            }
            else
            { // adelantamos suficiente, hay que esperar
                detalles_nodos[detalles_proceso->id_nodo].adelantamientos = 0;
                sem_post(&sem_var_adelantamientos[detalles_proceso->id_nodo][detalles_proceso->prioridad]);
                // bloquear variables id nodos pendientes y nodos pendientes
                // PRIORIDAD 0
                sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);

                if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] > 0)
                { // existen prioridades 0 en otros nodos esperando?
                    detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                    struct msg m = {
                        .mtype = 1,
                        .tipo = REPLY,
                        .ticket = -1,
                        .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                        .rol = detalles_proceso->rol};
                    /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0]-- > 0) {
                        //msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                          //  .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0]][0]], &m, sizeof(m), 0);
                    } */
                    if (detalles_nodos[detalles_proceso->id_nodo].esperando[0] == 0)
                    {
                        detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[0] = 0;
                    }

                    for (int i; i < NUMERO_NODOS - 1; i++)
                    {
                        msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                           .id_nodos[i]],
                               &m, sizeof(m), 0);
                        sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                        total_mensajes++;
                        sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                    }

                    sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                    sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);
                }
                else
                {
                    sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][0]);
                    sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][0]);

                    // bloquear variables id nodos pendientes y nodos pendientes
                    sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                    sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);

                    if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] > 0)
                    { // existen prioridades 1 en otros nodos esperando?
                        detalles_nodos[detalles_proceso->id_nodo].corta = 1;
                        struct msg m = {
                            .mtype = 1,
                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                            .rol = detalles_proceso->rol};
                        /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1]-- > 0) {
                           // msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                             //   .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1]][1]], &m, sizeof(m), 0);
                        } */
                        if (detalles_nodos[detalles_proceso->id_nodo].esperando[1] == 0)
                        {
                            detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[1] = 0;
                        }

                        for (int i; i < NUMERO_NODOS - 1; i++)
                        {
                            msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                               .id_nodos[i]],
                                   &m, sizeof(m), 0);
                            sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                            total_mensajes++;
                            sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                        }

                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);
                    }
                    else
                    {
                        sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][1]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][1]);

                        // prioridad 2
                        // bloquear variables id nodos pendientes y nodos pendientes
                        sem_wait(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                        sem_wait(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);

                        if (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] > 0)
                        { // existen prioridades 2 en otros nodos esperando?
                            detalles_nodos[detalles_proceso->id_nodo].corta = 0;
                            struct msg m = {
                                .mtype = 1,

                                .tipo = REPLY,
                                .ticket = -1,
                                .id_nodo = detalles_nodos[detalles_proceso->id_nodo].mi_id,
                                .rol = detalles_proceso->rol};
                            /* while (detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2]-- > 0) {
                                //msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                  //  .id_nodos_pendientes[detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2]][2]], &m, sizeof(m), 0);
                            } */
                            if (detalles_nodos[detalles_proceso->id_nodo].esperando[2] == 0)
                            {
                                detalles_nodos[detalles_proceso->id_nodo].nodos_pendientes[2] = 0;
                            }

                            for (int i; i < NUMERO_NODOS - 1; i++)
                            {
                                msgsnd(msg_ids[detalles_nodos[detalles_proceso->id_nodo]
                                                   .id_nodos[i]],
                                       &m, sizeof(m), 0);
                                sem_wait(&sem_total_mensajes[detalles_proceso->id_nodo]);
                                total_mensajes++;
                                sem_post(&sem_total_mensajes[detalles_proceso->id_nodo]);
                            }

                            sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                            sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);
                        }
                        else
                        {
                            sem_post(&sem_var_nodos_pendientes[detalles_proceso->id_nodo][2]);
                            sem_post(&sem_var_id_nodos_pendientes[detalles_proceso->id_nodo][2]);
                        }
                    }
                }
            }
        }
    }

    detalles_nodos[detalles_proceso->id_nodo].salir = 1;

    fin_ciclo = clock();

    tiempo_ciclo_vida = ((double)(fin_ciclo - inicio_ciclo)) / CLOCKS_PER_SEC;

    tiempo_transcurrido = ((double)(fin - inicio)) / CLOCKS_PER_SEC;

    //Escribir en fichero 1
    FILE *archivo;
    archivo = fopen("TiempoParaEntrarSC.txt", "a");
    if (archivo == NULL) {
        fprintf(stderr, "Error al abrir el archivo.\n");
        //return 1;
    }
    
    fprintf(archivo, "%f,%f\n", (double)detalles_proceso->id_nodo, tiempo_transcurrido);
    
    fclose(archivo);

    tiempo_transcurrido_sc = ((double)(fin_sc - inicio_sc)) / CLOCKS_PER_SEC;

    //Escribir en fichero 2
    FILE *archivo2;
    archivo2 = fopen("TiempoDentroSC.txt", "a");
    if (archivo2 == NULL) {
        fprintf(stderr, "Error al abrir el archivo.\n");
        //return 1;
    }
    
    fprintf(archivo2, "%f,%f\n", (double)detalles_proceso->id_nodo, tiempo_transcurrido_sc);
    
    fclose(archivo2);

    //Escribimos en fichero 3
    FILE *archivo3;
    archivo3 = fopen("MensajesXNodo.txt", "a");
    if (archivo3 == NULL) {
        fprintf(stderr, "Error al abrir el archivo.\n");
        //return 1;
    }
    
    fprintf(archivo3, "%d,%d\n", detalles_proceso->id_nodo, total_mensajes);
    
    fclose(archivo3);

    //Escribimos en fichero 4
    FILE *archivo4;
    archivo4 = fopen("TiempoDeVida.txt", "a");
    if (archivo4== NULL) {
        fprintf(stderr, "Error al abrir el archivo.\n");
        //return 1;
    }
    
    fprintf(archivo4, "%d,%f\n", detalles_proceso->id_nodo, tiempo_ciclo_vida);
    
    fclose(archivo4);



    pthread_exit(EXIT_SUCCESS);
}

void receptor(struct detalles_nodo *detalles_nodo)
{
    srand(time(NULL));
    while (1)
    {
        struct msg m;
        // printf("esperando en msgid: %d id: %d\n", msg_ids[detalles_nodo->mi_id], detalles_nodo->mi_id);
        msgrcv(msg_ids[detalles_nodo->mi_id], &m, sizeof(struct msg), 0, 0);
        // printf("[Soy %d]Recibi mensaje tipo %d de %d\n",detalles_nodo->mi_id, m.tipo, m.id_nodo);
        if (m.tipo == REQUEST)
        {
            sem_wait(&sem_var_dentro[detalles_nodo->mi_id]);
            if (detalles_nodo->dentro == 0)
            {
                sem_wait(&sem_var_esperando[detalles_nodo->mi_id][0]);
                sem_wait(&sem_var_esperando[detalles_nodo->mi_id][1]);
                sem_wait(&sem_var_esperando[detalles_nodo->mi_id][2]);
                if (detalles_nodo->esperando[0] > 0)
                {
                    if ((m.rol == CANCELACIONES) &&
                        (detalles_nodo->mi_ticket > m.ticket ||
                         (detalles_nodo->mi_ticket == m.ticket && detalles_nodo->mi_id > m.id_nodo)))
                    {
                        struct msg r = {
                            .mtype = 1,
                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = CANCELACIONES // CANCELACIONES por que se "decodifica" en el receptor de los REPLY a prioridad 0
                        };
                        msgsnd(msg_ids[m.id_nodo], &r, sizeof(struct msg), 0);
                        // printf("yo %d respondi a %d un reply %d\n", detalles_nodo->mi_id, msg_ids[m.id_nodo], r.tipo);
                    }
                    else
                    {
                        int prioridad = 0;
                        if (m.rol == CANCELACIONES)
                        {
                            prioridad = 0;
                        }
                        else if (m.rol == PAGO || m.rol == ADMINISTRACION)
                        {
                            prioridad = 1;
                        }
                        else
                        {
                            prioridad = 2;
                        }
                        sem_wait(&sem_var_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                        sem_wait(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                        detalles_nodo->id_nodos_pendientes[detalles_nodo->nodos_pendientes[prioridad]++][prioridad] = m.id_nodo;
                        sem_post(&sem_var_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                    }
                }
                else if (detalles_nodo->esperando[1] > 0)
                {
                    if (m.rol == CANCELACIONES)
                    {
                        // abortar mi peticion
                        printf("[Soy %d] Abortado por %d", detalles_nodo->mi_id, m.id_nodo);
                        sem_wait(&sem_var_esperando[detalles_nodo->mi_id][2]);
                        if (detalles_nodo->esperando[2] > 0)
                        { // solo si existen prioridades 2 en este nodo esperando las abortamos
                            sem_wait(&sem_var_abortado[detalles_nodo->mi_id][2]);
                            detalles_nodo->abortados[2]++;
                            printf("[Soy %d de 2] Abortado por %d", detalles_nodo->mi_id, m.id_nodo);
                            sem_post(&sem_var_abortado[detalles_nodo->mi_id][2]);
                            struct msg r = {
                                .mtype = 1,

                                .tipo = REPLY,
                                .ticket = -1,
                                .id_nodo = detalles_nodo->mi_id,
                                .rol = RESERVA // RESERVA por que se "decodifica" en el receptor de los REPLY a prioridad 2
                            };
                            msgsnd(msg_ids[m.id_nodo], &r, sizeof(struct msg), 0);
                            // actualizar mi ticket
                            if (m.ticket > detalles_nodo->mi_ticket)
                            {
                                detalles_nodo->mi_ticket = m.ticket;
                            }
                            struct msg m2 = {
                                .mtype = 1,

                                .tipo = REQUEST,
                                .ticket = detalles_nodo->mi_ticket + 1,
                                .id_nodo = detalles_nodo->mi_id,
                                .rol = RESERVA};
                            msgsnd(msg_ids[m.id_nodo], &m2, sizeof(struct msg), 0);
                            printf("[Soy %d] mando REQUEST a %d", detalles_nodo->mi_id, m.id_nodo);
                        }
                        sem_post(&sem_var_esperando[detalles_nodo->mi_id][2]);

                        sem_wait(&sem_var_abortado[detalles_nodo->mi_id][1]);
                        detalles_nodo->abortados[1]++;
                        sem_post(&sem_var_abortado[detalles_nodo->mi_id][1]);
                        struct msg r = {
                            .mtype = 1,

                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = PAGO // CANCELACIONES por que se "decodifica" en el receptor de los REPLY a prioridad 0
                        };
                        msgsnd(msg_ids[m.id_nodo], &r, sizeof(struct msg), 0);
                        // actualizar mi ticket
                        if (m.ticket > detalles_nodo->mi_ticket)
                        {
                            detalles_nodo->mi_ticket = m.ticket;
                        }
                        struct msg m2 = {
                            .mtype = 1,

                            .tipo = REQUEST,
                            .ticket = detalles_nodo->mi_ticket + 1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = PAGO};
                        msgsnd(msg_ids[m.id_nodo], &m2, sizeof(struct msg), 0);
                        printf("[Soy %d] mando REQUEST a %d", detalles_nodo->mi_id, m.id_nodo);
                    }
                    else if ((m.rol == PAGO || m.rol == ADMINISTRACION) &&
                             (detalles_nodo->mi_ticket > m.ticket ||
                              (detalles_nodo->mi_ticket == m.ticket && detalles_nodo->mi_id > m.id_nodo)))
                    {
                        struct msg r = {
                            .mtype = 1,

                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = PAGO // PAGO por que se "decodifica" en el receptor de los REPLY a prioridad 1
                        };
                        msgsnd(msg_ids[m.id_nodo], &r, sizeof(struct msg), 0);
                    }
                    else
                    {
                        int prioridad = 0;
                        if (m.rol == CANCELACIONES)
                        {
                            prioridad = 0;
                        }
                        else if (m.rol == PAGO || m.rol == ADMINISTRACION)
                        {
                            prioridad = 1;
                        }
                        else
                        {
                            prioridad = 2;
                        }

                        sem_wait(&sem_var_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                        sem_wait(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                        detalles_nodo->id_nodos_pendientes[detalles_nodo->nodos_pendientes[prioridad]++][prioridad] = m.id_nodo;
                        sem_post(&sem_var_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                    }
                }
                else
                { // somos consulta o reserva
                    if (m.rol == PAGO || m.rol == ADMINISTRACION || m.rol == CANCELACIONES)
                    {
                        // abortar mi peticion
                        sem_wait(&sem_var_abortado[detalles_nodo->mi_id][2]);
                        detalles_nodo->abortados[2]++;
                        sem_post(&sem_var_abortado[detalles_nodo->mi_id][2]);
                        struct msg r = {
                            .mtype = 1,

                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = RESERVA // RESERVA por que se "decodifica" en el receptor de los REPLY a prioridad 2
                        };
                        msgsnd(msg_ids[m.id_nodo], &r, sizeof(struct msg), 0);
                        // actualizar mi ticket
                        if (m.ticket > detalles_nodo->mi_ticket)
                        {
                            detalles_nodo->mi_ticket = m.ticket;
                        }
                        struct msg m2 = {
                            .mtype = 1,

                            .tipo = REQUEST,
                            .ticket = detalles_nodo->mi_ticket + 1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = RESERVA};
                        msgsnd(msg_ids[m.id_nodo], &m2, sizeof(struct msg), 0);
                    }
                    else if (detalles_nodo->mi_ticket > m.ticket ||
                             (detalles_nodo->mi_ticket == m.ticket && detalles_nodo->mi_id > m.id_nodo))
                    {
                        struct msg r = {
                            .mtype = 1,

                            .tipo = REPLY,
                            .ticket = -1,
                            .id_nodo = detalles_nodo->mi_id,
                            .rol = RESERVA // RESERVA por que se "decodifica" en el receptor de los REPLY a prioridad 2
                        };
                        msgsnd(msg_ids[m.id_nodo], &r, sizeof(struct msg), 0);
                    }
                    else
                    {
                        sem_wait(&sem_var_nodos_pendientes[detalles_nodo->mi_id][2]);
                        sem_wait(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][2]);
                        detalles_nodo->id_nodos_pendientes[detalles_nodo->nodos_pendientes[2]++][2] = m.id_nodo;
                        sem_post(&sem_var_nodos_pendientes[detalles_nodo->mi_id][2]);
                        sem_post(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][2]);
                    }
                }
                sem_post(&sem_var_esperando[detalles_nodo->mi_id][0]);
                sem_post(&sem_var_esperando[detalles_nodo->mi_id][1]);
                sem_post(&sem_var_esperando[detalles_nodo->mi_id][2]);
            }
            else
            { // no esta dentro
                int prioridad;
                if (m.rol == CANCELACIONES)
                {
                    prioridad = 0;
                }
                else if (m.rol == PAGO || m.rol == ADMINISTRACION)
                {
                    prioridad = 1;
                }
                else
                {
                    prioridad = 2;
                }

                sem_wait(&sem_var_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                sem_wait(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                detalles_nodo->id_nodos_pendientes[detalles_nodo->nodos_pendientes[prioridad]++][prioridad] = m.id_nodo;
                sem_post(&sem_var_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
                sem_post(&sem_var_id_nodos_pendientes[detalles_nodo->mi_id][prioridad]);
            }
            sem_post(&sem_var_dentro[detalles_nodo->mi_id]);
        }
        else
        { // REPLY
            int prioridad;
            if (m.rol == CANCELACIONES)
            {
                prioridad = 0;
            }
            else if (m.rol == PAGO || m.rol == ADMINISTRACION)
            {
                prioridad = 1;
            }
            else
            {
                prioridad = 2;
            }
            sem_wait(&sem_var_replies_recibidos[detalles_nodo->mi_id]); //[prioridad]);
            sem_wait(&sem_var_abortado[detalles_nodo->mi_id][prioridad]);
            detalles_nodo->replies_recibidos++; //[prioridad]++;
            if (detalles_nodo->replies_recibidos == ((NUMERO_NODOS - 1) + detalles_nodo->abortados[prioridad]))
            { // if (detalles_nodo->replies_recibidos[prioridad] == ((NUMERO_NODOS - 1) + detalles_nodo->abortados[prioridad])) {
                // copiado
                sem_wait(&sem_var_esperando[detalles_nodo->mi_id][0]); // miro si hay prioridad 0
                if (detalles_nodo->esperando[0] > 0)
                {                                                           // existen prioridades 0 en este nodo esperando?
                    sem_post(&sem_paso_prioridad[detalles_nodo->mi_id][0]); // darle paso a la primera de prioridad 0
                    sem_post(&sem_var_esperando[detalles_nodo->mi_id][0]);  // liberar esperando
                }
                else
                {
                    sem_post(&sem_var_esperando[detalles_nodo->mi_id][0]); // liberar esperando
                    sem_wait(&sem_var_esperando[detalles_nodo->mi_id][1]); // prioridad 1 -> cancelaciones
                    if (detalles_nodo->esperando[1] > 0)
                    {                                                           // existen prioridades 1 en este nodo esperando?
                        sem_post(&sem_paso_prioridad[detalles_nodo->mi_id][1]); // darle paso a la primera de prioridad 1
                        sem_post(&sem_var_esperando[detalles_nodo->mi_id][1]);  // liberar esperando
                    }
                    else
                    {
                        sem_post(&sem_var_esperando[detalles_nodo->mi_id][1]); // liberar esperando
                        sem_wait(&sem_var_esperando[detalles_nodo->mi_id][2]); // prioridad 2 -> cancelaciones
                        if (detalles_nodo->esperando[2] > 0)
                        { // existen prioridades 2 en este nodo esperando?
                            sem_wait(&sem_var_consultas_esperando[detalles_nodo->mi_id]);
                            if (detalles_nodo->consultas_esperando > 0 && detalles_nodo->esperando[2] - detalles_nodo->consultas_esperando > 0)
                            { // existen consulta y reservas
                                // si existen ambas, aleatorio
                                if (rand() % 2 == 0)
                                {
                                    sem_post(&sem_paso_consultas[detalles_nodo->mi_id]);
                                }
                                else
                                {
                                    sem_post(&sem_paso_prioridad[detalles_nodo->mi_id][2]); // darle paso a la primera de prioridad 2
                                }
                            }
                            else if (detalles_nodo->esperando[2] - detalles_nodo->consultas_esperando > 0)
                            {                                                           // si solo existen reservas
                                sem_post(&sem_paso_prioridad[detalles_nodo->mi_id][2]); // darle paso a la primera de prioridad 2
                            }
                            else
                            { // si solo existen consultas
                                sem_post(&sem_paso_consultas[detalles_nodo->mi_id]);
                            }
                            sem_post(&sem_var_esperando[detalles_nodo->mi_id][2]); // liberar esperando
                            sem_post(&sem_var_consultas_esperando[detalles_nodo->mi_id]);
                        }
                        else
                        {
                            sem_post(&sem_var_esperando[detalles_nodo->mi_id][2]); // liberar esperando
                        }
                    }
                }
                detalles_nodo->replies_recibidos = 0;                       // detalles_nodo->replies_recibidos[prioridad] = 0;
                sem_post(&sem_var_replies_recibidos[detalles_nodo->mi_id]); // sem_post(&sem_var_replies_recibidos[detalles_nodo->mi_id][prioridad]);
                sem_post(&sem_var_abortado[detalles_nodo->mi_id][prioridad]);
            }
            sem_post(&sem_var_replies_recibidos[detalles_nodo->mi_id]); // sem_post(&sem_var_replies_recibidos[detalles_nodo->mi_id][prioridad]);
            sem_post(&sem_var_abortado[detalles_nodo->mi_id][prioridad]);
        }
        // actualizar mi ticket
        if (m.ticket > detalles_nodo->mi_ticket)
        {
            detalles_nodo->mi_ticket = m.ticket;
        }
    }
    pthread_exit(EXIT_SUCCESS);
}

int *removeInt(int arr[], int size, int target)
{
    int *newArr = (int *)malloc(size * sizeof(int));
    if (newArr == NULL)
    {
        return NULL;
    }

    int found = 0;
    for (int i = 0; i < size; i++)
    {
        if (arr[i] == target && !found)
        {
            found = 1;
            continue;
        }

        newArr[i - found] = arr[i];
    }

    return newArr;
}

void parseRoles(char *input, struct Data *data)
{   // input = "1c2r3a4p5x"
    // Initialize struct members to 0
    data->numConsultas = 0;
    data->numReservas = 0;
    data->numAdministracion = 0;
    data->numPago = 0;
    data->numCancelaciones = 0;

    int len = strlen(input);
    int i = 0;
    while (i < len)
    {
        // Extract the number
        int num = 0;
        while (input[i] >= '0' && input[i] <= '9')
        {
            num = num * 10 + (input[i] - '0');
            i++;
        }
        // Extract the letter
        char letter = input[i++];
        // Update the corresponding struct member based on the letter
        switch (letter)
        {
        case 'c':
            data->numConsultas = num;
            break;
        case 'r':
            data->numReservas = num;
            break;
        case 'a':
            data->numAdministracion = num;
            break;
        case 'p':
            data->numPago = num;
            break;
        case 'x':
            data->numCancelaciones = num;
            break;
        default:
            break;
        }
    }
}