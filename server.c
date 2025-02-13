#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server_util.h"
#include "common.h"
#include "timer.h"

// #define MAX_CLIENTS 100
#define BUFFER_SIZE 256             // MAX SIZE OF MESSAGE 
#define STRING_SIZE 100             // MAX STRING SIZE IN ARRAY
#define PORT 8000                   // DEFAULT PORT 
#define SAVE_POINT 1000             // SAVES EVERY 1000 REQUESTS
#define DEFUALT_NUM_STRINGS 100


int server_fd;
int NUM_STRINGS;
char **array;
ReadWriteLock rw;
ThreadPool *pool;
Analytics *anyt;

// void handle_sigint(int sig) {
//     printf("\nServer shutting down...\n");
//     close(server_fd); // Close the server socket
//     free(anyt->time);
//     free(anyt);
//     deinitRW(&rw);
//     destroyThreadPool(pool);
//     exit(EXIT_SUCCESS);
// }

int main(int argc, char *argv[]) {
    // signal(SIGINT, handle_sigint);
    // init Read Write Locks 
    initRW(&rw);
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <arraySize> <host> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    //init analytics struct 
    anyt = malloc(sizeof(Analytics));
    anyt->time = malloc(sizeof(double)*SAVE_POINT);
    anyt->length = 0;

    // Init string array
    NUM_STRINGS = atoi(argv[1]);
    char *server_ip = argv[2];
    int server_port = strtol(argv[3], NULL, 10);

    if (NUM_STRINGS <= 0) {
        if(DEBUG){
            fprintf(stderr, "Invalid array size.\n");
        }
        exit(EXIT_FAILURE);
    }
    array = malloc(NUM_STRINGS * sizeof(char *));
    
    // format initial values for the array
    for (int i = 0; i < NUM_STRINGS; i++) {
        array[i] = malloc(STRING_SIZE);
        snprintf(array[i], STRING_SIZE, "String %d: the initial value”", i);
    }
    //start thread pool
    pool = initThreadPool(COM_NUM_REQUEST);
    startThreadPool(pool);

    // start server connection 
    int client_fd;
    struct sockaddr_in server_addr, client_addr; //IPV4
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    // server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, COM_NUM_REQUEST) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on %s:%d \n",server_ip, server_port);

    int requestCount = 0;
    while (1) { // wait for connections 
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept() failed");
            fprintf(stderr, "Failed to accept connection on port %d\n", server_port);
            continue;
        }
        requestCount ++;
        if(DEBUG){
            printf("New client connected! Submitting Job to pool...\n");
        }
        int *newClientSock = malloc(sizeof(int));
        *newClientSock = client_fd;
        Task *task = malloc(sizeof(Task));
        task->clientSock = newClientSock; // Pass the client socket pointer to the task

        addTaskToPool(pool, task);

        if (requestCount%SAVE_POINT == 0){
            pthread_mutex_lock(&(pool->lock));
            saveTimes(anyt->time, anyt->length);
            memset(anyt->time, 0, sizeof(double)*SAVE_POINT);
            pthread_mutex_unlock(&(pool->lock));
        }
    }

    free(anyt->time);
    free(anyt);
    deinitRW(&rw);
    destroyThreadPool(pool);
    close(server_fd);
    return 0;
}

void *threadFunction(void *arg){
    while (1) {
            pthread_mutex_lock(&pool->lock);
            // Wait if there are no tasks in the queue
            while (pool->task_count == 0 && !pool->terminate) {
                if(DEBUG){
                    printf("Sleeping For Task.\n");
                }    
                pthread_cond_wait(&pool->isJob, &pool->lock);
            }
            if(DEBUG){
                printf("Picked up task\n");
            }

            // If the pool is terminating, exit
            if (pool->terminate) {
                pthread_mutex_unlock(&pool->lock);
                break;
            }

            // Get the next task from the queue FIFO
            Task *task = getTaskFromPool(pool); 

            pthread_mutex_unlock(&pool->lock);
            
            // Execute the task (handle client)
            handle_client(task->clientSock);

            free(task); // Free the task memory once it is completed
        }

        return NULL;
}

void handle_client(void *client_socket) {
    int client_fd = *((int *)client_socket);
    free(client_socket);
    char message[BUFFER_SIZE];
    
    memset(message, 0, BUFFER_SIZE);
    int bytes_read = recv(client_fd, message, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        if(DEBUG){
            printf("Client disconnected.\n");
        }    
        close(client_fd);
        return;
    }
    message[bytes_read] = '\0';
    ClientRequest request;
    if(ParseMsg(message, &request) !=0){
        if(DEBUG){
            printf("Failed to parse the messge.\n");
        }    
    }
    char response[STRING_SIZE];
    double start, end;
    GET_TIME(start)
    if (request.is_read) {
        readlock(&rw);
        getContent(response, request.pos, array);
        if (DEBUG)
        {
            printf("%s\n", response);
        }
        
        rwUnlock(&rw);   
    } else {
        writeLock(&rw);
        setContent(request.msg, request.pos, array);
        rwUnlock(&rw);   
        strcpy(response, "Write Complete!\n");
    }
    GET_TIME(end);

    pthread_mutex_lock(&(pool->lock));
    anyt->time[anyt->length] = (end-start);
    anyt->length++;
    anyt->length %= SAVE_POINT;
    pthread_mutex_unlock(&(pool->lock));

    send(client_fd, response, strlen(response), 0);
    close(client_fd);
}


