#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <map>
#include <iostream>

#define PORT "9034"

enum SOCK_STATUS {
  CONNECTING = 0, // client is connecting => is currently choosing a username
  CONNECTED = 1  // client is ready to send messages
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void)
{
  fd_set master;                      // master file descriptor list
  fd_set read_fds;                    // temp file descriptor list for select()
  int fdmax;                          // maximum file descriptor number
  int listener;                       // listening socket descriptor
  int newfd;                          // newly accept()ed socket descriptor
  std::map<int,SOCK_STATUS> fd_stat;
  std::map<int,std::string> fd_username;
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;
  char buf[256]; // buffer for client data
  int nbytes;
  char remoteIP[INET6_ADDRSTRLEN];
  int yes = 1; // for setsockopt() SO_REUSEADDR, below
  int i, j, rv;

  struct addrinfo hints, *ai, *p;

  FD_ZERO(&master); // clear the master and temp sets
  FD_ZERO(&read_fds);

  // get us a socket and bind it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
  {
    fprintf(stderr, "server: %s\n", gai_strerror(rv));
    exit(1);
  }
  for (p = ai; p != NULL; p = p->ai_next)
  {
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0)
    {
      continue;
    }
    // lose the pesky "address already in use" error message
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
    {
      close(listener);
      continue;
    }
    break;
  }
  if (p == NULL)
  {
    // if we got here, it means we didn't get bound
    fprintf(stderr, "server: failed to bind\n");
    exit(2);
  }
  freeaddrinfo(ai); // all done with this
  // listen
  if (listen(listener, 10) == -1)
  {
    perror("listen");
    exit(3);
  }
  // add the listener to the master set
  FD_SET(listener, &master);
  // keep track of the biggest file descriptor
  fdmax = listener; // so far, it's this one

  // main loop
  for (;;)
  {
    read_fds = master; // copy it
    // select returns when a file descriptor is ready to be read from
    // and updates read_fds with the fds which are now ready
    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
    {
      perror("select");
      exit(4);
    }
    // run through the existing connections looking for data to read
    for (i = 0; i <= fdmax; i++)
    {
      // select() modified read_fds to signal which fds are now ready
      // so we check if this fd is included thus in the "ready state"
      if (FD_ISSET(i, &read_fds))
      {
        // we got a fd which is ready for reading!
        // is it the listener socket?
        if (i == listener)
        {
          // yes, it is => handle new connections
          addrlen = sizeof remoteaddr;
          newfd = accept(listener,
                         (struct sockaddr *)&remoteaddr,
                         &addrlen);
          if (newfd == -1)
          {
            perror("accept");
          }
          else
          {
            FD_SET(newfd, &master); // add to master set
            fd_stat.insert(std::pair<int, SOCK_STATUS>(newfd, SOCK_STATUS::CONNECTING));
            if (newfd > fdmax)
            {
              fdmax = newfd; // keep track of the max
            }
            printf("server: new connection from %s on "
                   "socket %d\n",
                   inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr *)&remoteaddr),remoteIP, INET6_ADDRSTRLEN),
                   newfd);
             char const *userprompt = "Hello! Please choose a username:";
             if(send(newfd, userprompt, strlen(userprompt), 0) == -1)
             {
               perror("send");
             }
          }
        }
        else
        {
          // handle data from a client
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
          {
            // got error or connection closed by client
            if (nbytes == 0)
            {
              // connection closed
              printf("server: socket %d hung up\n", i);
            }
            else
            {
              perror("recv");
            }
            close(i);           // bye!
            FD_CLR(i, &master); // remove from master set
          }
          else
          {
            printf("Data received from client %d: %s\n", i, buf);
            if(fd_stat.count(i))
            {
              switch (fd_stat.at(i)) {
                case SOCK_STATUS::CONNECTING :
                  // we got the chosen username from the client
                  fd_username.insert(std::pair<int,std::string>(i,std::string(buf)));
                  fd_stat.at(i) = SOCK_STATUS::CONNECTED;
                  std::cout << "username " << i << ": " << fd_username.at(i) << std::endl;
                  break;
                case SOCK_STATUS::CONNECTED :
                  // we got data from a connected client!
                  // send to every connected client!
                  for (j = 0; j <= fdmax; j++)
                  {
                    if (FD_ISSET(j, &master))
                    {
                      // except the listener and ourselves
                      // and not fully connected clients
                      if (j != listener && j != i && fd_stat.at(j) != SOCK_STATUS::CONNECTING)
                      {
                        char message[256];
                        std::string username = fd_username.at(i) + ": ";
                        strcpy(message, username.c_str());
                        strcat(message, buf);
                        std::cout << j << " " << message << std::endl;
                        if (send(j, message, strlen(message), 0) == -1)
                        {
                          perror("send");
                        }
                      }
                    }
                  }
                break;
                default :
                  std::cerr << "ERR: Undefined status of " << j << "!" << std::endl;
                  break;
              }
            }
            else
            {
              // fd could not be found in map. Status of fd unknown. Error!
              std::cerr << "ERR: Socket status of " << j << " unknown!" << std::endl;
            }
            // clear read buffer
            memset(&buf, 0, sizeof buf);
          }
        } // END handle data from client
      }   // END got new incoming connection
    }     // END looping through file descriptors
  }       // END for(;;)
  return 0;
}
