#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define RESET   "\033[0m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define BLUE    "\033[34m"
#define RED     "\033[31m"


/// VARIABLES GLOBALES DEL RECURSO COMPARTIDO
int recurso = 0;               // Recurso compartido entre lectores y escritores
int lectores_activos = 0;      // Número de lectores leyendo actualmente
int escritores_esperando = 0;  // Número de escritores esperando su turno
bool escribiendo = false;      // Indica si un escritor está escribiendo actualmente


/// SINCRONIZACIÓN: mutex y variables de condición
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;           // Mutex general
pthread_mutex_t mtx_escritura = PTHREAD_MUTEX_INITIALIZER; // Mutex exclusivo para escritores
pthread_cond_t puede_leer = PTHREAD_COND_INITIALIZER;      // Señal para lectores
pthread_cond_t puede_escribir = PTHREAD_COND_INITIALIZER;  // Señal para escritores


/// MÉTRICAS DE DESEMPEÑO
long total_lecturas = 0;
long total_escrituras = 0;
long long tiempo_espera_lectores = 0;
long long tiempo_espera_escritores = 0;

/// FUNCIÓN PARA REINICIAR LAS VARIABLES ENTRE SIMULACIONES
void reiniciarVariables() {
    recurso = 0;
    lectores_activos = 0;
    escritores_esperando = 0;
    escribiendo = false;
    total_lecturas = 0;
    total_escrituras = 0;
    tiempo_espera_lectores = 0;
    tiempo_espera_escritores = 0;
}

/// FUNCIÓN PARA OBTENER TIEMPO EN MICROSEGUNDOS
long long microsegundos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

void separador() {
    printf(BLUE "------------------------------------------------------------\n" RESET);
}

/// MODO 1: PRIORIDAD A LECTORES

///  HILO DE LECTOR CON PRIORIDAD A LECTORES 
void *lectorPrioridadLectores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();

        // Bloqueo de acceso al contador de lectores
        pthread_mutex_lock(&mtx);
        lectores_activos++;
        if (lectores_activos == 1)
            pthread_mutex_lock(&mtx_escritura);  // Primer lector bloquea a escritores
        pthread_mutex_unlock(&mtx);

        long long fin = microsegundos();
        tiempo_espera_lectores += (fin - inicio);

        printf(GREEN "[Lector %d] leyendo valor -> %d\n" RESET, id, recurso);
        usleep((rand() % 40 + 10) * 1000);  // Simula tiempo de lectura

        // Cuando el lector termina
        pthread_mutex_lock(&mtx);
        lectores_activos--;
        if (lectores_activos == 0)
            pthread_mutex_unlock(&mtx_escritura); // Último lector libera a escritores
        pthread_mutex_unlock(&mtx);

        total_lecturas++;
        usleep((rand() % 40 + 10) * 1000); // Espera antes de volver a leer
    }
    return NULL;
}

///  HILO DE ESCRITOR CON PRIORIDAD A LECTORES 
void *escritorPrioridadLectores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();
        pthread_mutex_lock(&mtx_escritura);  // Espera su turno para escribir
        long long fin = microsegundos();
        tiempo_espera_escritores += (fin - inicio);

        recurso++;  // Modifica el recurso
        printf(YELLOW " [Escritor %d] modifico recurso -> %d\n" RESET, id, recurso);
        usleep((rand() % 60 + 20) * 1000);  // Simula tiempo de escritura

        pthread_mutex_unlock(&mtx_escritura);  // Libera el recurso
        total_escrituras++;
        usleep((rand() % 60 + 20) * 1000);
    }
    return NULL;
}

/// FUNCIÓN PRINCIPAL DE PRIORIDAD A LECTORES 
void prioridadLectores() {
    printf(CYAN "\n=== SIMULACION: PRIORIDAD A LECTORES ===\n" RESET);
    separador();
    reiniciarVariables();

    pthread_t lectores[6], escritores[3];
    int idL[6], idE[3];

    // Crear lectores
    for (int i = 0; i < 6; i++) {
        idL[i] = i + 1;
        pthread_create(&lectores[i], NULL, lectorPrioridadLectores, &idL[i]);
    }
    // Crear escritores
    for (int i = 0; i < 3; i++) {
        idE[i] = i + 1;
        pthread_create(&escritores[i], NULL, escritorPrioridadLectores, &idE[i]);
    }

    // Esperar a que todos terminen
    for (int i = 0; i < 6; i++) pthread_join(lectores[i], NULL);
    for (int i = 0; i < 3; i++) pthread_join(escritores[i], NULL);

    separador();
    printf(MAGENTA "\n RESULTADOS PRIORIDAD A LECTORES \n" RESET);
    printf("Lecturas totales: %ld\n", total_lecturas);
    printf("Escrituras totales: %ld\n", total_escrituras);
    printf("Promedio espera lectores: %.2f microsegundos\n", 
           (double)tiempo_espera_lectores / (total_lecturas ? total_lecturas : 1));
    printf("Promedio espera escritores: %.2f microsegundos\n",
           (double)tiempo_espera_escritores / (total_escrituras ? total_escrituras : 1));
    separador();
}

