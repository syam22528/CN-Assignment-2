#include "sys/select.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "pthread.h"
#include "sys/epoll.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void *client_connection(void *arg)
{
    int client_sock;
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(0);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0){
        perror("Invalid address");
        exit(0);
    }
    

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(0);
    }

    
    char buffer[1024] = "what are the top two cpu consuming processes?";
    send(client_sock, buffer, strlen(buffer), 0);

    int fd = client_sock;
    buffer[0] = '\0';
    int valread = read(fd, buffer, 1024);
    buffer[valread] = '\0';
    if (valread == 0)
    {
        printf("Server disconnected\n");
        exit(0);
    }
    else
    {
        printf("Server: %s\n", buffer);
    }
    close(fd);
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[])
{   

    pid_t pid = getpid();
    char command[100];
    sprintf(command, "taskset -cp 1 %d", pid);
    if (system(command) < 0)
    {
        perror("taskset failed");
        exit(1);
    }

    int n = atoi(argv[1]);
    pthread_t *threads = (pthread_t *)malloc(n * sizeof(pthread_t));

    int client_ids[n];
    for (int i = 0; i < n; i++)
    {
        pthread_create(&threads[i], NULL, client_connection, &client_ids[i]);
    }
    for (int i = 0; i < n; i++)
    {
        pthread_join(threads[i], NULL);
    }
}