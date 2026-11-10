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
	srand(time(NULL));
	int client_socket;
	int connection_status;
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9002);
	inet_pton(AF_INET, "192.168.0.100", &server_addr.sin_addr);
	//server_addr.sin_addr.s_addr = INADDR_ANY;

	// create socket
	CHECK(client_socket = socket(AF_INET, SOCK_STREAM, 0), "failed to create socket");
	
	// connect the socket
	CHECK(connection_status = connect(client_socket,(const struct sockaddr*) &server_addr, sizeof(server_addr)),
										"failed to connect to server");
	
	
	int packet_nbr = 10;
    uint16_t seq_nbr = 0;
	int total_size = 0;
    struct antenna_packet packets[10];

    for(int i = 0 ; i  < packet_nbr ; i++)
    {
	    int total_sent = 0;
		int byte_sent = 0;
		
        CHECK(generate_packet(&packets[i], (uint8_t)i, &seq_nbr),"failed to generate packet");
		total_size = sizeof(packets[i].header) + antenna_packet_payload_bytes(packets[i].header.payload_length)
							 + sizeof(uint16_t);

	    uint16_t sample_count = packets[i].header.payload_length;
		
	    /* convert multibyte fields to network byte order before sending */
	    packets[i].header.magic_number    = htonl(packets[i].header.magic_number);
	    packets[i].header.sequence_number = htons(packets[i].header.sequence_number);
	    packets[i].header.payload_length  = htons(packets[i].header.payload_length);
	    for(int j = 0; j < sample_count; j++)
	    {
	    	packets[i].samples[j] = htons(packets[i].samples[j]);
	    }
		
		size_t crc_data_size = sizeof(packets[i].header) + antenna_packet_payload_bytes(sample_count);
		uint16_t crc = htons(crc16_ccitt_false((const uint8_t *)&packets[i], crc_data_size));
		memcpy((char*)&packets[i] + sizeof(struct antenna_packet_header) + antenna_packet_payload_bytes(sample_count),
				&crc, sizeof(uint16_t));
		
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

        //nanosleep(&delay, NULL); // sleep 100ms
    }

	close(client_socket);
	
	return 0;
}