/// MODO 2: PRIORIDAD A ESCRITORES

/// HILO DE LECTOR CON PRIORIDAD A ESCRITORES
void *lectorPrioridadEscritores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();

        pthread_mutex_lock(&mtx);
        // Espera si hay escritores esperando o escribiendo
        while (escritores_esperando > 0 || escribiendo)
            pthread_cond_wait(&puede_leer, &mtx);

        long long fin = microsegundos();
        tiempo_espera_lectores += (fin - inicio);

        lectores_activos++;
        pthread_mutex_unlock(&mtx);

        printf(GREEN "[Lector %d] leyendo valor -> %d\n" RESET, id, recurso);
        usleep((rand() % 40 + 10) * 1000);

        pthread_mutex_lock(&mtx);
        lectores_activos--;
        if (lectores_activos == 0)
            pthread_cond_signal(&puede_escribir);
        pthread_mutex_unlock(&mtx);

        total_lecturas++;
        usleep((rand() % 40 + 10) * 1000);
    }
    return NULL;
}

/// HiLO DE ESCRITOR CON PRIORIDAD A ESCRITORES 
void *escritorPrioridadEscritores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();

        pthread_mutex_lock(&mtx);
        escritores_esperando++;
        while (lectores_activos > 0 || escribiendo)
            pthread_cond_wait(&puede_escribir, &mtx);
        escritores_esperando--;
        escribiendo = true;
        pthread_mutex_unlock(&mtx);

        long long fin = microsegundos();
        tiempo_espera_escritores += (fin - inicio);

        recurso++;
        printf(RED "[Escritor %d] modifico recurso -> %d\n" RESET, id, recurso);
        usleep((rand() % 60 + 20) * 1000);

        pthread_mutex_lock(&mtx);
        escribiendo = false;
        if (escritores_esperando > 0)
            pthread_cond_signal(&puede_escribir); // Otro escritor
        else
            pthread_cond_broadcast(&puede_leer);   // Libera lectores
        pthread_mutex_unlock(&mtx);

        total_escrituras++;
        usleep((rand() % 60 + 20) * 1000);
    }
    return NULL;
}

/// FUNCIÓN PRINCIPAL DE PRIORIDAD A ESCRITORES 
void prioridadEscritores() {
    printf(CYAN "\n=== SIMULACION: PRIORIDAD A ESCRITORES ===\n" RESET);
    separador();
    reiniciarVariables();

    pthread_t lectores[6], escritores[3];
    int idL[6], idE[3];

    // Crear lectores y escritores
    for (int i = 0; i < 6; i++) {
        idL[i] = i + 1;
        pthread_create(&lectores[i], NULL, lectorPrioridadEscritores, &idL[i]);
    }
    for (int i = 0; i < 3; i++) {
        idE[i] = i + 1;
        pthread_create(&escritores[i], NULL, escritorPrioridadEscritores, &idE[i]);
    }

    // Esperar que todos finalicen
    for (int i = 0; i < 6; i++) pthread_join(lectores[i], NULL);
    for (int i = 0; i < 3; i++) pthread_join(escritores[i], NULL);

    separador();
    printf(MAGENTA "\n RESULTADOS PRIORIDAD A ESCRITORES\n" RESET);
    printf("Lecturas totales: %ld\n", total_lecturas);
    printf("Escrituras totales: %ld\n", total_escrituras);
    printf("Promedio espera lectores: %.2f microsegundos\n", 
           (double)tiempo_espera_lectores / (total_lecturas ? total_lecturas : 1));
    printf("Promedio espera escritores: %.2f microsegundos\n",
           (double)tiempo_espera_escritores / (total_escrituras ? total_escrituras : 1));
    separador();
}

/// FUNCIÓN PRINCIPAL (MENÚ DE EJECUCIÓN)
int main() {
    int opcion;
    do {
        printf(CYAN "\n============================================\n" RESET);
        printf("      SIMULADOR LECTORES / ESCRITORES   \n");
        printf("============================================\n");
        printf("  1. Ejecutar Prioridad a Lectores\n");
        printf("  2. Ejecutar Prioridad a Escritores\n");
        printf("  3. Salir del Programa\n");
        printf("    Seleccione una opcion:  ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1: prioridadLectores(); break;
            case 2: prioridadEscritores(); break;
            case 3: printf(GREEN "\n Saliendo del programa...\n" RESET); break;
            default: printf(RED "\n Opcion invalida. Intente de nuevo.\n" RESET);
        }
    } while (opcion != 3);

    return 0;
}
