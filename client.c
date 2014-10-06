#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils.c"

// TODO: Switch from writing to stdout to stderr where appropriate
int main(int argc, char* argv[]) {
  struct addrinfo hints;
  struct addrinfo *addrInfo;

  if (argc < 2 || strcmp(argv[1], "--help") == 0) {
    printf("One argument required. The host to connect to.");
    exit(1);
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // TODO: Could also use inet_conenct instead of what's below to make this shorter.
  // It absracts away many of these details.
  // TODO: Support https??? Also just support all kinds of ports...
  // TODO: Actually parse the argument. In particular this doesn't work for anything
  // that has a non '/' request. See below in the GET request and this connection code
  // here.
  if (getaddrinfo(argv[2], "http", &hints, &addrInfo) != 0) {
    perror("Couldn't get address info");
    exit(2);
  }

  struct addrinfo *current;
  int socketFd;
  int count = 0;
  for (current = addrInfo; current != NULL; current = addrInfo->ai_next) {
    count = count + 1;
    if (count > 100) {
      break;
    }
    if (current->ai_socktype != SOCK_STREAM) {
      printf("Socket type isn't stream...");
    }
    socketFd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (socketFd == -1) {
      perror("Socket didn't work");
      continue;
    } 
    if (connect(socketFd, current->ai_addr, current->ai_addrlen) != -1) {
      break;
    }
    close(socketFd);
  }
  if (current == NULL) {
    fprintf(stderr, "Could not connect to any socket");
    exit(1);
  }

  freeaddrinfo(addrInfo);

  char request[1000];
  sprintf(request, "GET / HTTP/1.1\r\nUser-Agent: KyleTest\r\nHost:%s\r\nConnection: close\r\n\r\n", argv[1]);
  if (write(socketFd, request, strlen(request)) != strlen(request)) {
    perror("Error writing");
    exit(3);
  }
  // Sample of what I would do to pipeline requests...
  // If I wanted to handle this particular request I would need to implement redirection handling as it returns a 301
  // Note that to get this to work I would have to remove Connection: close from the header above.
  // This would mean handling more header metadata (either content-length or Transfer-Encoding: chunked.
  // Both of these make the implementation slightly more complicated, though not in a way I'm
  // interested in right now.
  //char* input2 = "GET /news HTTP/1.1\r\nUser-Agent: KyleTest\r\nHost:www.google.com\r\n\r\n";
  //if (write(socketFd, input2, strlen(input2)) != strlen(input2)) {
  //  perror("Ending sending /news");
  //  exit(3);
  //}
  int responseCode = getResponseCode(socketFd);
  if (responseCode < 200 || responseCode > 299) {
    printf("Don't handle non-2xx response code: %d", responseCode);
  }
  // Get headers. Note that there are quite a few details about how to parse headers
  // which I'm ignoring for now. In particular they can span multiple lines.
  int inHeader = 1;
  char line[1000];
  for (;;) {
    int numRead = readLine(socketFd, line, 1000); 
    if (inHeader) {    
      // TODO: Should have a command line flag to print the headers.
      //printf("Header %s\n: ", line);
    }
    if (numRead == 0 && inHeader == 1) {
      // We got an empty line indicating the end of the headers
      inHeader = 0;
    } else if (numRead == 0) {
      // We're at the end...
      break;
    } 
    //printf("%s\n", line);
    if (inHeader == 0) {
      printf("%s", line);
    }
    memset(line, 0, numRead);
  }
  return 0;
}

// Read the first line of the response and parses out the response code.
// Right now this assumes that the response is HTTP/1.1. It doesn't handle
// earlier clients.
// TODO: Just return a three-element array here. That's what we should have...
int getResponseCode(socketFd) {
  int bufferSize = 1000 * sizeof(char);
  char* line = malloc(bufferSize);
  // Get response code line
  int numRead = readLine(socketFd, line, bufferSize);
  if (numRead == 0) {
    printf("Error invalid response");
    exit(0);
  }
  strsep(&line, " ");
  // Return code should be the second element
  // TODO: Do I need to free any memory here?? What's happening to line??
  char* codeStr = strsep(&line, " ");
  int returnCode = strtol(codeStr, (char**) NULL, 10);
  return returnCode;
}
