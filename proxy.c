#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#define MAX_NUM_CHILD 100

//PROCESS RUNNING
int noofChildren;

//MAX ERROR LENGTH 
int errorMLen = 100; 

//DEFAULT ERROR MESSAGE
const char *errorM = "HTTP/1.0 500 INTERNAL ERROR\r\n\r\n";

// CREATE A SOCKET AND RETURN IT 
 int createClientSocket(char *pcAddress, char *pcPort) {
  struct addrinfo sockval, *sockeid;
  int socknumber;

  bzero(&sockval,sizeof(sockval));
  sockval.ai_family = AF_UNSPEC;
  sockval.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(pcAddress, pcPort, &sockval, &sockeid) != 0) {
    exit(1);
  }

  //CREATE AND CONNECT TO SOCKET
  if ((socknumber = socket(sockeid->ai_family, sockeid->ai_socktype, sockeid->ai_protocol)) < 0) {
    exit(1);
  }
  if (connect(socknumber, sockeid->ai_addr, sockeid->ai_addrlen) < 0) {
    exit(1);
  }

  //printf("%d\n", socknumber);
  freeaddrinfo(sockeid);

  return socknumber;
}

// WRITING TO THE HOST THE REQUEST STUFF IE GET / PROTOCOL
 void writetohost (const char *buf, int sockfd, int otherfd, int *len) {
	int iSent;
	int iTotalSent = 0;

	for (;iTotalSent < *len;) {
		if ((iSent = send(sockfd, (void *) (buf + iTotalSent), *len - iTotalSent, 0)) < 0) {
			close(sockfd);
			if (otherfd != -1) {
				close(otherfd);
			}
			exit(1);
		}
		iTotalSent += iSent;
	}
	//printf("%d\n", iTotalSent);
}


//CREATING SERVER REQUEST WHICH WILL THEN BE SENT TO HOST 
 char *getServerReq (struct ParsedRequest *req, int clienSockVal, int *reqLen) {
	int hdrlength;
	char *serverReq;
	char *headersBuf;

	//HEADERS
	ParsedHeader_set(req, "Host", req->host);
	ParsedHeader_set(req, "Connection", "close");

	hdrlength = ParsedHeader_headersLen(req);
	headersBuf = (char *) malloc(hdrlength + 1);

	ParsedRequest_unparse_headers(req, headersBuf, hdrlength);
	headersBuf[hdrlength] = '\0';

	*reqLen = strlen(req->method) + strlen(req->path) + strlen(req->version) + hdrlength + 4;
	serverReq = (char *) malloc(*reqLen + 1);

	// CREATING THE REQUEST
	serverReq[0] = '\0';
	strcpy(serverReq, req->method);
	strcat(serverReq, " ");
	strcat(serverReq, req->path);
	strcat(serverReq, " ");
	strcat(serverReq, req->version);
	strcat(serverReq, "\r\n");
	strcat(serverReq, headersBuf);

	free(headersBuf);
	//printf("%s\n", serverReq);
	return serverReq;
}

void checkcon(char *val) {
	char newStr[100] = "";
	for (int i = 0; val[i] != '\n'; i++) {
		if (i == 1)
			break;
		strcat(newStr, "0");
	}
}

/* READ THE SERVER SOCKET  */
 char *readFromClient(int sockfd) {
	int buffer_len = 4096;
	char buf[buffer_len + 1];
	char *request;
	int iRecv, iSize;
	int iReqSize = 0;

	/* REQUEST STRING FROM CLIENT */
	request = (char *) malloc(buffer_len + 1);

	iSize = buffer_len;
	request[0] = '\0';

	/* REALLOC IF MEMORY IS LESS */
	while (strstr(request, "\r\n\r\n") == NULL) {
		if ((iRecv = recv(sockfd, buf, buffer_len, 0)) < 0) {
			writetohost(errorM, sockfd, -1, &errorMLen);
			close(sockfd);
			exit(1);
		}
		if (iRecv == 0) break;
		buf[iRecv] = '\0';
		iReqSize += iRecv;
		if (iReqSize > iSize) {
			iSize *= 2;
			request = (char *) realloc(request, iSize + 1);
		}
		strcat(request, buf);
	}
	//printf("%s\n", req);
	checkcon(request);
	return request;
}

// READ FROM THE CLEINTS AND WRITE TO SERVER

 void sendtoclient (int clienSockVal, int hostcon) {
	int buffer_len = 4096;
	int iRecv;
	char buf[buffer_len];

	while ((iRecv = recv(hostcon, buf, buffer_len, 0)) > 0)
	      writetohost(buf, clienSockVal, hostcon, &iRecv);

	//printf("%s\n", buf);

	//IF FAILED TO RECIEVE FROM SERVER
	if (iRecv < 0) {
	  writetohost(errorM, clienSockVal, hostcon, &errorMLen);
	  //printf("%s\n", errorM);
	  close(clienSockVal);
	  close(hostcon);
	  exit(1);
	}
}

