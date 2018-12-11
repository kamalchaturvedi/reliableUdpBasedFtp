/*
 * udpserver.c - A Unreliable UDP echo server
 * usage: udpserver <port>
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 1024
#define RESPSIZE 64
#define FILESIZE 64

typedef struct packet_tr {
	int type;
	int sequenceNo;
	int fileSize;
	char fileName[FILESIZE];
	char data[BUFSIZE];
} transfer_packet;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(1);
}
int sizeofPacket(){
	return (BUFSIZE + FILESIZE + 3*sizeof(int));
}
int sendFileToClient(char *, struct sockaddr_in, socklen_t, int);
int sendAll(int, FILE *, struct stat, struct sockaddr_in, socklen_t, char *);
int sendPacket(int, char *, int, int, struct sockaddr_in);
int min(int, int);
int writeIntoFile(char[], char *, int);
int exitServer(struct sockaddr_in, socklen_t, int);
int sendDirectoryDetailsToClient(struct sockaddr_in, socklen_t, int);
int deleteDirectoryOfFileNameReceived(char *, struct sockaddr_in, socklen_t, int);

static int currentSequenceCounter = -1;
static char buf[BUFSIZE]; /* message buf */

int main(int argc, char **argv) {
	int sockfd; /* socket */
	int portno; /* port to listen on */
	socklen_t clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr;/* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
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
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
	bzero((char * ) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short ) portno);
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
			"Ola ! Server here. You can perform the following tasks & send task id to server \n 1. Add a file to the server\n 2. Get a file from the server\n 3. List files in the server \n 4. Delete a file from the server\n");

	while (1) {
		/*
		 * recvfrom: receive a UDP datagram from a client
		 */
		bzero(buf, BUFSIZE);
		memset(&clientaddr, 0, sizeof(clientaddr));
		clientaddr.sin_family = AF_INET;
		transfer_packet *packet = (transfer_packet*) malloc(
				sizeof(transfer_packet));
		n = recvfrom(sockfd, (char*) packet, sizeof(transfer_packet), 0,
				(struct sockaddr *) &clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");

		/*
		 * gethostbyaddr: determine who sent the datagram
		 */
		hostp = gethostbyaddr((const char *) &clientaddr.sin_addr.s_addr,sizeof(clientaddr.sin_addr.s_addr), AF_INET);
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
		printf("server received %lu/%d bytes:\n", sizeof(packet), n);
		// convertToCorrectForm(buf, &receivedData);
		int receivedCounter = packet->sequenceNo;
		if (currentSequenceCounter < receivedCounter) {
			currentSequenceCounter = receivedCounter;
			switch (packet->type) {
			case 1:
				writeIntoFile(packet->data, packet->fileName, packet->fileSize);
				// bzero(resp, RESPSIZE);
				transfer_packet *response = (transfer_packet*) malloc(
						sizeof(transfer_packet));
				sprintf(response->data, "Received %d bytes from file %s",
						packet->fileSize, packet->fileName);
				response->sequenceNo = currentSequenceCounter;
				n = sendto(sockfd, (char *) response, sizeof(transfer_packet),
						0, (struct sockaddr *) &clientaddr, clientlen);
				if (n < 0)
					error("ERROR in sendto");
				break;
			case 2:
				sendFileToClient(packet->fileName, clientaddr, clientlen,
						sockfd);
				break;
			case 3:
				sendDirectoryDetailsToClient(clientaddr, clientlen, sockfd);
				break;
			case 4:
				deleteDirectoryOfFileNameReceived(packet->fileName, clientaddr,
						clientlen, sockfd);
				break;
			case 5:
				exitServer(clientaddr, clientlen, sockfd);
				break;
			default:
				break;
			}
		}
		free(packet);
	}
}

