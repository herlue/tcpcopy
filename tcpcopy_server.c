#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT       3000
#define NQUEUE     5
#define BUFFERSIZE 2048
#define FNBUFSIZ   256

struct meta {
  size_t fnlength;
  char   filename[FNBUFSIZ];
  size_t filesize;
};

void close_errmsg(int);
void close_socket_errmsg(int);
ssize_t recvline(int, char*, size_t);
ssize_t writeall(int, const char*, size_t);
int parsemeta(const char*, struct meta*);
ssize_t recvwrall(int, int, char*, size_t, size_t);

int main(void) {
  int serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverfd == -1) {
    fprintf(stderr, "cannot open socket: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  struct sockaddr_in addr;
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;
  socklen_t addrlen = sizeof(addr);

  if (bind(serverfd, (struct sockaddr*) &addr, addrlen)) {
    fprintf(stderr, "cannot bind socket: %s\n", strerror(errno));
    close_errmsg(serverfd);
    exit(EXIT_FAILURE);
  }

  if (listen(serverfd, NQUEUE)) {
    fprintf(stderr, "cannot listen: %s\n", strerror(errno));
    close_errmsg(serverfd);
    exit(EXIT_FAILURE);
  }

  const int optval = 1;
  if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
    perror("setsockopt()");

  int clientfd, filefd, nfiles;
  char buffer[BUFFERSIZE];
  ssize_t nread;
  mode_t mode = 0664;
  struct meta metadata;

  while (1) {
    // accept a new client connection
    clientfd = accept(serverfd, (struct sockaddr*) &addr, &addrlen);
    if (clientfd == -1) {
      fprintf(stderr, "cannot accept: %s\n", strerror(errno));
      continue;
    }
    // retrieve number of files from client
    if (recvline(clientfd, buffer, sizeof(buffer)) <= 0) {
      fprintf(stderr, "cannot retrieve number of files from client\n");
      close_socket_errmsg(clientfd);
      continue;
    }
    // get number of files
    if (!(nfiles = atoi(buffer))) {
      fprintf(stderr, "cannot parse number of files\n");
      close_socket_errmsg(clientfd);
      continue;
    }
    // for each file
    for (int i = 0; i < nfiles; i++) {
      // get meta information for each file
      if (recvline(clientfd, buffer, sizeof(buffer)) <= 0) {
        fprintf(stderr, "cannot get meta data from client [no %2d]\n", i);
        break;
      }
      if (parsemeta(buffer, &metadata)) {
        fprintf(stderr, "cannot parse meta data\n");
        break;
      }      
      // add ./files as the path prefix
      snprintf(buffer, sizeof(buffer), "./files/%s", metadata.filename);
      filefd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, mode);
      if (filefd == -1) {
        fprintf(stderr, "cannot open file \"%s\": %s\n", buffer, strerror(errno));
        break;
      }
      // read the file's contents from the client
      if (recvwrall(clientfd, filefd, buffer, sizeof(buffer), metadata.filesize) <= 0) {
        fprintf(stderr, "failed to read/write some data (file = %s)\n", metadata.filename);
        break;
      }
      close_errmsg(filefd);
    }
    close_socket_errmsg(clientfd);
  }
}

void close_errmsg(int fd) {
  if (close(fd) == -1)
    fprintf(stderr, "cannot close file (descriptor = %d): %s\n", fd, strerror(errno));
}

void close_socket_errmsg(int sockfd) {
  if (close(sockfd) == -1)
    fprintf(stderr, "cannot close socket (descriptor = %d): %s\n", sockfd, strerror(errno));
}

ssize_t recvline(int sockfd, char* buffer, size_t buffersize) {
  int  i, n;
  char c;

  for (i = 0; i < buffersize - 1; i++) {
    n = recv(sockfd, &c, 1, 0);
    if (n <= 0)
      return n;

    if (c == '\n')
      break;
    
    buffer[i] = c;
  }
  
  buffer[i] = '\0';
  return i;
}

ssize_t writeall(int fd, const char* buffer, size_t length) {
  ssize_t nwrite;
  size_t  nleft = length;

  while (nleft > 0) {
    nwrite = write(fd, buffer, nleft);
    if (nwrite <= 0)
      return nwrite;

    nleft  -= nwrite;
    buffer += nwrite;
  }

  return length;
}

int parsemeta(const char* line, struct meta* metadata) {
  char* ptr = strchr(line, ' ');
  if (!ptr)
    return -1;

  *ptr = '\0';
  size_t fnlength;
  if (!(fnlength = atol(line)) || fnlength > FNBUFSIZ)
    return -1;

  char filename[FNBUFSIZ];

  ptr++;
  int i;

  for (i = 0; i < fnlength; i++) {
    if (ptr[i] == '\0')
      return -1;

    filename[i] = ptr[i];
  }
  filename[i] = '\0';

  ptr += i;
  if (*ptr != ' ')
    return -1;

  ptr++;
  size_t filesize;
  if (!(filesize = atol(ptr)))
    return -1;

  metadata->fnlength = fnlength;
  strcpy(metadata->filename, filename);
  metadata->filesize = filesize;

  return 0;
}

ssize_t recvwrall(int sockfd, int filefd, char* buffer, size_t buffersize, size_t length) {
  ssize_t nrecv;
  size_t  nleft = length;
  size_t  chunk;

  while (nleft > 0) {
    chunk = nleft < buffersize ? nleft : buffersize;
    nrecv = recv(sockfd, buffer, chunk, 0);
    if (nrecv <= 0)
      return nrecv;

    if (writeall(filefd, buffer, nrecv) <= 0)
      return -2;

    nleft -= nrecv;
  }

  return length;
}
