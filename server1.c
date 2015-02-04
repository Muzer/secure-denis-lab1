#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/select.h>

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
  return sock;
}

int doHttpStuff(int sock)
{
  printf("Doing HTTP stuff for socket %i\n", sock);

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
    for(int i = 0; i < max_fd + 1; i++) {
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
        printf("Accepted some person\n");
      }
      for(int i = 0; i < max_fd + 1; i++)
      {
        if(FD_ISSET(i, &read_fds) && i != listenSock)
        {
          doHttpStuff(i);
        }
      }
    }
  }
}
