#include "sys/select.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define MAX_BUFFER 1024

struct pid_info {
    int pid;
    char name[100];
    int utime;
    int stime;
};

struct top_two_pids {
    struct pid_info pid1;
    struct pid_info pid2;
};

struct top_two_pids get_top_two_cpu_processes()
{
    DIR *proc_folder = opendir("/proc");
    if (proc_folder < 0)
    {
        perror("open failed");
        exit(1);
    }
    struct dirent *entry;
    int pid_count = 0;

    struct pid_info pid1 = {0, "", 0, 0};
    struct pid_info pid2 = {0, "", 0, 0};
    struct top_two_pids top_two;
    while ((entry = readdir(proc_folder)) != NULL)
    {

        if (isdigit(entry->d_name[0]))
        {
            int pid = atoi(entry->d_name);
            if (pid > 0)
            {
                char path[100];
                sprintf(path, "/proc/%d/stat", pid);
                FILE *file = fopen(path, "r");
                if (file == NULL)
                {
                    perror("fopen failed");
                    exit(1);
                }
                struct pid_info pid;
                for (int i = 0; i < 22; i++)
                {   
                    if (i == 0)
                        fscanf(file, "%d", &pid.pid);
                    else if (i == 1)
                        fscanf(file, "%s", pid.name);
                    else if (i == 13)
                        fscanf(file, "%d", &pid.utime);
                    else if (i == 14)
                        fscanf(file, "%d", &pid.stime);
                    else
                        fscanf(file, "%*s");
                }
                if ((pid1.utime + pid1.stime) < (pid.utime + pid.stime))
                {
                    pid2 = pid1;
                    pid1 = pid;
                }
                else if ((pid2.utime + pid2.stime) < (pid.utime + pid.stime))
                {
                    pid2 = pid;
                }
                fclose(file);
                pid_count++;
            }
        }
    }

    closedir(proc_folder);
    top_two.pid1 = pid1;
    top_two.pid2 = pid2;
    
    return top_two;
    
}

void handle_clients_with_select(int server_fd) {
    int client_sockets[MAX_CLIENTS] = {0}; 
    fd_set readfds;
    char buffer[MAX_BUFFER];
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int max_sd, sd, new_socket, valread;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error");
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("Accept failed");
                exit(1);
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];

            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, MAX_BUFFER);
                if (valread == 0) {
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    struct top_two_pids top_two = get_top_two_cpu_processes();
                    struct pid_info pid1 = top_two.pid1;
                    struct pid_info pid2 = top_two.pid2;

                    char response[MAX_BUFFER];
                    sprintf(response,
                            "\nTop 2 CPU processes:\n"
                            "pid1: %d, name: %s, user time: %d, kernel time: %d\n"
                            "pid2: %d, name: %s, user time: %d, kernel time: %d\n",
                            pid1.pid, pid1.name, pid1.utime, pid1.stime,
                            pid2.pid, pid2.name, pid2.utime, pid2.stime);
                    
                    send(sd, response, strlen(response), 0);
                }
            }
        }
    }
}

int main() {
    pid_t pid = getpid();
    char command[100];
    sprintf(command, "taskset -cp 0 %d", pid);
    if (system(command) < 0) {
        perror("taskset failed");
        exit(1);
    }

    int server_fd;
    struct sockaddr_in server_addr;
    server_addr = (struct sockaddr_in){AF_INET, htons(PORT), INADDR_ANY};
    socklen_t addrlen = sizeof(server_addr);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, addrlen) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(1);
    }

    handle_clients_with_select(server_fd);

    close(server_fd);

    return 0;
}
