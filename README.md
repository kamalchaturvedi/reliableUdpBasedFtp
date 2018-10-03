--------------------------------------------------------------------------------------
		Connectionless file transfer protocol(UDP FTP) Implementation
---------------------------------------------------------------------------------------
Author: Kamal Chaturvedi (kamalchaturvedi15@gmail.com)
---------------------------------------------------------------------------------------
This program is an implementation of a connectionless file transfer protocol(UDP FTP)

We have a client(client/client.c) & a server(server/server.c), and the client sends the following commands to the server to perform these tasks

1. Add a file to the server
2. Get a file from the server
3. List files in the server
4. Delete a file from the server
5. Exit server

Client is a command-line utility to select a task number (from above), select file if required & then initiate transfers with the server.

The server just responds to the requests from the client.

The protocol has been implemented in such a way that whenever a packet is sent from the client, it waits for an acknowledgement from the server, and only upon that, it sends the next packet. If no acknowledgement is received within the timeout, a new message is sent. This makes sure that reliability is maintained

What works :

The current client server model works for all the 5 tasks above (I have transferred a 8 MB file without a drop in packet, when both client & server are in different remote VMs)

What doesnt work :

* The only files I can transfer are text files i.e. of type .txt, .c, .java etc, but no binary files can be transferred(jpeg, mp3 etc)

How to create output files:

* Run the makefile in client or server folder by, typing
	make

Running the output

* ./server {portNo}
* ./client {serverhostaddress} {serverportno}

-------------------------------------------------------------------------------------------
