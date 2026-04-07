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
    servaddr.s_addr = inet_addr(hostinfo->h_addr_list[0]);
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
  char*   filename;
  ssize_t nread;

  int nfiles = argc - 2;

  // send number of files to server
  snprintf(buffer, sizeof(buffer), "%d\n", nfiles);
  if (sendall(sockfd, buffer, strlen(buffer)) <= 0) {
    fprintf(stderr, "failed to send number of files\n");
    close_socket_errmsg(sockfd);
    exit(EXIT_FAILURE);
  }

  // for each file
  for (int i = 2; i < argc; i++) {
    filename = argv[i];
    filefd = open(filename, O_RDONLY);
    if (filefd == -1) {
      fprintf(stderr, "cannot open file \"%s\": %s\n", filename, strerror(errno));
      close_socket_errmsg(sockfd);
      exit(EXIT_FAILURE);
    }
    // send file name
    snprintf(buffer, sizeof(buffer), "%s\n", filename);
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
    break; // only send one file!
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


//   int fd;
//   char* filename;
//   char buffer[BUFFERSIZE];
//   ssize_t nread;

//   for (int i = 2; i < argc; i++) {
//     filename = argv[i];
//     fd = open(filename, O_RDONLY);
//     if (fd == -1) {
//       fprintf(stderr, "cannot open file \"%s\"\n", filename);
//       continue;
//     }
//     snprintf(buffer, sizeof(buffer), "%s\n", filename);
//     if (sendall(sockfd, buffer, strlen(buffer)) == -1) {
//       fprintf(stderr, "failed to send filename \"%s\" to server\n", filename);
//       close_print_error(filename, fd);
//       continue;
//     }
//     while ((nread = read(fd, buffer, sizeof(buffer))) > 0) {
//       if (sendall(sockfd, buffer, nread) == -1)
//         fprintf(stderr, "failed to send some data to the server (filename = %s)\n", filename);
//     }
//     if (nread == -1)
//       fprintf(stderr, "failed to read some data from file \"%s\"\n", filename);

//     close_print_error(filename, fd);
//   }

//   if (close(sockfd) == -1)
//     fprintf(stderr, "failed to close socket\n");

//   exit(EXIT_SUCCESS);
// }

// int sendall(int sockfd, const char* buffer, size_t length) {
//   ssize_t nsend;
//   size_t  nleft = length;

//   while (nleft > 0) {
//     nsend = send(sockfd, buffer, nleft, 0);
//     if (nsend == -1)
//       return -1;

//     nleft  -= nsend;
//     buffer += nsend;
//   }

//   return 0;
// }

// void close_print_error(const char* fn, int fd) {
//   if (close(fd) == -1)
//     fprintf(stderr, "failed to close file \"%s\" (file descriptor = %d)\n", fn, fd);
// }