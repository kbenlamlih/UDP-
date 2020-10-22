#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "funcs.h"

int main(int argc, char *argv[])
{

  struct sockaddr_in address;
  int port = 1234;
  int valid = 1;
  int sentsize = 0;
  char msg[MAX_DGRAM_SIZE];
  char blanmsg[MAX_DGRAM_SIZE];
  char* server_ip;
  int bytes_sent = 0;

  fd_set readfs;
  FD_ZERO(&readfs);

  if (argc != 3)
  {
    printf("USAGE: client <server> <port>\n");
    return 0;
  }

  port = atoi(argv[2]);
  server_ip = argv[1];

  if (port < 0)
  {
    printf("Port number must be greater than 0\n");
  }

  //create socket
  int server_desc = socket(AF_INET, SOCK_DGRAM, 0);

  // handle error
  if (server_desc < 0)
  {
    perror("cannot create socket\n");
    return -1;
  }

  setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  printf("IP: %s\n", server_ip);

  if (inet_aton(server_ip, &address.sin_addr) < 0) {
    perror("Error binding address\n");
    return -1;
  }

  int data_port = tcp_over_udp_connect(server_desc, &address);

  if (data_port < 0) {
    printf("Error opening connection\n");
    return -1;
  }

  printf("Data port: %d\n", data_port);

  if (data_port < 1) {
    perror("Error receiving data port\n");
    return -1;
  }

  address.sin_port = htons(data_port);

  socklen_t address_len = sizeof(struct sockaddr);

  while (1) {
    memset(&msg, 0, MAX_DGRAM_SIZE);
    memset(&blanmsg, 0, MAX_DGRAM_SIZE);
    printf("> ");
    fgets(msg,255,stdin);

    if (strcmp(msg, "stop\n") == 0) {
      sentsize = sendto(server_desc, "FIN\n", 4, 0, (struct sockaddr *) &address, address_len);
      break;
    }

    sentsize = safe_send(server_desc, msg, &address, bytes_sent, &readfs, 10);

    if (sentsize < 0)
    {
         perror("An error has happened while writing to socket\n");
    }

    bytes_sent += sentsize;

    fflush(stdin);
  }

  close(server_desc);
  return 0;
}