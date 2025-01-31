#ifndef SERVERUTIL_H_
#define SERVERUTIL_H_
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG 0

typedef struct{
    int readers; 
    int writer;
    pthread_cond_t readersProceed;
    pthread_cond_t writersProceed;
    int pendingWriters;
    pthread_mutex_t lock;
} ReadWriteLock;

typedef struct{
    double *time;
    int length;
} Analytics;

void* threadFunction(void* arg);
void handle_client(void *client_socket);

void initRW(ReadWriteLock *rw){
    rw->readers = 0;
    rw->writer  = 0;
    rw->pendingWriters = 0;
    pthread_cond_init(&(rw->readersProceed), NULL);
    pthread_cond_init(&(rw->writersProceed), NULL);
    pthread_mutex_init(&(rw->lock), NULL);
}

void deinitRW(ReadWriteLock *rw){
    rw->readers = 0;
    rw->writer  = 0;
    rw->pendingWriters = 0;
    pthread_cond_destroy(&(rw->readersProceed));
    pthread_cond_destroy(&(rw->writersProceed));
    pthread_mutex_destroy(&(rw->lock));
}

void readlock(ReadWriteLock *rw){
    // if there is a lock or pending readers then wait, otherwise obtain "lock"
    pthread_mutex_lock(&(rw->lock));
    while (rw->writer > 0 || rw->pendingWriters > 0){
        if(DEBUG){
            printf("Waiting for Read Lock.\n");
        }  
        pthread_cond_wait(&(rw->readersProceed), &(rw->lock));
    }
    rw->readers++;
    if(DEBUG){
        printf("Obtained Read Lock.\n");
    }   
    pthread_mutex_unlock(&(rw->lock));

}

void writeLock(ReadWriteLock *rw){
    // if there is readers or an active writer then wai, otherwise obtain "lock"
    pthread_mutex_lock(&(rw->lock));
    while(rw->readers >0 || rw->writer>0){
        rw->pendingWriters++;
        pthread_cond_wait(&(rw->writersProceed), &(rw->lock));
        rw->pendingWriters--;
    }
    rw->writer++;
    pthread_mutex_unlock(&(rw->lock));
    if(DEBUG){
        printf("Obtained Write Lock.\n");
    }    
}

void rwUnlock( ReadWriteLock *rw){
    pthread_mutex_lock(&(rw->lock));
    if(rw->writer > 0){
        rw->writer = 0;
        if(DEBUG){
            printf("Releasing Writer\n");
        }  
    }
    else if (rw->readers > 0){
        rw->readers--;
        if(DEBUG){
            printf("Releasing Reader\n");
        }  
    }

    if(rw->readers>0){
        pthread_cond_broadcast(&(rw->readersProceed));
    }
    else if (rw->pendingWriters>0){
        pthread_cond_signal(&(rw->writersProceed));
    }
    else if (rw->pendingWriters == 0){
        pthread_cond_broadcast(&(rw->readersProceed));
    }
    pthread_mutex_unlock(&(rw->lock));
}

typedef struct Task {
    void* clientSock;
    struct Task *nextTask;
} Task;

typedef struct {
    pthread_t *threads;          // Array of worker threads
    Task* lastIn, *firstOut;
    int numThreads;
    int task_count;              // Number of tasks in the queue
    int terminate; 
    pthread_mutex_t lock;  // Mutex for locking the task queue
    pthread_cond_t isJob;   // Condition variable for task availability
} ThreadPool;

ThreadPool* initThreadPool(int numThreads){
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));  // Allocate memory for ThreadPool
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*numThreads);
    pool->task_count = 0;      // Initialize task count
    pool->terminate = 0;       // Initialize terminate flag to 0
    pool->numThreads = numThreads;
    pool->firstOut = NULL;
    pool->lastIn = NULL;
    pthread_mutex_init(&pool->lock, NULL);   // Initialize mutex
    pthread_cond_init(&pool->isJob, NULL);  // Initialize condition variable
    return pool;
}

void destroyThreadPool(ThreadPool *pool) {
    if (pool) {
        free(pool->threads);  // Free the threads array
        Task *task = pool->firstOut;
        Task *next = NULL;
        while(task != NULL){
            next = task->nextTask;
            free(task);
            task = next;
        }
        pthread_mutex_destroy(&pool->lock);  // Destroy mutex
        pthread_cond_destroy(&pool->isJob);  // Destroy condition variable
        free(pool);  // Free the ThreadPool structure itself
    }
}

void addTaskToPool(ThreadPool *pool, Task *task) {
    task->nextTask = NULL;
    pthread_mutex_lock(&pool->lock);  
    // If the queue is empty, new task becomes first and last 
    if (pool->task_count == 0) {
        pool->firstOut = task;
    } else {
        pool->lastIn->nextTask= task;
    }
    pool->lastIn = task;  
    pool->task_count++;   
    
    pthread_cond_signal(&pool->isJob);  
    pthread_mutex_unlock(&pool->lock);  
}

Task* getTaskFromPool(ThreadPool *pool) {
    
    Task *task = pool->firstOut;
    pool->firstOut = task->nextTask;
    
    if (pool->firstOut == NULL) {
        pool->lastIn = NULL; 
    }
    
    pool->task_count--; 
    return task;  // Return the task
}

// Start all threads in the pool
void startThreadPool(ThreadPool *pool) {
    for (int i = 0; i < pool->numThreads; i++) {
        if (pthread_create(&pool->threads[i], NULL, threadFunction, pool) != 0) {
            perror("Failed to create thread");
        }

    }
    if(DEBUG){
        printf("\tStarted Thread Pool!\n");
    }
}

#endif