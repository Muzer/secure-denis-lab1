#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char buffer[2049];
int evilAdminVariable = 0;
char *rootPath = "/home/muzer/public_html";

int initSocket(void)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0); // create internet TCP socket
  if(sock == -1)
  {
    perror("socket()");
    return -1;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr));
  if(bind(sock, &addr, sizeof(addr)) == -1)
  {
    perror("bind()");
    return -1;
  }
  if(listen(sock, 5) == -1)
  {
    perror("listen()");
    return -1;
  }
  int reuseaddr = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
  return sock;
}

int serveFile(int sock, char *path)
{
  if(evilAdminVariable)
  {
    char *response = "HTTP/1.0 200 OK\r\n\r\n<!DOCTYPE html><html><head>"
      "<title>Welcome to the secret admin page</title></head><body><p>It's a"
      " secret!</p></body></html>";
    send(sock, response, strlen(response), 0);
    return 1;
  }
  char fullPath[2048+strlen(rootPath)+11];
  strncpy(fullPath, rootPath, 2048+strlen(rootPath)+11);
  strncat(fullPath, path, 2048+strlen(rootPath)+11);
  printf("Serving path %s\n", fullPath);

  FILE *f = fopen(fullPath, "r");
  if(f == NULL)
  {
    char *error = "HTTP/1.0 404 Not Found\r\n";
    send(sock, error, strlen(error), 0);
    return -1;
  }

  struct stat stat_struct;
  fstat(fileno(f), &stat_struct);
  if((stat_struct.st_mode & S_IFMT) == S_IFDIR)
  {
    fclose(f);
    strncat(fullPath, "/index.html", 2048+strlen(rootPath)+11);
    f = fopen(fullPath, "r");
    if(f == NULL)
    {
      char *error = "HTTP/1.0 404 Not Found\r\n";
      send(sock, error, strlen(error), 0);
      return -1;
    }
    fstat(fileno(f), &stat_struct);
    if((stat_struct.st_mode & S_IFMT) == S_IFDIR)
    {
      char *error = "HTTP/1.0 404 Not Found\r\n";
      send(sock, error, strlen(error), 0);
      return -1;
    }
  }

  char *status = "HTTP/1.0 200 OK\r\n\r\n";
  
  send(sock, status, strlen(status), 0);

  while(!feof(f))
  {
    size_t bytesToWrite = fread(buffer, 1, 2048, f);
    if(bytesToWrite == 0)
    {
      return -1;
    }
    send(sock, buffer, bytesToWrite, 0);
  }
  return 1;
}

int doHttpStuff(int sock)
{
  buffer[2048] = '\0';
  printf("Doing HTTP stuff for socket %i\n", sock);
  ssize_t receivedSize = recv(sock, buffer, 2048, 0);
  if(receivedSize == -1)
  {
    perror("recv()");
    return -1;
  }
  char *eol = NULL;
  eol = memchr(buffer, '\n', 2048);
  if(eol != buffer && eol != NULL && eol[-1] == '\r')
    eol--;
  if(eol == NULL) {
    /* URL too long for IE so it's too long for us */
    char *error = "HTTP/1.0 400 Bad Request\r\n";
    send(sock, error, strlen(error), 0);
    return -1;
  }

  if(strncmp(buffer, "GET ", 4) != 0)
  {
    char *error = "HTTP/1.0 501 Not Implemented\r\n";
    send(sock, error, strlen(error), 0);
    return -1;
  }

  char *startOfString = buffer + 4;
  char *spaceChar = memchr(startOfString, ' ', 2048-4);
  if(spaceChar == NULL)
  {
    char *error = "HTTP/1.0 400 Bad Request\r\n";
    send(sock, error, strlen(error), 0);
    return -1;
  }

  *spaceChar = '\0'; /* make it a real string */

  if(strstr(spaceChar + 1, "\n\n") || strstr(spaceChar + 1, "\r\n\r\n"))
  {
    /* we've already found it */
    return serveFile(sock, startOfString);
  }

  int endsWithNewline = 0;
  if(buffer[receivedSize-1] == '\n')
    endsWithNewline = 1;
  while(1)
  {
    int len = recv(sock, buffer, 2048, 0);
    if(endsWithNewline && len > 0 && (buffer[0] == '\n' || buffer[1] == '\n'))
      return serveFile(sock, startOfString);
    if(len == -1)
    {
      perror("recv()");
      return -1;
    }
    if(strstr(buffer, "\n\n") || strstr(buffer, "\r\n\r\n"))
    {
      return serveFile(sock, startOfString);
    }
    if(len >= 1 && buffer[len-1] == '\n')
      endsWithNewline = 1;
  }

  printf("Done HTTP stuff for socket %i\n", sock);
  return 1;
}

int main(int argc, char **argv)
{
  int listenSock = initSocket();
  if(listenSock == -1)
    return 1;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(listenSock, &fds);
  int max_fd = listenSock;

  while(1)
  {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int i;
    for(i = 0; i < max_fd + 1; i++) {
      if(FD_ISSET(i, &fds))
        FD_SET(i, &read_fds);
    }
    printf("Blocking select\n");
    int retval = select(max_fd+1, &read_fds, NULL, NULL, NULL);
    printf("No longer blocking select\n");
    if(retval == -1)
    {
      perror("select()");
      return 1;
    }
    else if (retval)
    {
      if(FD_ISSET(listenSock, &read_fds))
      {
        int newSock = accept(listenSock, NULL, NULL);
        if(newSock == -1)
        {
          perror("accept()");
          continue;
        }
        FD_SET(newSock, &fds);
        if(max_fd < newSock)
          max_fd = newSock;

        /* timeout all reads after 30s */
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, &tv,
            sizeof(struct timeval));

        printf("Accepted some person\n");
      }
      int i;
      for(i = 0; i < max_fd + 1; i++)
      {
        if(FD_ISSET(i, &read_fds) && i != listenSock)
        {
          doHttpStuff(i);
          close(i);
          FD_CLR(i, &fds); /* It's now disconnected */
        }
      }
    }
  }
}
