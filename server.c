#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.c"

void doWork(int socketFd);
void handleHttpRequest(int socketFd);
int readHttpRequest(int socketFd);
void sigchldHandler(int sig); 


int main(int argc, char* argv[]) {

  // TODO: This should become a daemon process and handle SIGHUP
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC; // Either IPv4 or 6
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
  
  // Right now we're hard-coding this to connect to localhost
  // port 42285
  struct addrinfo *addrInfo;
  if (getaddrinfo(NULL, "42285", &hints, &addrInfo) != 0) {
    perror("Couldn't get port, I guess");
    exit(1); // TODO: Use the constant here...
  }

  struct sigaction sa;
  sa.sa_flags = SA_RESTART; // This is important because it restarts system calls on failure
  sa.sa_handler = sigchldHandler;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("Error establishing the SIGCHLD handler");
    exit(EXIT_FAILURE);
  }

  // Just use the first addrInfo we have - in theory we would have to loop
  // through, but I can't remember why...
  int socketFd = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
  // TODO: Understand why this is here...
  int socketOption = 1;
  if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) == -1) {
    perror("Error setting socket option");
    exit(EXIT_FAILURE);
  }
  // TODO: Set more socket options...
  if (bind(socketFd, addrInfo->ai_addr, addrInfo->ai_addrlen) != 0) {
    perror("Couldn't bind socket");
    exit(EXIT_FAILURE);
  }

  if (listen(socketFd, SOMAXCONN) == -1) {
    perror("Couldn't listen on socket");
    exit(EXIT_FAILURE);
  }
  doWork(socketFd);
  return 0;
}

void sigchldHandler(int sig) {
  // Get all the children we can, without waiting
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0) {
    if (status != 0) {
      printf("Unexpected child exit code: %d", status);
    }
  }
}

void write404(int clientFd);
void write200(int clientFd);

void doWork(int socketFd) {
  struct sockaddr_storage addrStorage;
  socklen_t addrLength = sizeof(struct sockaddr_storage);
  for (;;) {
    // TODO: Understand why we cast here...
    //int clientFd = accept(socketFd, (struct sockaddr*)&addrStorage, &addrLength);
    int clientFd = accept(socketFd, NULL, NULL);
    if (clientFd == -1) {
      perror("Failed to accept socket");
      continue;
    }
    switch (fork()) {
    case -1:
      perror("Couldn't create child process");
      exit(EXIT_FAILURE);

    case 0:
      // The child should close the listening socket because it doesn't need it
      close(socketFd);
      handleHttpRequest(clientFd);
      _exit(EXIT_SUCCESS);

    default:
      close(clientFd);
      // The parent then continues about their work
    }
  }  
}

void handleHttpRequest(int clientFd) {
  // Could get information about the connecting socket, but for now I'll ignore that...
  int forIndex = readHttpRequest(clientFd);
  sleep(3);
  // TODO: It would be cool to support re-direction
  if (forIndex) {
    write200(clientFd);
  } else {
    write404(clientFd);
  }
  if (close(clientFd) == -1) {
    perror("Error closing");
  }
}

// Right now this just returns a boolean is the request is for index.html.
// If it's not it returns zero so we can return a 404.
int readHttpRequest(int clientFd) {
  int bufferSize = 1000;
  char buffer[1000];
  int returnVal = 0;
  int val = readLine(clientFd, buffer, bufferSize);
  if (val <= 0) {
    return 0;
  }
  // TODO: Be a bit smarter about this. We should be able to just split on the space.
  if (strcmp("GET /index.html HTTP/1.1", buffer) == 0) {
    returnVal = 1; 
  }
  // Ignore the headers and everything else for now. Just read until the end
  while (readLine(clientFd, buffer, bufferSize) != 0);
  return returnVal;
}

// TODO: 200 and 404 are so similar right now it seems like we might want to refactor,
// but I think they'll quickly diverge.
void write404(int clientFd) {
  char* response = "HTTP/1.1 404 Not Found\r\n\r\n";
  // TODO: Add the rest of the headers, etc...
  if (write(clientFd, response, strlen(response)) != strlen(response)) {
    perror("Error writing");
  }
}

// TODO: We should use sendfile here. Maybe this could actually become a true
// file server then...
void write200(int clientFd) {
  // TODO: Add in some headers...
  // TODO: An optimization here would be to use TCP_CORK to avoid sending two packets
  char* responseCode = "HTTP/1.1 200 Success\r\n\r\n";
  if (write(clientFd, responseCode, strlen(responseCode)) != strlen(responseCode)) {
    perror("Error writing");
  }
  // TODO: Provide more flexibility on the file location
  int indexFd = open("test_output/index.html", O_RDONLY);
  if (indexFd == -1) {
    perror("Couldn't find index file");
    // TODO: What to do...??? Here is where a 404 would be appropriate
  }
  // TODO: Make sendfile actually send the right length for the data. Get from stats
  if (sendfile(clientFd, indexFd, 0, 10000) == -1) {
    perror("Error with sendfile");
  }

  if (close(indexFd) == -1) {
    perror("Couldn't close file");
  }
}