void test() {
	int no = 4096;
	int cn = 0;
	int flag = 0;
	for (int i = 2; i * i < no; i++) {
		if (flag == 1)
			break;
		for (int j = 2; j < i; j++) {
			if (j == 2) {
				flag = 1;
				break;
			}
			if (i % j == 0) {
				cn++;
			}
		}
	}
}

//HANDLING CLIENT REQUESTS AS MULTIPLE THREADS
 void clientRequests (int clienSockVal, int socknumber) {
	int iPid, hostcon;
	char *clientReq;
	char *serverReq;

	/* CREATING THE CHILD WHICH WILL HAVE THE SAME VALUES AS THE PARENT PROCESS */
	fflush(NULL);
	iPid = fork();
	if (iPid == -1) {
		close(clienSockVal);
		return;
	}

	if (iPid == 0)
	{
		struct ParsedRequest *req;
		int lenClient = 0;
		int *reqLen = &lenClient;

		/* SOCKET CLOSE */
		close(socknumber);

		clientReq = readFromClient(clienSockVal);
		//printf("%s", clientReq);
		/* CLEINTREQUEST IS THE STRING OBTAINED FROM CLIENT THAT IS THE OUTPUT IE WEBPAGE */
		
		req = ParsedRequest_create();

		//STORING THE VALUES TO PARSED STRUCTURE
		if (ParsedRequest_parse(req, clientReq, strlen(clientReq)) < 0) {
			writetohost(errorM, clienSockVal, -1, &errorMLen);
			close(clienSockVal);
			exit(1);
		}
		// TAKING DEFAULT PORT AS 80
		if (req->port == NULL) req->port = (char *) "80";
		//printf("%s", req->port);

		serverReq = getServerReq(req, clienSockVal, reqLen);
		
		//printf("%s\n", serverReq);
		
		//CONNECTING TO MENTIONED HOST VIA MENTIONED PORT DEFAULT IS 80
		hostcon = createClientSocket(req->host, req->port);
		//printf("connection no is %d\n", hostcon);
		// WRITING THE GET TO THE HOST IE ISERVERID AND RECIEVEING THE PACKAGE IE WEBPAGE AS GENERAL
		writetohost(serverReq, hostcon, clienSockVal, reqLen);

		//WHATEVER IT RECIEVES FROM THAT HOST IS SEND BACK TO THE CLIENT IE CLIENTSOCKVAL IN THIS CASE
		sendtoclient(clienSockVal, hostcon);

		// FREE MEMORY AND CLEAR THE REQ STRUCTURE
		ParsedRequest_destroy(req);
		free(serverReq);
		free(clientReq);
		close(clienSockVal);
		close(hostcon);

		exit(0);
	}

	test();
	/* Waits for child processes and updates their number */
	noofChildren++;
	for (;waitpid(-1, NULL, WNOHANG) > 0;) {
		noofChildren--;
	}
	if (noofChildren >= MAX_NUM_CHILD) {
		wait(NULL);
		noofChildren--;
	}

	close(clienSockVal);
}

/*SENDING SOCKID TO CALLER*/
 int makeServer(char *pcPort) {
  int laggedOnes = 101;
  struct addrinfo sockval, *sockeid;
  int socknumber;

  /* GET ADDRESS */
  bzero(&sockval,sizeof(sockval));
  sockval.ai_family = AF_UNSPEC;
  sockval.ai_socktype = SOCK_STREAM;
  sockval.ai_flags = AI_PASSIVE;

  if (getaddrinfo(NULL, pcPort, &sockval, &sockeid) < 0) {
      exit(1);
  }

  /* BIND AND LISTEN */
  if ((socknumber = socket(sockeid->ai_family, sockeid->ai_socktype, sockeid->ai_protocol)) < 0) {
    exit(1);
  }
  if (bind(socknumber, sockeid->ai_addr, sockeid->ai_addrlen) < 0) {
    exit(1);
  }
  if (listen(socknumber, laggedOnes) < 0) {
    exit(1);
  }
	//printf("%d\n",socknumber);
  freeaddrinfo(sockeid);
  //RETURN THE SOCKET CREATED
  return socknumber;
}

//MAIN FUNCTION STARTS HERE IE MAIN LOGIC
int main(int argc, char * argv[]) {
	int socknumber, clienSockVal;
	socklen_t iLen;
	struct sockaddr aClient;

	if (argc != 2) {
	   fprintf(stderr, "usage: %s port\n", argv[0]);
	   exit(1);
    }

	  /* Creating the server for processing the request from the clients */	  
	  socknumber = makeServer(argv[1]);
	  noofChildren = 0;
	  iLen = sizeof(struct sockaddr);
	
	  for(;;) {
	  	/* ServerSocket is created now taking the clients */
	    if ((clienSockVal = accept(socknumber, &aClient, &iLen)) <=  0) {
	      close(clienSockVal);
	      continue;
	    }
	    //printf("%d\n", clienSockVal);
	    clientRequests(clienSockVal, socknumber);
	  }
	  close(socknumber);
	  return 0;
}
