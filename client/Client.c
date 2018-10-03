/*
 * udpclient.c - A Reliable UDP client
 * usage: udpclient <host> <port>
 */

#include <bits/stdint-intn.h>
#include <bits/types/FILE.h>
#include <bits/types/struct_timeval.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILESIZE 64
#define RESPSIZE 1024

static char buf[BUFSIZE];
typedef struct packet_tr {
	int type;
	int sequenceNo;
	int fileSize;
	char data[RESPSIZE];
	char fileName[FILESIZE];
} transfer_packet;
/*
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(0);
}
int prepareToSendFileToServer(int, struct sockaddr_in, int, char *);
int prepareToExitServer(int, struct sockaddr_in, int);
int prepareToDeleteFileFromServer(int, struct sockaddr_in, int, char *);
int prepareToGetListFromServer(int, struct sockaddr_in, int);
int writeIntoFile(char *, char *, int);
int sendAll(int, FILE *, struct stat, struct sockaddr_in, int, char *);
int min(int, int);
int stringToInt(char *);
int sendAndReceiveReliably(int, char *, int, int, struct sockaddr_in);
static int sequenceCounter = 0;
int main(int argc, char **argv) {
	int sockfd, portno, n;
	int serverlen;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
	char fileSelected[FILESIZE];
	int taskChosen = 0;

	/* check command line arguments */
	if (argc != 3) {
		fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
		exit(0);
	}
	hostname = argv[1];
	portno = atoi(argv[2]);

	/* socket: create the socket */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	/* gethostbyname: get the server's DNS entry */
	server = gethostbyname(hostname);
	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host as %s\n", hostname);
		exit(0);
	}

	/* build the server's Internet address */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *) server->h_addr,
	(char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(portno);
	serverlen = sizeof(serveraddr);

	while (1) {
		printf(
				"Hello there \n Enter the task number from below to execute \n 1. Add a file to the server\n 2. Get a file from the server\n 3. List files in the server \n 4. Delete a file from the server\n 5. Exit server\n");
		scanf("%d", &taskChosen);
		printf("\nTask chosen is %d \n", taskChosen);
		switch (taskChosen) {
		case 1:
			printf("\nEnter the file name to be pushed to server\n");
			scanf("%s", fileSelected);
			prepareToSendFileToServer(sockfd, serveraddr, serverlen,
					fileSelected);
			break;
		case 2:
			printf("\nEnter the file name to be pulled from server\n");
			scanf("%s", fileSelected);
			prepareToGetFromServer(sockfd, serveraddr, serverlen, fileSelected);
			break;
		case 3:
			prepareToGetListFromServer(sockfd, serveraddr, serverlen);
			break;
		case 4:
			printf(
					"\nEnter the file name of the file to be deleted from server\n");
			scanf("%s", fileSelected);
			prepareToDeleteFileFromServer(sockfd, serveraddr, serverlen,
					fileSelected);
			break;
		case 5:
			prepareToExitServer(sockfd, serveraddr, serverlen);
			break;
		default:
			break;
		}
	}

	return 0;
}

int prepareToExitServer(int sockfd, struct sockaddr_in serveraddr,
		int serverlen) {
	int n = 0;
	transfer_packet *packet = (transfer_packet *) malloc(
			sizeof(transfer_packet));
	packet->type = 5;
	packet->sequenceNo = sequenceCounter++;
	n = sendAndReceiveReliably(sockfd, (char *) packet, sizeof(transfer_packet),
			serverlen, serveraddr);
	free(packet);
	return n;
}
int prepareToDeleteFileFromServer(int sockfd, struct sockaddr_in serveraddr,
		int serverlen, char *fileName) {
	int n = 0;
	transfer_packet *packet = (transfer_packet *) malloc(
			sizeof(transfer_packet));
	packet->type = 4;
	packet->sequenceNo = sequenceCounter++;
	strcpy(packet->fileName, fileName);
	n = sendAndReceiveReliably(sockfd, (char *) packet, sizeof(transfer_packet),
			serverlen, serveraddr);
	free(packet);
	return n;
}
int prepareToGetListFromServer(int sockfd, struct sockaddr_in serveraddr,
		int serverlen) {
	int n = 0;
	transfer_packet *packet = (transfer_packet *) malloc(
			sizeof(transfer_packet));
	packet->type = 3;
	packet->sequenceNo = sequenceCounter++;
	n = sendAndReceiveReliably(sockfd, (char *) packet, sizeof(transfer_packet),
			serverlen, serveraddr);
	free(packet);
	return n;
}
int prepareToGetFromServer(int sockfd, struct sockaddr_in serveraddr,
		int serverlen, char *fileName) {
	int n = 0;
	transfer_packet *packet = (transfer_packet *) malloc(
			sizeof(transfer_packet));
	packet->type = 2;
	packet->sequenceNo = sequenceCounter++;
	int fileSize = 0, sizeReceived = 0;
	strcpy(packet->fileName, fileName);
	n = sendAndReceiveReliably(sockfd, (char *) packet, sizeof(transfer_packet),
			serverlen, serveraddr);
	fileSize = n;
	printf("Size of the file is %d", fileSize);
	bzero(packet->data, RESPSIZE);
	strcpy(packet->data, "ok");
	sendto(sockfd, (char *) packet, sizeof(transfer_packet), 0,
			(struct sockaddr *) &serveraddr, serverlen);
	printf("\n Sent datagram \n");
	while (sizeReceived < fileSize) {
		bzero(packet->data, RESPSIZE);
		bzero(buf, BUFSIZE);
		recvfrom(sockfd, (char*) packet, sizeof(transfer_packet), 0,
				(struct sockaddr *) &serveraddr, &serverlen);
		writeIntoFile(packet->data, fileName, sizeof(packet->data));
		bzero(packet->data, RESPSIZE);
		sprintf(buf, "Received %d bytes from file %s", n, fileName);
		strcpy(packet->data, buf);
		sendto(sockfd, (char*) packet, sizeof(transfer_packet), 0,
				(struct sockaddr *) &serveraddr, serverlen);
		printf("\n Sent datagram \n");
		sizeReceived += n;
	}

	return n;
}

