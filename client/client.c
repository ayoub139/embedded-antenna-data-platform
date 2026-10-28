#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "antenna_simulator.h"


int main()
{
	int client_socket;
	int connection_status;
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9002);
	inet_pton(AF_INET, "192.168.0.100", &server_addr.sin_addr);
	
	// create socket
	CHECK(client_socket = socket(AF_INET, SOCK_STREAM, 0), "failed to create socket");
	
	// connect the socket
	CHECK(connection_status = connect(client_socket,(const struct sockaddr*) &server_addr, sizeof(server_addr)),
										"failed to connect to server");
	
	
	int packet_nbr = 10;
    int seq_nbr = 0;
	int total_size = 0;
    struct antenna_packet packets[10];

    for(int i = 0 ; i  < packet_nbr ; i++)
    {
	    int total_sent = 0;
		int byte_sent = 0;
		total_size = sizeof(packets[i]);
        generate_packet(&packets[i], i, &seq_nbr); 
		
		while(total_sent < total_size)
		{
			char* ptr = (char *)&packets[i] + total_sent;

			byte_sent = send(client_socket, ptr, total_size - total_sent, 0);
			CHECK(byte_sent, "send failed");

			if (byte_sent == 0)
			{
				printf("0 data has been sent\n");
				close(client_socket);
				return -1;
			}
				
			total_sent+=byte_sent;
		}

        nanosleep(&delay, NULL); // sleep 100ms
    }

	close(client_socket);
	
	return 0;
}
