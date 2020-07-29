#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 2048
#define div_for_nano 1000000000

typedef struct
{
    int size;
    pthread_t *threads;
} thread_pool;

typedef struct
{
    int use;
    int fill;
    int *index;
    int size;
} bounded_buffer;

void *consumer(void *arg);
void *producer(void *arg);
int get();
void put(int idx);
int readFile(char *fname, float (*array)[SIZE]);
int init_buffer(int size);
int init_pool(int size);
void calc(int y);

float A_arr[SIZE][SIZE];
float B_arr[SIZE][SIZE];
float C_arr[SIZE][SIZE];

thread_pool pool;
bounded_buffer buffer;

sem_t full;
sem_t empty;
sem_t mutex;

struct timespec tspec;
long timeDist = 0;
long totalDist = 0;

int main(int argc, char *argv[])
{
    clock_gettime(CLOCK_REALTIME, &tspec);
    totalDist = tspec.tv_sec;

    pthread_t main_thread;
    float(*array[])[SIZE] = {A_arr, B_arr};
    int bb_size = 1;
    int tp_size = 1;
    FILE *fp = NULL;

    if (argc < 4)
    {
        fprintf(stderr, "[ERROR] Insufficient factors.\n");
        exit(-1);
    }
    else if (argc == 5)
    {
        bb_size = atoi(argv[4]);
        tp_size = 1;
    }
    else if (argc > 5)
    {
        bb_size = atoi(argv[4]);
        tp_size = atoi(argv[5]);
    }
    else if (argc == 4)
    {
        bb_size == 1;
        tp_size == 1;
    }

    // init thread pool
    if (!init_pool(tp_size))
    {
        fprintf(stderr, "[ERROR] malloc error\n");
        exit(-1);
    }

    // init bounded buffer
    if (!init_buffer(bb_size))
    {
        fprintf(stderr, "[ERROR] malloc error\n");
        exit(-1);
    }

    // Data Read
    clock_gettime(CLOCK_REALTIME, &tspec);
    timeDist = tspec.tv_nsec;
    for (int idx = 1; idx < 3; ++idx)
    {
        if (readFile(argv[idx], array[idx - 1]) < 0)
        {
            fprintf(stderr, "[ERROR] File Read Failed\n");
            exit(-1);
        }
    }
    clock_gettime(CLOCK_REALTIME, &tspec);
    fprintf(stdout, "[NOTICE] Data read time : %f\n", (double)(tspec.tv_nsec - timeDist) / div_for_nano);

    // init sem
    sem_init(&mutex, 0, 1);
    sem_init(&empty, 0, buffer.size);
    sem_init(&full, 0, 0);

    // worker threads - consumer
    clock_gettime(CLOCK_REALTIME, &tspec);
    timeDist = tspec.tv_sec;
    for (int idx = 0; idx < pool.size; ++idx)
    {
        if (pthread_create(&pool.threads[idx], NULL, consumer, NULL))
        {
            fprintf(stderr, "[ERROR] pthread_create error\n");
            exit(-1);
        }
    }

    // main thread - producer
    if (pthread_create(&main_thread, NULL, producer, NULL))
    {
        fprintf(stderr, "[ERROR] pthread_create error\n");
        exit(-1);
    }

    // JOIN
    for (int idx = 0; idx < pool.size; ++idx)
    {
        if (pthread_join(pool.threads[idx], NULL))
        {
            fprintf(stderr, "[ERROR] thread_join error\n");
            exit(-1);
        }
    }

    clock_gettime(CLOCK_REALTIME, &tspec);
    fprintf(stdout, "[NOTICE] calculate time : %ld\n", tspec.tv_sec - timeDist);

    // delete
    free(pool.threads);
    free(buffer.index);

    // Data Write
    clock_gettime(CLOCK_REALTIME, &tspec);
    timeDist = tspec.tv_nsec;

    if ((fp = fopen(argv[3], "wb")) == NULL)
    {
        fprintf(stderr, "[ERROR] file write error\n");
        exit(-1);
    }
    fwrite(C_arr, sizeof(float), SIZE * SIZE, fp);
    fclose(fp);

    clock_gettime(CLOCK_REALTIME, &tspec);
    fprintf(stdout, "[NOTICE] Data write time : %f\n", (double)(tspec.tv_nsec - timeDist) / div_for_nano);

    fprintf(stdout, "[SUCCESS] ALL FINISH!\n");

    clock_gettime(CLOCK_REALTIME, &tspec);
    fprintf(stdout, "[NOTICE] total time : %ld\n", tspec.tv_sec - totalDist);

    return 0;
}

int init_pool(int size)
{
    pool.size = size;
    pool.threads = (pthread_t *)malloc(sizeof(pthread_t) * pool.size);
    if (pool.threads == NULL)
        return 0;
    return 1;
}

int init_buffer(int size)
{
    buffer.size = size;
    buffer.fill = 0;
    buffer.use = 0;
    buffer.index = (int *)malloc(sizeof(int) * buffer.size);
    if (buffer.index == NULL)
        return 0;
    return 1;
}

int readFile(char *fname, float (*array)[SIZE])
{
    FILE *fp;

    if ((fp = fopen(fname, "rb")) == NULL)
    {
        fprintf(stderr, "[ERROR] file read error\n");
        exit(-1);
    }

    fread(array, sizeof(float), SIZE * SIZE, fp);
    fclose(fp);
    return 0;
}

void put(int idx)
{
    buffer.index[buffer.fill] = idx;
    buffer.fill = (buffer.fill + 1) % buffer.size;
}

int get()
{
    if (buffer.index[buffer.use] >= SIZE)
        return -1;
    int temp = buffer.index[buffer.use];
    buffer.use = (buffer.use + 1) % buffer.size;

    return temp;
}

void *consumer(void *arg)
{
    int result = 0;
    while (1)
    {
        sem_wait(&full);
        sem_wait(&mutex);
        result = get();
        sem_post(&mutex);
        sem_post(&empty);
        if (result == -1)
            break;

        calc(result);
    }
}

void *producer(void *arg)
{
    for (int idx = 0; idx < SIZE + pool.size; ++idx)
    {
        sem_wait(&empty);
        sem_wait(&mutex);
        put(idx);
        sem_post(&mutex);
        sem_post(&full);
    }
}

void calc(int y)
{
    int x = 0;
    float sum = 0;
    for (int idx = 0; idx < SIZE; ++idx)
    {
        sum = 0;
        for (int jdx = 0; jdx < SIZE; ++jdx)
        {
            sum += A_arr[y][jdx] * B_arr[jdx][idx];
        }
        C_arr[y][idx] = sum;
    }
}