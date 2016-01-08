#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <malloc.h>


#define FILENAMESIZE 100
#define BUFSIZE 1024

typedef enum \
{NEW,READING_HEADERS,WRITING_RESPONSE,READING_FILE,WRITING_FILE,CLOSED} states;

typedef struct connection_s connection;
typedef struct connection_list_s connection_list;

struct connection_s
{
  int sock;
  int fd;
  char filename[FILENAMESIZE+1];
  FILE* fp;
  char *buf;
  char *endheaders;
  bool ok;
  long filelen;
  states state;
  int headers_read,response_written,file_read,file_written, file_towrite;

  connection *next;
};

struct connection_list_s
{
  connection *first,*last;
};

void add_connection(int,connection_list *);
void insert_connection(int,connection_list *);
void init_connection(connection *con);


int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
void read_headers(connection *);
void write_response(connection *);
void read_file(connection *);
void write_file(connection *);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc, j;
  fd_set readlist,writelist;
  fd_set readfd, writefd;
  connection_list *connections;
  connection *i;
  int maxfd;
  //char ch;

	connections = (connection_list*) malloc(sizeof(connection_list));
	connections->first = NULL;
	connections->last = NULL;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server3 k|u port\n");
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

	/* setting the socket to unblocking mode */
	fcntl(sock, F_SETFL, O_NONBLOCK);

  /* set server address*/
        memset(&sa, 0, sizeof sa);
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        sa.sin_port = htons(server_port);

        memset(&sa2, 0, sizeof sa2);
        sa2.sin_family = AF_INET;
        sa2.sin_addr.s_addr = htonl(INADDR_ANY);
        sa2.sin_port = htons(server_port);


    /* bind listening socket */
        if(minet_bind(sock, &sa) < 0) {
            minet_perror("bind error");
            return 2;
         }
	/* start listening */
    if((rc = minet_listen(sock, 0) < 0))
        minet_perror("Could not listen");

	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
	FD_ZERO(&readlist);
	FD_ZERO(&writelist);
	FD_SET(sock, &readlist);
	FD_SET(sock, &writelist);
	maxfd = sock;

  /* connection handling loop */
  while(1)
  {
	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
  	readfd = readlist;
	writefd = writelist;
	/* do a select */
	if( (rc = select(maxfd+1, &readfd, &writefd , NULL, NULL)) < 0)
	{
		//minet_perror("Error in select");
	}

    /* process sockets that are ready */
	for(j=0; j<=maxfd; j++)
	{
		if((!(FD_ISSET(j, &readlist)))&&(!FD_ISSET(j, &writelist)))
			continue;
      /* for the accept socket, add accepted connection to connections */
      if (j == sock)
      {
	  	sock2 = minet_accept(sock, &sa2);
		insert_connection(sock2, connections);
		if(sock2 > maxfd)
			maxfd = sock2+1;
		FD_SET(sock2, &readlist);
      }
      else /* for a connection socket, handle the connection */
      {
	  	i = connections->first;
	  	while((i != NULL)&&(i->sock != j))
			i = i->next;
		if((i == NULL) || (i->sock != j))
		{
			break;
		}
		if(i->state == NEW)
		{
			init_connection(i);
			read_headers(i);

		}
		else if(i->state == READING_HEADERS)
		{
			read_headers(i);
			FD_CLR(j, &readlist);
		 FD_SET(j, &writelist);

		}
		else if(i->state == WRITING_RESPONSE)
			write_response(i);
		else if(i->state == READING_FILE)
			read_file(i);
		else if(i->state == WRITING_FILE)
			write_file(i);
		else if(i->state == CLOSED)
		{
				FD_CLR(j, &writelist );
            if(j == maxfd){
			while(!(FD_ISSET(maxfd, &writelist)) && !(FD_ISSET(maxfd, &readlist))){
				maxfd--;
			}
			}
		}
		else
		{
			printf("\n Invalid state");
		}
	  }
	}
  }

	// free memory for connections

  for(i = connections->first; i!= NULL; i=i->next)
  {
		free(i->buf);
		free(i->endheaders);

  }
  free(connections);

   /* close minet socket */
  minet_deinit();


}

