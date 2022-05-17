 #include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

//Comida maxima
#define comida_maxima 5
//Tiempo de espera
#define tiempo_espera_maximo 3000000
//Numero de filosofos
#define numero_filosofos 3
//Tiempo de trabajo
#define Tiempo_de_trabajo 1

enum estados {
    PENSANDO, COMIENDO, HAMBRIENTO
};

struct FilosofosComensales {
    pthread_mutex_t bloquear;
    enum estados estado_actual[numero_filosofos];
    pthread_cond_t condicion[numero_filosofos];
    long tiempo_espera_total[numero_filosofos];
    long contador[numero_filosofos];
};

struct FilosofosComensales filosofos;

long tiempo_espera_unico[numero_filosofos];

// Salimos del nodo hijo sin preocuparnos de las perdidas de memoria
void salida() {
    long sum = 0;
    printf("Tiempo total de espera: ");
    for (int k = 0; k < numero_filosofos; k++) {
        sum += filosofos.tiempo_espera_total[k];
        printf("%f ", (float) filosofos.tiempo_espera_total[k] / 1000000);
    }
    printf("\n Tiempo medio de espera = %f\n", (float) sum / (numero_filosofos * 1000000));
    for (int k = 0; k < numero_filosofos; k++) {
        printf("%ld ", filosofos.contador[k]);
    }
    printf("\n Programa finalizado con exito ::: La comida se terminó \n");
    exit(0);
}


// Obtenemos la hora en este mismo momento
long getHora() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (long) (ts.tv_sec * 1000000000 + ts.tv_nsec);
    }
    return 0;
}


//Damos permiso a los demás filosofos sin están hambrientos y han esperado mucho tiempo
int conPermiso(int i) {
    long hora = getHora();
    long temp1 = 0, temp2 = 0, tiempo = tiempo_espera_unico[i] + hora;
    if (filosofos.estado_actual[(i + 1) % numero_filosofos] == HAMBRIENTO) {//El filosofo va a estar hambriento
        temp1 = tiempo_espera_unico[(i + 1) % numero_filosofos] + hora;
        if (temp1 > tiempo && temp1 > tiempo_espera_maximo) {//Evitando la inanicion por no comer
            printf("Filosofo %d dando paso a %d a comer para evitar INANICION\n", i, (i + 1) % numero_filosofos);
            return 0;
        }
    }
    if (filosofos.estado_actual[(i + numero_filosofos - 1) % numero_filosofos] == HAMBRIENTO) {
        temp2 = tiempo_espera_unico[(i + numero_filosofos - 1) % numero_filosofos] + hora;
        if (temp2 > tiempo && temp2 > tiempo_espera_maximo) {
            printf("Filosofo %d dando paso a %d a comer para evitar la inanicion\n", i, (i + numero_filosofos - 1) % numero_filosofos);
            return 0;
        }
    }
    return 1;
}

//Comprueba si los vecinos no estan comiendo y si el filosofo no esta hambriento
//Cambiamos estado_actual a comer y señalamos la condicion
void check_chopsticks(int i) {
    printf("Filosofo %d buscando tenedores\n", i);
    if (filosofos.estado_actual[i] == HAMBRIENTO && (filosofos.estado_actual[(i + 1) % numero_filosofos] != COMIENDO && filosofos.estado_actual[(i + numero_filosofos - 1) % numero_filosofos] != COMIENDO) && conPermiso(i)) {
        filosofos.estado_actual[i] = COMIENDO;

        //El tiempo de espera = tiempo de inicio - tiempo en este momento
        tiempo_espera_unico[i] += getHora();
        filosofos.tiempo_espera_total[i] += tiempo_espera_unico[i];
        //Esto no desbloquea el mutex, debemos hacerlo desde el padre
        pthread_cond_signal(&filosofos.condicion[i]);
    }
}

/*
Atomic call, change estado_actual of philosopher to thinking
Call the neighbours to check for free chopsticks
*/
void return_chopsticks(int philosopher_number) {
    static int meal_no = 0;

    pthread_mutex_lock(&filosofos.bloquear);
    filosofos.contador[philosopher_number]++;
    filosofos.estado_actual[philosopher_number] = PENSANDO;
    printf("Filosofo %d termina de comer\n", philosopher_number);

    meal_no++;
    printf("#Recuento de comidas = %d\n\n", meal_no);
    if (meal_no == comida_maxima) {
        salida();
    }
    check_chopsticks((philosopher_number + numero_filosofos - 1) % numero_filosofos);
    check_chopsticks((philosopher_number + 1) % numero_filosofos);
    pthread_mutex_unlock(&filosofos.bloquear);
}

/*
Atomic call, the philosopher becomes hungry
Checks for free chopsticks, if not free then wait till signal
*/
void pickup_chopsticks(int philosopher_number) {
    pthread_mutex_lock(&filosofos.bloquear);
    filosofos.estado_actual[philosopher_number] = HAMBRIENTO;
    printf("Filosofo %d esta hambriento\n", philosopher_number);

    // saving start time
    tiempo_espera_unico[philosopher_number] = (-1) * getHora();

    check_chopsticks(philosopher_number);
    if (filosofos.estado_actual[philosopher_number] != COMIENDO) {
        // need to already have the bloquear before calling this
        // release the bloquear, so that other threads can use shared data
        pthread_cond_wait(&filosofos.condicion[philosopher_number], &filosofos.bloquear);
    }
    pthread_mutex_unlock(&filosofos.bloquear);
}

/*
Runs infinitely for all philosophers in different threads
Philosophers think and eat in this function
*/
void *philosopher(void *param) {
    int philosopher_number = *(int *) param;
    while (1) {
        srand(time(NULL) + philosopher_number);
        int think_time = (rand() % Tiempo_de_trabajo) + 1;
        int eat_time = (rand() % Tiempo_de_trabajo) + 1;
        filosofos.estado_actual[philosopher_number] = PENSANDO;
        printf("Filosofo %d esta pensando durante %d segundos\n", philosopher_number, think_time);
        sleep(think_time);
        pickup_chopsticks(philosopher_number);
        printf("Filosofo %d esta comiendo durante %d segundos\n", philosopher_number, think_time);
        sleep(eat_time);
        return_chopsticks(philosopher_number);
    }
}

int main() {
    /*Initialization code*/
    pthread_t tid[numero_filosofos];
    int id[numero_filosofos];
    pthread_mutex_init(&filosofos.bloquear, NULL);
    for (int j = 0; j < numero_filosofos; j++) {
        id[j] = j;
        filosofos.estado_actual[j] = PENSANDO;
        filosofos.contador[j] = 0;
        filosofos.tiempo_espera_total[j] = 0;
        tiempo_espera_unico[j] = 0;
        pthread_create(&tid[j], NULL, &philosopher, &id[j]);
    }

    // main thread must keep running when other threads are running
    for (int j = 0; j < numero_filosofos; j++) {
        pthread_join(tid[j], NULL);
    }
}
