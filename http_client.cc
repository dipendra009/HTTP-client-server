#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#include <string.h>

#define BUFSIZE 1024
using namespace std;
int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char bptr[BUFSIZE + 1];
    char bptr2[BUFSIZE + 1];
    char * endheaders = NULL;
   
    //struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
    if((sock = minet_socket(SOCK_STREAM)) < 0) {
	minet_perror("Cannot open socket");
    }

    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);
    if(site == NULL)
	minet_perror("No such server name");

    /* set address */
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = *(unsigned long*)site->h_addr_list[0]; 
    sa.sin_port = htons(server_port);

    /* connect socket */
    if((rc = minet_connect(sock, &sa)) < 0) {
     	minet_perror("connection error");
    	close(sock);
     	return 2;
     }
    /* send request */
    req = (char *)malloc(sizeof(char)*(strlen(server_path)+(strlen(server_name))+30));
    sprintf(req, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", server_path, server_name);
   
    if((rc = write_n_bytes(sock, req, strlen(req))) < 0)
    {
	minet_perror("Error writing request");
    }

    /* wait till socket can be read */
    FD_ZERO(&set);
    FD_SET(sock, &set);

    /* Hint: use select(), and ignore timeout for now. */
    if((rc = minet_select(sock + 1, &set, NULL, NULL, NULL)) <=0) {
    	minet_perror("Select error");
    }
    /* first read loop -- read headers */
    while((endheaders = strstr(buf, "\r\n\r\n")) == NULL)
    {
        if((rc = minet_read(sock, buf+datalen, BUFSIZE- datalen)) < 0)
        {
            minet_perror("Error in reading headers");
        }
        datalen = datalen + rc;
    }
    endheaders = endheaders + 4;
    buf[datalen] = '\0';	       
      
     //Skip "HTTP/1.0"
    strcpy(bptr, endheaders);
    strncpy(bptr2, buf, endheaders - buf);
  //remove the '\0'
  // Normal reply has return code 200
  if(!strstr( bptr2, "200"))
  {    	    				            	             
   fflush(stdout);
  fprintf(stderr,"%s", buf);
      ok = false;
  }    	    				            	            			        	
  else
    {
	fflush(stdout);
        /* print first part of response */
        fprintf(wheretoprint, "%s", bptr);
 
        /* second read loop -- print out the rest of the response */
        while((rc = minet_read(sock, buf, BUFSIZE)) > 0)
        {
            buf[rc] = '\0';
            fprintf(wheretoprint, "%s", buf);
 
        }
 
    }
   /*close socket and deinitialize*/
    free(req);
    minet_close(sock);
    minet_deinit();   	    				            	            			      
    if (ok) { 
	return 0;
    } else {
        return -1;  
    }
}
    
int write_n_bytes(int fd, char * buf, int count) {
	int rc = 0;
        int totalwritten = 0;
        while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
              totalwritten += rc;
        }
        if (rc < 0) {
           return -1;
        } else {
           return totalwritten;
        }
}


