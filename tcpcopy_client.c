#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT       3000
#define BUFFERSIZE 2048

void close_errmsg(const char*, int);
void close_socket_errmsg(int);
ssize_t sendall(int, const char*, size_t);

int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <hostname/ipaddr> <file1> [... <fileN>]\n", *argv);
    exit(EXIT_FAILURE);
  }
  struct in_addr servaddr;
  if (!inet_aton(argv[1], &servaddr)) {
    struct hostent* hostinfo = gethostbyname(argv[1]);
    if (!hostinfo) {
      fprintf(stderr, "cannot resolve hostname \"%s\"\n", argv[1]);
      exit(EXIT_FAILURE);
    }
    memcpy(&servaddr, hostinfo->h_addr_list[0], hostinfo->h_length);
  }
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    fprintf(stderr, "cannot open socket: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(PORT);
  addr.sin_addr   = servaddr;

  if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr))) {
    fprintf(stderr, "cannot connect to %s: %s\n", inet_ntoa(servaddr), strerror(errno));
    close_socket_errmsg(sockfd);
    exit(EXIT_FAILURE);
  }

  int     filefd;
  char    buffer[BUFFERSIZE];
  char*   path;
  char*   filename;
  char*   temp;
  ssize_t nread;

  int nfiles = argc - 2;

  // send number of files to server
  snprintf(buffer, sizeof(buffer), "%d\n", nfiles);
  if (sendall(sockfd, buffer, strlen(buffer)) <= 0) {
    fprintf(stderr, "failed to send number of files\n");
    close_socket_errmsg(sockfd);
    exit(EXIT_FAILURE);
  }

  struct stat attr;
  off_t       filesize;
  size_t      fnlength;

  // for each file
  for (int i = 2; i < argc; i++) {
    path = argv[i];
    filefd = open(path, O_RDONLY);
    if (filefd == -1) {
      fprintf(stderr, "cannot open file \"%s\": %s\n", path, strerror(errno));
      close_socket_errmsg(sockfd);
      exit(EXIT_FAILURE);
    }

    // get file status
    if (fstat(filefd, &attr) == -1) {
      fprintf(stderr, "cannot get file attributes for file \"%s\": %s\n", path, strerror(errno));
      close_errmsg(path, filefd);
      continue;
    }

    filename = path;
    if (temp = strrchr(path, '/') && *(++temp))
      filename = temp;

    filesize = attr.st_size;
    fnlength = strlen(filename);

    // send meta information
    snprintf(buffer, sizeof(buffer), "%zu %s %ld\n", fnlength, filename, filesize);
    if (sendall(sockfd, buffer, strlen(buffer)) <= 0) {
      fprintf(stderr, "failed to send file name\n");
      close_socket_errmsg(sockfd);
      exit(EXIT_FAILURE);
    }
    // send file contents
    while ((nread = read(filefd, buffer, sizeof(buffer))) > 0) {
      if (sendall(sockfd, buffer, nread) <= 0) {
        fprintf(stderr, "failed to send some data (file: \"%s\") \n", filename);
        close_socket_errmsg(sockfd);
        exit(EXIT_FAILURE);
      }
    }
    close_errmsg(filename, filefd);
  }
}

void close_errmsg(const char* filename, int fd) {
  if (close(fd) == -1)
    fprintf(stderr, "cannot close file \"%s\" (descriptor = %d): %s\n", filename, fd, strerror(errno));
}

void close_socket_errmsg(int sockfd) {
  if (close(sockfd) == -1)
    fprintf(stderr, "cannot close socket (descriptor = %d): %s\n", sockfd, strerror(errno));
}

ssize_t sendall(int sockfd, const char* buffer, size_t length) {
  ssize_t nsend;
  size_t  nleft = length;

  while (nleft > 0) {
    nsend = send(sockfd, buffer, nleft, 0);
    if (nsend <= 0)
      return nsend;

    nleft  -= nsend;
    buffer += nsend;
  }

  return length;
} 

// protocol: n filename m\n