#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <thread>

#define PORT "9034"     // port the client will connect to
#define MAXDATASIZE 256 // max number of bytes we can get and send at once

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Initializes connection with server
int setup_connection(int *sockfd, const char *address, const char *port) {
  struct addrinfo hints, *servinfo, *p;
  int rv;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  char s[INET6_ADDRSTRLEN];

  if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }
    if (connect(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(*sockfd);
      perror("client: connect");
      continue;
    }
    // no errors occured - we have found a socket ...
    break;
  }
  // ... or did run through all results
  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
  printf("client: connecting to %s\n", s);
  freeaddrinfo(servinfo); // all done with this structure
  return 0;
}

// read from socket and print to standard output
void read_from_server(int socket) {
  while(1) {
    char buf[MAXDATASIZE];
    int numbytes;
    if((numbytes = recv(socket, buf, MAXDATASIZE-1, 0)) == -1) {
      perror("recv");
    }
    if(numbytes == 0) {
      printf("server closed connection.\n");
      exit(0);
    }
    buf[numbytes] = '\0';
    printf("%s\n", buf);
  }
}

int main(int argc, char *argv[])
{
  int sockfd;

  if (argc != 2) {
    fprintf(stderr, "usage: client hostname\n");
    exit(EXIT_FAILURE);
  }

  if(setup_connection(&sockfd, argv[1], PORT) != 0) {
    printf("Could not connect to server.\n");
    exit(EXIT_FAILURE);
  };

  // Start thread for receiving messages from server
  std::thread t1(read_from_server, sockfd);

  // Read from standard input and send to server
  for(;;) {
    std::string userinput;
    std::getline(std::cin, userinput);
    const char* cinput = userinput.c_str();
    if (send(sockfd, cinput, strlen(cinput), 0) == -1) perror("send");
  }
  // Currently, program will never be terminated by this since we enter endless loop
  exit(EXIT_SUCCESS);
}
