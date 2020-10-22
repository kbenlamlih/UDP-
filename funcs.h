#ifndef SERVER_FUNCS_H
#define SERVER_FUNCS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <time.h> 

#define MAX_DGRAM_SIZE 1000

float timedifference_msec(struct timespec t0, struct timespec t1);

int tcp_over_udp_connect(int fd, struct sockaddr_in *server);

int tcp_over_udp_accept(int fd, int data_port, struct sockaddr_in *client);

int safe_send(int fd, char* buffer, struct sockaddr_in *client, int seq_number, fd_set *readfs, int max_retries);

int safe_recv(int fd, char* buffer, struct sockaddr_in *client, int seq_number);

#endif