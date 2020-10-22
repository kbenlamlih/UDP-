#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>

#include "funcs.h"

int main(int argc, char *argv[])
{
  printf("Henlo\n");
  struct sockaddr_in server_address, client;
  int optval = 1; // To set SO_REUSEADDR to 1
  int port = 0;
  int data_port = 0;
  int recvsize = 0;
  char buffer[MAX_DGRAM_SIZE];
  pid_t fork_pid = 0;
  int sequence_number = 1; // We're waiting for the 1st byte

  if (argc != 3)
  {
    printf("USAGE: server <control_port> <data_port>\n");
    return 0;
  }

  port = atoi(argv[1]);
  data_port = atoi(argv[2]);

  if (port < 0 || data_port < 0)
  {
    printf("Port number must be greater than 0\n");
  }

  //create socket
  int server_udp = socket(AF_INET, SOCK_DGRAM, 0);
  setsockopt(server_udp, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

  //handle error
  if (server_udp < 0)
  {
    perror("Cannot create socket\n");
    return -1;
  }

  printf("Binding address...\n");

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_udp, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
  {
    perror("UDP Bind failed\n");
    close(server_udp);
    return -1;
  }

  while (1)
  {
    printf("Accepting connections...\n");

    // Serveur UDP
    int portnumber = tcp_over_udp_accept(server_udp, data_port, &client);

    if (portnumber == 0)
    {
      printf("Everything is fine my friend\n");
    }

    data_port++;

    // Forking to serve new clients

    fork_pid = fork();

    if (fork_pid < 0)
    {
      perror("Error forking process\n");
      return -1;
    }

    if (fork_pid == 0)
    {
      // Child process here
      close(server_udp);

      server_address.sin_port = htons(portnumber);
      server_udp = socket(AF_INET, SOCK_DGRAM, 0);
      setsockopt(server_udp, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

      if (bind(server_udp, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
      {
        perror("UDP Data connection bind failed\n");
        close(server_udp);
        return -1;
      }
      printf("New socket created on port %d\n", portnumber);

      // Starting here: we are connected to a client
      while (1)
      {
        memset(&buffer, 0, MAX_DGRAM_SIZE);
        printf("Waiting for new message\n");
        recvsize = safe_recv(server_udp, buffer, &client, sequence_number);

        if (strcmp(buffer, "FIN\n") == 0)
        {
          printf("FIN received from client. Closing connection\n");
          break;
        }

        if (recvsize < 0) {
          perror("Unable to receive message from client\n");
        }

        sequence_number += recvsize;

        printf("Client(%d)>%s\n", recvsize, buffer);
      }
      close(server_udp);
      exit(0);
    }
    else
    {
      printf("Child process started at PID %d\n", fork_pid);
    }
  }

  printf("Closing UDP server\n");
  close(server_udp);
  return 0;
}