int prepareToSendFileToServer(int sockfd, struct sockaddr_in serveraddr,
		int serverlen, char *fileName) {
	struct stat s;
	//start sending one by one until all is sent
	if (stat(fileName, &s) == -1) {
		printf("File info not available");
		return -1;
	}
	FILE *f = fopen(fileName, "rb");
	if (!f) {
		printf("Cannot read from file");
		return -1;
	}
	int n = sendAll(sockfd, f, s, serveraddr, serverlen, fileName);
	if (n < 0) {
		error("Could not complete the file transfer");
		return -1;
	}
	fclose(f);
	return 1;
}

int sendAll(int sockfd, FILE *f, struct stat s, struct sockaddr_in serveraddr,
		int serverlen, char *fileName) {
	int64_t size = s.st_size;
	int n, sequenceNo;
	printf("File size is %d \n", size);
	int fileSizeSent = 0, sizeToBeSentPerIteration = 0, totalSizeSent = 0,
			actionSize = 0;
	transfer_packet *packet;
	while (size > 0) {
		packet = (transfer_packet *) malloc(sizeof(transfer_packet));
		sequenceNo = sequenceCounter++;
		bzero(buf, BUFSIZE);
		sizeToBeSentPerIteration = min(sizeof(buf), size);
		printf("Reading %d bytes", sizeToBeSentPerIteration);
		int readFromFile = fread(buf, 1, sizeToBeSentPerIteration, f);
		if (readFromFile < 0) {
			printf("The file sequence cannot be read \n");
			return -1;
		}
		fileSizeSent = sizeToBeSentPerIteration;
		packet->type = 1;
		packet->sequenceNo = sequenceCounter++;
		strcpy(packet->data, buf);
		strcpy(packet->fileName, fileName);
		sendAndReceiveReliably(sockfd, (char *) packet, sizeof(transfer_packet),
				serverlen, serveraddr);
		free(packet);
		size = size - fileSizeSent;
		totalSizeSent = totalSizeSent + fileSizeSent;
	}
	printf("Successfully sent the file to server : %d bytes! \n",
			totalSizeSent);
	return 1;
}
int sendAndReceiveReliably(int sockfd, char *action, int sizeToBeSent,
		int serverlen, struct sockaddr_in serveraddr) {
	struct timeval wait;
	fd_set readTemplate;
	int n, sentSize;
	transfer_packet *response = (transfer_packet*) malloc(
			sizeof(transfer_packet));
	while (1) {
		sendto(sockfd, action, sizeToBeSent, 0, (struct sockaddr*) &serveraddr,
				serverlen);
		printf("\n Sent datagram \n");
		wait.tv_sec = 10;
		wait.tv_usec = 0;
		FD_ZERO(&readTemplate);
		FD_SET(sockfd, &readTemplate);
		if ((n = select(FD_SETSIZE, &readTemplate, NULL, NULL, &wait)) <= 0) {
			printf("Timed out : sending packet again\n");
		} else if (FD_ISSET(sockfd, &readTemplate)) {
			if (recvfrom(sockfd, (char*) response, sizeof(transfer_packet), 0,
					(struct sockaddr*) &serveraddr, &serverlen) < 0) {
				perror("Error in server acknowledgment");
				exit(1);
			}
			printf("Echo from server: %s \n", response->data);
			break;
		}
	}
	return 1;
}
int writeIntoFile(char *buf, char *fileName, int n) {
	int writingIntoFile;
	FILE *openingFileForWriting = fopen(fileName, "ab");
	if (!openingFileForWriting) {
		printf("Cannot open file to execute write operation");
	} else {
		writingIntoFile = fwrite(buf, 1, n, openingFileForWriting);
		fflush(openingFileForWriting);
		printf(" \n \n Wrote %d out of %d into file \n \n", writingIntoFile,
				strlen(buf));
		if (writingIntoFile < 0) {
			printf("\n\n The file sequence cannot be written \n\n");
			return -1;
		}
		fclose(openingFileForWriting);
	}
	return 1;
}

int min(int val0, int val1) {
	if (val0 < val1)
		return val0;
	else
		return val1;
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