void read_headers(connection *con)
{
	int rc;

	con->state = READING_HEADERS;
  /* first read loop -- get request and headers*/
	while((con->endheaders = strstr(con->buf, "\r\n\r\n")) == NULL)
    {
        if((rc = minet_read(con->sock, con->buf+con->headers_read, BUFSIZE- con->headers_read)) < 0)
        {
			if (errno == EAGAIN)
     			 return;
			else
			{
				minet_perror("Error in reading headers");
				exit(1);
			}


        }
        con->headers_read = con->headers_read + rc;

    }
    con->endheaders = con->endheaders + 4;
    con->buf[con->headers_read] = '\0';

    /* parse request to get file name */
    /* Assumption: this is a GET request and filename contains no spaces*/
    
        char * start = strstr(con->buf,"GET") + 5;
        int length = strstr(con->buf,"HTTP") - start-1;
        strncpy(con->filename, start, length);
        con->filename[length] = '\0';

    /* try opening the file */

    con->ok = (con->fp = fopen(con->filename, "r"));
	if(con->ok)
	{
	con->fd = fileno(con->fp);

	/* set to non-blocking, get size */
	if((fcntl(con->fd, F_SETFL, O_NONBLOCK))<0)
	{
		minet_perror("\n Error in setting fcntl...");
	}
	}
	con->state = WRITING_RESPONSE;
  write_response(con);

}

void write_response(connection *con)
{
  int sock2 = con->sock;
  int rc;
  int written = con->response_written;
  char ok_response_f[] = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char notok_response[] = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
 /* send response */
    if (con->ok)
    {
      fseek(con->fp, 0, SEEK_END);
      con->filelen = ftell(con->fp);
      fseek(con->fp,0, SEEK_SET);
    /* send headers */
        sprintf(ok_response, ok_response_f, con->filelen);
        rc = writenbytes(sock2, ok_response + written, strlen(ok_response) - written);
		if(rc > 0)
		{
			con->response_written += rc;
			con->state = READING_FILE;
			read_file(con);
		}
		else if(errno == EAGAIN)
		return;
		else
		minet_perror("Error in writting response");
	}
	else	// send error response
  	{
		con->state = WRITING_RESPONSE;
		writenbytes(sock2, notok_response + written, strlen(notok_response) - written);
		con->state = CLOSED;
  	}

}

void read_file(connection *con)
{
    /* send file */

	memset(con->buf,BUFSIZE+1, 0);
	fseek(con->fp, 0, con->file_read);
	if(fgets(con->buf,BUFSIZE, con->fp))
	{
	 con->buf[strlen(con->buf)] = '\0';
    con->file_read += strlen(con->buf);
	con->file_written = 0;
	con->file_towrite = strlen(con->buf);
    con->state = WRITING_FILE;
    write_file(con);
  	}
else
  {
    if (errno == EAGAIN)
      return;
	  con->state = CLOSED;
    minet_close(con->sock);

	return;
  }
    }

void write_file(connection *con)
{
  int written = con->file_written;
  int rc = writenbytes(con->sock, con->buf+written, con->file_towrite);
  if (rc < 0)
  {
    if (errno == EAGAIN)
      return;
    con->state = CLOSED;
    minet_close(con->sock);
    return;
  }
  else
  {
    con->file_written += rc;
    con->file_towrite -= rc;
    if (con->file_towrite == 0)
    {
      con->state = READING_FILE;
      con->file_written = 0;
      read_file(con);
    }
    else
      printf("shouldn't happen\n");
  }
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


// inserts a connection in place of a closed connection
// if there are no closed connections, appends the connection 
// to the end of the list

void insert_connection(int sock,connection_list *con_list)
{
  connection *i;
  for (i = con_list->first; i != NULL; i = i->next)
  {
    if (i->state == CLOSED)
    {
      i->sock = sock;
      i->state = NEW;
      return;
    }
  }
  add_connection(sock,con_list);
}
 
void add_connection(int sock,connection_list *con_list)
{
  connection *con = (connection *) malloc(sizeof(connection));
  con->next = NULL;
  con->state = NEW;
  con->sock = sock;
  if (con_list->first == NULL)
    con_list->first = con;
  if (con_list->last != NULL)
  {
    con_list->last->next = con;
    con_list->last = con;
  }
  else
    con_list->last = con;
}

void init_connection(connection *con)
{
  con->headers_read = 0;
  con->response_written = 0;
  con->file_read = 0;
  con->file_written = 0;
  con->filelen = 0;
  con->buf = (char*)malloc(sizeof(char)*(BUFSIZE +1));
  con->endheaders = (char*)malloc(sizeof(char)*(BUFSIZE + 1));
}
