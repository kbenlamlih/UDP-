#include "funcs.h"

float timedifference_msec(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_nsec - t0.tv_nsec) / 1000000.0f;
}

int tcp_over_udp_connect(int fd, struct sockaddr_in *server)
{
  char msg[32];
  int n;
  socklen_t server_len = sizeof(struct sockaddr);
  printf("Sending SYN\n");

  n = sendto(fd, "SYN\n", 4, 0, (struct sockaddr *)server, server_len);
  if (n < 0)
  {
    perror("Error sending SYN packet\n");
    return -1;
  }

  printf("SYN sent\n");

  memset(msg, 0, 32);
  n = recvfrom(fd, &msg, 32, 0, (struct sockaddr *)server, &server_len);

  if (n < 0)
  {
    perror("Error receiving SYN-ACK\n");
    return -1;
  }

  if (strncmp(msg, "SYN-ACK", 7) != 0)
  {
    perror("SYN-ACK not received\n");
    return -1;
  }

  printf("SYN-ACK received\n");

  char data_port[6];
  memset(data_port, 0, 6);

  strncpy(data_port, msg + 8, 6);

  printf("Sending ACK\n");
  n = sendto(fd, "ACK\n", 4, 0, (struct sockaddr *)server, server_len);
  if (n < 0)
  {
    perror("Error sending ACK packet\n");
    return -1;
  }

  printf("ACK sent\n");
  return atoi(data_port);
}

int tcp_over_udp_accept(int fd, int data_port, struct sockaddr_in *client)
{
  printf("Waiting for connection\n");
  char buffer[32];
  int n;
  socklen_t client_size = sizeof(struct sockaddr);

  char SYN_ACK[16];
  sprintf(SYN_ACK, "SYN-ACK-%d\n", data_port);

  memset(buffer, 0, 32);
  n = recvfrom(fd, &buffer, 32, 0, (struct sockaddr *)client, &client_size);

  if (strcmp(buffer, "SYN\n") != 0)
  {
    perror("Connection must start with SYN\n");
    return -1;
  }
  printf("SYN received\n");

  n = sendto(fd, SYN_ACK, strlen(SYN_ACK), 0, (struct sockaddr *)client, client_size);
  if (n < 0)
  {
    perror("Unable to send SYN-ACK\n");
    return -1;
  }
  printf("SYN-ACK sent\n");

  memset(buffer, 0, 32);

  n = recvfrom(fd, &buffer, 32, 0, (struct sockaddr *)client, &client_size);
  if (strcmp(buffer, "ACK\n") != 0)
  {
    perror("ACK not received\n");
    return -1;
  }
  printf("ACK received. Connected\n");

  return data_port;
}

int safe_send(int fd, char *buffer, struct sockaddr_in *client, int seq_number, fd_set *readfs, int max_retries)
{
  socklen_t client_size = sizeof(struct sockaddr);
  char ack_buffer[12];
  int ack_msglen;
  char ack_number[7];
  int next_byte_expected = 0;
  int next_byte_to_send = 0;
  float rtt = 0;
  struct timespec sent_t, received_t;
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 1000*1000;
  int n_retries = 0;
  int msglen = 0;

  clock_gettime(CLOCK_MONOTONIC_RAW, &sent_t);

  while (n_retries < max_retries || n_retries < 1) {
    FD_ZERO(readfs);
    FD_SET(fd, readfs);

    timeout.tv_usec = 1000*1000;

    printf("Sending packet. Attempt #%d\n", n_retries + 1);

    msglen = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)client, client_size);

    if (msglen < 0)
    {
      perror("Error sending message\n");
    }
    printf("%d bytes sent. Waiting for ACK\n", msglen);

    if (select(fd + 1, readfs, NULL, NULL, &timeout) < 0) {
      fprintf(stderr, "An unknown error happened\n");
    }

    if (FD_ISSET(fd, readfs)) {
      ack_msglen = recvfrom(fd, ack_buffer, 12, 0, (struct sockaddr *)client, &client_size);
      break; // We have received an ACK
    } else {
      fprintf(stderr, "ACK not received in time. Timeout %ld : %ld\n", timeout.tv_sec, timeout.tv_usec);
    }

    n_retries++;
  }

  if (n_retries >= max_retries) {
    fprintf(stderr, "Could not send packet in %d attempts.\n", n_retries);
    return -1;
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &received_t);

  rtt = timedifference_msec(sent_t, received_t);

  printf("ACK received in %.2f milliseconds.\n", rtt);

  if (ack_msglen < 0)
  {
    perror("Error receiving ACK\n");
    return -1;
  }

  if (strncmp(ack_buffer, "ACK-", 4) != 0)
  {
    perror("Bad ACK format\n");
    return -1;
  }

  memset(ack_number, 0, 7);

  strncpy(ack_number, ack_buffer + 4, 6);

  next_byte_expected = atoi(ack_number);

  printf("ACK received. Next byte expected from other side: %d\n", next_byte_expected);

  next_byte_to_send = seq_number + msglen + 1;

  if (next_byte_expected > next_byte_to_send)
  {
    perror("ACK Error: Servers has received more bytes than we have sent\n");
    return -1;
  }
  else if (next_byte_expected < next_byte_to_send)
  {
    perror("ACK Error: Server hasn't received all of our information\n");
    return -1;
  }

  return msglen;
}

int safe_recv(int fd, char *buffer, struct sockaddr_in *client, int seq_number)
{
  socklen_t client_size = sizeof(struct sockaddr);

  int msglen = recvfrom(fd, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr *)client, &client_size);

  if (msglen < 0)
  {
    perror("Error receiving message\n");
    return -1;
  }
  printf("(safe_recv) Message received (%ld): %s\n", strlen(buffer), buffer);

  char ack[12];
  memset(ack, 0, 12);

  int new_seq_number = seq_number + msglen;

  sprintf(ack, "ACK-%06d\n", new_seq_number);

  printf("Next byte expected: %d\n", new_seq_number);

  int ack_msglen = sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)client, client_size);

  if (ack_msglen < 0)
  {
    perror("Error sending ACK\n");
    return -1;
  }

  return msglen;
}