int exitServer(struct sockaddr_in clientaddr, socklen_t clientlen, int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);
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
		struct sockaddr_in clientaddr, socklen_t clientlen, int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);
	transfer_packet *response = (transfer_packet*) malloc(
			sizeof(transfer_packet));
	printf("%s", fileName);
	if (remove(fileName) == 0) {
		sprintf(buf, "Deleted file %s successfully\n", fileName);
	} else {
		sprintf(buf, "Error in deleting file : %s\n", fileName);
	}
	response->sequenceNo = currentSequenceCounter;
	strcpy(response->data, buf);
	n = sendto(sockfd, (char *) response, sizeof(transfer_packet), 0,
			(struct sockaddr *) &clientaddr, clientlen);
	free(response);
	if (n < 0)
		error("ERROR in sendto");

	return n;
}
int sendDirectoryDetailsToClient(struct sockaddr_in clientaddr, socklen_t clientlen,
		int sockfd) {
	int n = 0;
	bzero(buf, BUFSIZE);
	// getting current directory details
	transfer_packet *response = (transfer_packet*) malloc(
			sizeof(transfer_packet));
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
	strcpy(response->data, buf);
	response->sequenceNo = currentSequenceCounter;
	/*n = sendto(sockfd, (char *) response, sizeof(transfer_packet), 0,
	 (struct sockaddr *) &clientaddr, clientlen);*/
	n = sendPacket(sockfd, (char*) response, sizeof(transfer_packet), clientlen,
			clientaddr);
	free(response);
	if (n < 0)
		error("ERROR in sendto");
	return n;
}
int sendPacket(int sockfd, char *action, int sizeToBeSent, int serverlen,
		struct sockaddr_in serveraddr) {
	int n;
	n = sendto(sockfd, action, sizeToBeSent, 0, (struct sockaddr*) &serveraddr,
			serverlen);
	printf("\n Sent datagram \n");
	return n;
}
int sendFileToClient(char *fileName, struct sockaddr_in clientaddr,
		socklen_t clientlen, int sockfd) {
	int n = 0;
	transfer_packet *dataPacket = (transfer_packet*) malloc(
			sizeof(transfer_packet));
	struct stat s;

	if (stat(fileName, &s) == -1) {
		printf("File info not available");
		return -1;
	}
	dataPacket->fileSize = (int) s.st_size;
	n = sendto(sockfd, (char*) dataPacket, sizeof(transfer_packet), 0,
			(struct sockaddr *) &clientaddr, clientlen);
	if (n < 0)
		error("ERROR in sendto");
	FILE *f = fopen(fileName, "rb");
	if (!f) {
		printf("Cannot read from file");
		return -1;
	}
	n = recvfrom(sockfd, (char*) dataPacket, sizeof(transfer_packet), 0,
			(struct sockaddr *) &clientaddr, &clientlen);
	if (n < 0)
		error("ERROR in recvfrom");
	n = sendAll(sockfd, f, s, clientaddr, clientlen, fileName);
	if (n < 0) {
		error("Could not complete the file transfer");
		return -1;
	}
	fclose(f);
	free(dataPacket);
	return 1;
}
int sendAll(int sockfd, FILE *f, struct stat s, struct sockaddr_in clientaddr,
		socklen_t clientlen, char *fileName) {
//tell server that I will send a file of name provided
	int64_t size = s.st_size;
	int n;
	transfer_packet *dataPacket = (transfer_packet*) malloc(
			sizeof(transfer_packet));
	printf("File size is %d \n", (int) size);
	dataPacket->fileSize = (int) size;
	int sizeToBeSentPerIteration = 0, totalSizeSent = 0;
	while (size > 0) {
		bzero(buf, BUFSIZE);
		sizeToBeSentPerIteration = min(sizeof(buf), size);
		printf("Reading %d bytes", sizeToBeSentPerIteration);
		bzero(dataPacket->data,BUFSIZE);
		int readFromFile = fread(dataPacket->data, sizeToBeSentPerIteration,1, f);
		if (readFromFile < 0) {
			printf("The file sequence cannot be read \n");
			return -1;
		}
		sendto(sockfd, (char*) dataPacket, sizeof(transfer_packet), 0,
				(struct sockaddr *) &clientaddr, clientlen);
		printf("\n Sent datagram \n");
		//getting ack back from the server
		n = recvfrom(sockfd, (char*) dataPacket, sizeof(transfer_packet), 0,
				(struct sockaddr *) &clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");
		printf("Echo from client: %s \n", dataPacket->data);

		size = size - sizeToBeSentPerIteration;
		totalSizeSent = totalSizeSent + sizeToBeSentPerIteration;
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
int writeIntoFile(char *buf, char *fileName,int n) {
	int writingIntoFile;
	fflush(stdout);
	FILE *openingFileForWriting = fopen(fileName, "ab");
	if (!openingFileForWriting) {
		printf("Cannot open file to execute write operation");
	} else {
		writingIntoFile = fwrite(buf, 1, n, openingFileForWriting);
		fflush(openingFileForWriting);
		printf(" \n \n Wrote %d out of %d into file \n \n", writingIntoFile,
				n);
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
