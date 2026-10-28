#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include "antenna_simulator.h"

int handle_connection(int client_socket)
{
	printf("Client is connected\n");
	struct antenna_packet packet;
	ssize_t recv_bytes = -1;
	ssize_t total_recv = 0;
	while(recv_bytes != 0)
	{
		while(total_recv < sizeof(packet))
		{
			char *ptr = (char *)&packet + total_recv;
			CHECK(recv_bytes = recv(client_socket, ptr, sizeof(packet) - total_recv, 0),
											"failed to receive bytes");
			if (recv_bytes == 0)
			{
				printf("Client disconnected\n");
				close(client_socket);
				return -1;
			}
			total_recv+=recv_bytes;
		}

		printf("Packet ID: %d\n", packet.id);
		printf("Sequence number: %d\n", packet.sequence_number);
		printf("Data:\n");
		for(int i = 0; i < MAX_SAMPLES_PER_PACKET; i++)
		{
			printf("%d ",packet.samples[i]);
		}
		printf("\n");
		total_recv = 0;
	}
	
	close(client_socket);

	return 1;
}

int main()
{
	int server_socket; 
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9002);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// create socket 
	CHECK(server_socket = socket(AF_INET, SOCK_STREAM, 0), "failed to create socket");

	// bind
	CHECK(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)), "bind failed");

	// listen on port 9002
	CHECK(listen(server_socket, 5), "listen failed");

	while (true)
	{
		printf("Waiting for connections sockets\n");
		
		//Accept client sockets 
		int client_socket;
		CHECK(client_socket = accept(server_socket, NULL, NULL), "failed to accept sockets");
		printf("Connected\n");
		handle_connection(client_socket);
	}

	return 0;
}
