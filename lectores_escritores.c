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


int recurso = 0;
int lectores_activos = 0;
int escritores_esperando = 0;
bool escribiendo = false;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_escritura = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t puede_leer = PTHREAD_COND_INITIALIZER;
pthread_cond_t puede_escribir = PTHREAD_COND_INITIALIZER;

long total_lecturas = 0;
long total_escrituras = 0;
long long tiempo_espera_lectores = 0;
long long tiempo_espera_escritores = 0;



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

long long microsegundos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

void separador() {
    printf(BLUE "------------------------------------------------------------\n" RESET);
}


void *lectorPrioridadLectores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();

        pthread_mutex_lock(&mtx);
        lectores_activos++;
        if (lectores_activos == 1)
            pthread_mutex_lock(&mtx_escritura);
        pthread_mutex_unlock(&mtx);

        long long fin = microsegundos();
        tiempo_espera_lectores += (fin - inicio);

        printf(GREEN "👀 [Lector %d] leyendo valor -> %d\n" RESET, id, recurso);
        usleep((rand() % 40 + 10) * 1000);

        pthread_mutex_lock(&mtx);
        lectores_activos--;
        if (lectores_activos == 0)
            pthread_mutex_unlock(&mtx_escritura);
        pthread_mutex_unlock(&mtx);

        total_lecturas++;
        usleep((rand() % 40 + 10) * 1000);
    }
    return NULL;
}

void *escritorPrioridadLectores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();
        pthread_mutex_lock(&mtx_escritura);
        long long fin = microsegundos();
        tiempo_espera_escritores += (fin - inicio);

        recurso++;
        printf(YELLOW "📝 [Escritor %d] modificó recurso -> %d\n" RESET, id, recurso);
        usleep((rand() % 60 + 20) * 1000);

        pthread_mutex_unlock(&mtx_escritura);
        total_escrituras++;
        usleep((rand() % 60 + 20) * 1000);
    }
    return NULL;
}

void prioridadLectores() {
    printf(CYAN "\n=== SIMULACIÓN: PRIORIDAD A LECTORES ===\n" RESET);
    separador();
    reiniciarVariables();

    pthread_t lectores[6], escritores[3];
    int idL[6], idE[3];

    for (int i = 0; i < 6; i++) {
        idL[i] = i + 1;
        pthread_create(&lectores[i], NULL, lectorPrioridadLectores, &idL[i]);
    }
    for (int i = 0; i < 3; i++) {
        idE[i] = i + 1;
        pthread_create(&escritores[i], NULL, escritorPrioridadLectores, &idE[i]);
    }

    for (int i = 0; i < 6; i++) pthread_join(lectores[i], NULL);
    for (int i = 0; i < 3; i++) pthread_join(escritores[i], NULL);

    separador();
    printf(MAGENTA "\n RESULTADOS PRIORIDAD A LECTORES \n" RESET);
    printf("Lecturas totales: %ld\n", total_lecturas);
    printf("Escrituras totales: %ld\n", total_escrituras);
    printf("Promedio espera lectores: %.2f µs\n", 
           (double)tiempo_espera_lectores / (total_lecturas ? total_lecturas : 1));
    printf("Promedio espera escritores: %.2f µs\n",
           (double)tiempo_espera_escritores / (total_escrituras ? total_escrituras : 1));
    separador();
}

void *lectorPrioridadEscritores(void *arg) {
    int id = *(int *)arg;
    srand(time(NULL) ^ pthread_self());

    for (int i = 0; i < 20; i++) {
        long long inicio = microsegundos();

        pthread_mutex_lock(&mtx);
        while (escritores_esperando > 0 || escribiendo)
            pthread_cond_wait(&puede_leer, &mtx);

        long long fin = microsegundos();
        tiempo_espera_lectores += (fin - inicio);

        lectores_activos++;
        pthread_mutex_unlock(&mtx);

        printf(GREEN "👀 [Lector %d] leyendo valor -> %d\n" RESET, id, recurso);
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
        printf(RED "📝 [Escritor %d] modificó recurso -> %d\n" RESET, id, recurso);
        usleep((rand() % 60 + 20) * 1000);

        pthread_mutex_lock(&mtx);
        escribiendo = false;
        if (escritores_esperando > 0)
            pthread_cond_signal(&puede_escribir);
        else
            pthread_cond_broadcast(&puede_leer);
        pthread_mutex_unlock(&mtx);

        total_escrituras++;
        usleep((rand() % 60 + 20) * 1000);
    }
    return NULL;
}

void prioridadEscritores() {
    printf(CYAN "\n=== SIMULACIÓN: PRIORIDAD A ESCRITORES ===\n" RESET);
    separador();
    reiniciarVariables();

    pthread_t lectores[6], escritores[3];
    int idL[6], idE[3];

    for (int i = 0; i < 6; i++) {
        idL[i] = i + 1;
        pthread_create(&lectores[i], NULL, lectorPrioridadEscritores, &idL[i]);
    }
    for (int i = 0; i < 3; i++) {
        idE[i] = i + 1;
        pthread_create(&escritores[i], NULL, escritorPrioridadEscritores, &idE[i]);
    }

    for (int i = 0; i < 6; i++) pthread_join(lectores[i], NULL);
    for (int i = 0; i < 3; i++) pthread_join(escritores[i], NULL);

    separador();
    printf(MAGENTA "\n RESULTADOS PRIORIDAD A ESCRITORES 📊\n" RESET);
    printf("Lecturas totales: %ld\n", total_lecturas);
    printf("Escrituras totales: %ld\n", total_escrituras);
    printf("Promedio espera lectores: %.2f µs\n", 
           (double)tiempo_espera_lectores / (total_lecturas ? total_lecturas : 1));
    printf("Promedio espera escritores: %.2f µs\n",
           (double)tiempo_espera_escritores / (total_escrituras ? total_escrituras : 1));
    separador();
}


int main() {
    int opcion;
    do {
        printf(CYAN "\n============================================\n" RESET);
        printf("      SIMULADOR LECTORES–ESCRITORES (SO)\n");
        printf("============================================\n");
        printf("1️  Ejecutar Prioridad a Lectores\n");
        printf("  Ejecutar Prioridad a Escritores\n");
        printf("  Salir del Programa\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1: prioridadLectores(); break;
            case 2: prioridadEscritores(); break;
            case 3: printf(GREEN "\n Saliendo del programa. ¡BYE!\n" RESET); break;
            default: printf(RED "\n Opción inválida. Intente de nuevo.\n" RESET);
        }
    } while (opcion != 3);

    return 0;
}
