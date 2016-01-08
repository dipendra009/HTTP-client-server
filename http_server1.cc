#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <fstream>

#define BUFSIZE 1024
#define FILENAMESIZE 100
using namespace std;
int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa;
  struct sockaddr_in sa2;
  int rc;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') {
    minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
    minet_init(MINET_USER);
    } else {
    fprintf(stderr, "First argument must be k or u\n");
    exit(-1);
    }

  /* initialize and make socket */
  if((sock = minet_socket(SOCK_STREAM)) < 0) {
     minet_perror("Connection error");
     return 1;
  }

  /* set server address*/
        memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(server_port);

        memset(&sa2, 0, sizeof sa2);
        sa2.sin_family = AF_INET;
        sa2.sin_port = htons(server_port);

  /* bind listening socket */
	if(minet_bind(sock, &sa) < 0) {
            minet_perror("bind error");
            return 2;
         }
  /* start listening */
    if((rc = minet_listen(sock, 0) < 0))
	minet_perror("Could not listen");

  /* connection handling loop */
  while(1)
  {
    /* handle connections */
	sock2 = minet_accept(sock, &sa2);
        rc = handle_connection(sock2);
  }
  /* close minet socket */
  minet_deinit();
}

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  int rc;
  char *buf;
  char *endheaders;
  int datalen=0;
  char ok_response_f[] = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char notok_response[] = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;
  buf = (char*)malloc(sizeof(char)* (BUFSIZE + 1));
  /* first read loop -- get request and headers*/
  while((endheaders = strstr(buf, "\r\n\r\n")) == NULL)
    {
        if((rc = minet_read(sock2, buf+datalen, BUFSIZE- datalen)) < 0)
        {
            //minet_perror("Error in reading headers");
        }
        datalen = datalen + rc;
 
    }
    endheaders = endheaders + 4;
    buf[datalen] = '\0';

    /* parse request to get file name */
    /* Assumption: this is a GET request and filename contains no spaces*/
        char * start = NULL; 
	printf("\nstart %s %s", start, buf);
	start = strstr(buf,"GET") + 5;
        int length = strstr(buf,"HTTP") - start-1;
	fflush(stdout);
	printf("start:%s \n %d \n",start,length);
	printf("%s \n",filename);
	strncpy(filename, start, length);
        filename[length] = '\0';
	printf("%s \n",filename);

    /* try opening the file */
    FILE *fp;
    ok = (fp = fopen(filename, "r"));

    /* send response */
    if (ok)
    {
      fseek(fp, 0, SEEK_END);
      datalen = ftell(fp);
      fseek(fp,0, SEEK_SET);
    /* send headers */
        sprintf(ok_response, ok_response_f, datalen);
	writenbytes(sock2, ok_response, strlen(ok_response));
    /* send file */
	while(fgets(buf, BUFSIZE , fp)) {
		writenbytes(sock2, buf, strlen(buf));
	}
    }
    else // send error response
    {
  	writenbytes(sock2, notok_response, strlen(notok_response));
    } 

  /* close socket and free space */
  minet_close(sock2);
  free(buf);

  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
      totalwritten += rc;
  if (rc < 0)
    return -1;
  else
    return totalwritten;
}





