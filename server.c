#include "proxy_parse.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>

typedef struct cache_element cache_element;

#define MAX_CLIENTS 10

struct cache_element {
    char *data;
    int len;
    char *url;
    time_t lru_time_track;
    cache_element *next;
};

cache_element *find(char *url);

int add_cache_element(char *data, int size, char *url);

void remove_cache_element();

int port_number = 8080;

int proxy_socketId;
pthread_t tid[MAX_CLIENTS];

sem_t semaphore;
pthread_mutex_t lock;

cache_element *head;
int cache_size;

int main(int argc, char *argv[]){
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if(argv == 2){
        port_number = atoi(argv[1]);
    }
    else{
        printf("Too few arguments\n");
        exit(1);
    }

    printf("Starting proxy server on port %d\n", port_number);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if(proxy_socketId < 0){
        perror("Error creating socket");
        exit(1);
    }

    int reuse = 1;
    if(setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0){
        perror("Error setting socket options");
        exit(1);
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_number);

    if(bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Error binding socket");
        exit(1);
    }
    printf("Binding on port %d successful\n", port_number);

    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if(listen_status < 0){
        perror("Error listening on socket");
        exit(1);
    }

    int i=0;
    int connectedSocketId[MAX_CLIENTS];
    while(1){
        bzero((char *)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, &client_len);
        if(client_socketId < 0){
            perror("Error accepting connection");
            continue;
        }
        else{
            connectedSocketId[i] = client_socketId;
        }

        struct sockaddr_in *client_ip = (struct sockaddr_in *)&client_addr;
        struct in_addr client_in_addr = client_ip->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_in_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number : %d and IP address : %s\n", ntohs(client_ip->sin_port), str);

        pthread_create(&tid[i], NULL, handle_client, (void *)&connectedSocketId[i]);
        i++;
    }
    close(proxy_socketId);
    return 0;

}

