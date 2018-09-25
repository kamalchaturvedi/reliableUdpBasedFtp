/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/stdint-intn.h>
#include <bits/types/FILE.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 1024
#define RESPSIZE 64
#define FILESIZE 64

/*
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(1);
}
void convertToCorrectForm(char *, char[][BUFSIZE]);
int sendFileToClient(char *, struct sockaddr_in, int, int);
int sendAll(int, FILE *, struct stat, struct sockaddr_in, int, char *);
int min(int, int);
int writeIntoFile(char[], char *);
int sendDirectoryDetailsToClient(struct sockaddr_in, int, int);
int deleteDirectoryOfFileNameReceived(char *, struct sockaddr_in, int, int);

static currentSequenceCounter = -1;
static char buf[BUFSIZE]; /* message buf */
static char resp[RESPSIZE];

int main(int argc, char **argv) {
	int sockfd; /* socket */
	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	char receivedData[4][BUFSIZE];
	/*
	 * check command line arguments
	 */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]);

	/*
	 * socket: create the parent socket
	 */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	/* setsockopt: Handy debugging trick that lets
	 * us rerun the server immediately after we kill it;
	 * otherwise we have to wait about 20 secs.
	 * Eliminates "ERROR on binding: Address already in use" error.
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
			sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) portno);

	/*
	 * bind: associate the parent socket with a port
	 */
	if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
		error("ERROR on binding");

	/*
	 * main loop: wait for a datagram, then echo it
	 */
	clientlen = sizeof(clientaddr);
	printf(
			"Ola ! Server here. Client can perform the following tasks & send task id to server \n 1. Add a file to the server\n 2. Get a file from the server\n 3. List files in the server \n 4. Delete a file from the server\n");

	while (1) {
		/*
		 * recvfrom: receive a UDP datagram from a client
		 */
		bzero(buf, BUFSIZE);
		bzero(resp, RESPSIZE);
		memset(&clientaddr, 0, sizeof(clientaddr));
		clientaddr.sin_family = AF_INET;
		n = recvfrom(sockfd, buf, sizeof(buf), 0,
				(struct sockaddr *) &clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");

		/*
		 * gethostbyaddr: determine who sent the datagram
		 */
		hostp = gethostbyaddr((const char *) &clientaddr.sin_addr.s_addr,
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		if (hostp == NULL) {
			printf("ERROR on gethostbyaddr : %s", hstrerror(h_errno));
			return -1;
		}
		fflush(stdout);
		hostaddrp = inet_ntoa(clientaddr.sin_addr);
		if (hostaddrp == NULL)
			error("ERROR on inet_ntoa\n");
		printf("server received datagram from %s (%s)\n", hostp->h_name,
				hostaddrp);
		printf("server received %d/%d bytes:\n", strlen(buf), n);
		printf("%s \n", buf);
		convertToCorrectForm(buf, &receivedData);
		int receivedCounter = stringToInt(receivedData[1]);
		if (currentSequenceCounter < receivedCounter) {
			currentSequenceCounter = receivedCounter;
			switch ((int) (receivedData[0][0] - '0')) {
			case 1:
				writeIntoFile(&receivedData[3], receivedData[2]);
				bzero(resp, RESPSIZE);
				sprintf(resp, "Received %d bytes from file %s",
						(int) strlen(receivedData[3]), receivedData[2]);
				n = sendto(sockfd, resp, strlen(resp), 0,
						(struct sockaddr *) &clientaddr, clientlen);
				if (n < 0)
					error("ERROR in sendto");
				break;
			case 2:
				sendFileToClient(receivedData[2], clientaddr, clientlen,
						sockfd);
				break;
			case 3:
				sendDirectoryDetailsToClient(clientaddr, clientlen, sockfd);
				break;
			case 4:
				deleteDirectoryOfFileNameReceived(receivedData[2], clientaddr,
						clientlen, sockfd);
				break;
			case 5:
				exitServer(clientaddr, clientlen, sockfd);
				break;
			default:
				break;
			}
		}
	}
}

int exitServer(struct sockaddr_in clientaddr, int clientlen, int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);
	struct stat s;
	sprintf(buf, "Server will shutdown now\n");
	n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr,
			clientlen);
	if (n < 0)
		error("ERROR in sendto");
	close(sockfd);
	exit(0);
	return n;
}
int deleteDirectoryOfFileNameReceived(char *fileName,
		struct sockaddr_in clientaddr, int clientlen, int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);
	struct stat s;

	if (remove(fileName) == 0) {
		sprintf(buf, "Deleted file %s successfully\n", fileName);
	} else {
		sprintf(buf, "Error in deleting file : %s\n", fileName);
	}
	n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr,
			clientlen);
	if (n < 0)
		error("ERROR in sendto");

	return n;
}
int sendDirectoryDetailsToClient(struct sockaddr_in clientaddr, int clientlen,
		int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);

	// getting current directory details
	DIR *directory;
	struct dirent *directoryEntry;
	directory = opendir(".");
	if (directory) {
		sprintf(buf, "%s \n", "File Name");
		while ((directoryEntry = readdir(directory)) != NULL) {
			sprintf(buf + strlen(buf), "%s \n", directoryEntry->d_name);
		}
		closedir(directory);
	}
	n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr,
			clientlen);
	if (n < 0)
		error("ERROR in sendto");
	return n;
}
void convertToCorrectForm(char buf[], char receivedData[][BUFSIZE]) {
	int j = 0, counter = 0;
	for (int i = 0; i <= (strlen(buf)); i++) {
		// if space or NULL found, assign NULL into newString[ctr]
		if ((counter < 3) && (buf[i] == ':' || buf[i] == '\0')) {
			receivedData[counter][j] = '\0';
			counter++;  //for next word
			j = 0;    //for next word, init index to 0
		} else {
			receivedData[counter][j] = buf[i];
			j++;
		}
	}
}
int sendFileToClient(char *fileName, struct sockaddr_in clientaddr,
		int clientlen, int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);
	struct stat s;

	if (stat(fileName, &s) == -1) {
		printf("File info not available");
		return -1;
	}
	sprintf(buf, "%d", (int) s.st_size);
	n = sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &clientaddr,
			clientlen);
	if (n < 0)
		error("ERROR in sendto");
	FILE *f = fopen(fileName, "rb");
	if (!f) {
		printf("Cannot read from file");
		return -1;
	}
	bzero(buf, BUFSIZE);
	n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &clientaddr,
			&clientlen);
	if (n < 0)
		error("ERROR in recvfrom");
	n = sendAll(sockfd, f, s, clientaddr, clientlen, fileName);
	if (n < 0) {
		error("Could not complete the file transfer");
		return -1;
	}
	fclose(f);
	return 1;
}
int sendAll(int sockfd, FILE *f, struct stat s, struct sockaddr_in clientaddr,
		int clientlen, char *fileName) {
	//tell server that I will send a file of name provided
	int64_t size = s.st_size;
	int n;
	printf("File size is %d \n", (int) size);
	int fileSizeSent = 0, sizeToBeSentPerIteration = 0, totalSizeSent = 0;
	while (size > 0) {
		bzero(buf, BUFSIZE);
		bzero(resp, BUFSIZE);
		sizeToBeSentPerIteration = min(sizeof(buf), size);
		printf("Reading %d bytes", sizeToBeSentPerIteration);
		int readFromFile = fread(buf, 1, sizeToBeSentPerIteration, f);
		if (readFromFile < 0) {
			printf("The file sequence cannot be read \n");
			return -1;
		}
		fileSizeSent = sendto(sockfd, buf, readFromFile, 0,
				(struct sockaddr *) &clientaddr, clientlen);
		printf("\n Sent datagram \n");

		//getting ack back from the server
		n = recvfrom(sockfd, resp, sizeof(resp), 0,
				(struct sockaddr *) &clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");
		printf("Echo from client: %s \n", resp);

		size = size - fileSizeSent;
		totalSizeSent = totalSizeSent + fileSizeSent;
	}
	printf("Successfully sent the file to client : %d bytes! \n",
			totalSizeSent);
	return 1;
}
int min(int val0, int val1) {
	if (val0 < val1)
		return val0;
	else
		return val1;
}
int writeIntoFile(char *buf, char *fileName) {
	int writingIntoFile;
	printf("%s\n", buf);
	fflush(stdout);
	FILE *openingFileForWriting = fopen(fileName, "ab");
	if (!openingFileForWriting) {
		printf("Cannot open file to execute write operation");
	} else {
		writingIntoFile = fwrite(buf, 1, strlen(buf), openingFileForWriting);
		fflush(openingFileForWriting);
		printf(" \n \n Wrote %d out of %d into file \n \n", writingIntoFile,
				(int) strlen(buf));
		if (writingIntoFile < 0) {
			printf("\n\n The file sequence cannot be written \n\n");
			return -1;
		}
		fclose(openingFileForWriting);
	}
	return 1;
}
int stringToInt(char * resp) {
	int i = 0, temp = 0, valueToReturn = 0;
	while (resp[i] != '\0') {
		temp = (int) (resp[i] - '0');
		valueToReturn = valueToReturn * 10 + temp;
		++i;
	}
	return valueToReturn;
}

