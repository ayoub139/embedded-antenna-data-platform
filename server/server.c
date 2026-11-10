#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include "antenna_simulator.h"
#include <queue.h>

#define THREAD_POOL_SIZE 4
#define MAX_EVENTS 10

struct client_state {
	int socket;
	char buffer[sizeof(struct antenna_packet)];
	size_t recv_bytes;
	size_t expected_size;
};

enum event_type{
	EVENT_SERVER,
	EVENT_CLIENT
};

struct event_data {
	enum event_type type;
	struct client_state *client;
};

void* worker_function(void* arg)
{
	struct packet_queue *queue = arg;
	struct antenna_packet packet;
	while(true)
	{
		queue_pop(queue, &packet);

        printf("Worker processing packet ID: %u\n", packet.header.id);
        printf("Sequence number: %u\n", packet.header.sequence_number);

        printf("Data:\n");
        for (uint16_t i = 0; i < packet.header.payload_length; i++)
        {
            printf("%u ", packet.samples[i]);
        }
        printf("\n");
    }

    return NULL;
} 

void cleanup_client(int epoll_fd, struct event_data* event_data)
{
	struct client_state* client = event_data->client;
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->socket, NULL);
	close(client->socket);
	free(client);
	free(event_data);
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

	// making my server socket non blocking 
	int flags = fcntl(server_socket, F_GETFL, 0);
	CHECK(flags, "fcntl F_GETFL failed");
	CHECK(fcntl(server_socket, F_SETFL, flags | O_NONBLOCK), "fcntl F_SETFL failed");

	// bind
	CHECK(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)), "bind failed");

	// listen on port 9002
	CHECK(listen(server_socket, 1500), "listen failed");

	// Create an epoll instance
	int epoll_fd = epoll_create1(0);
	CHECK(epoll_fd, "epoll_create failed");

	struct event_data server_event_data = {
		.type = EVENT_SERVER,
		.client = NULL
	};

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = &server_event_data;
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event), "failed");
	
	struct epoll_event events[MAX_EVENTS];

	struct packet_queue queue = {0};

	if (pthread_mutex_init(&queue.mutex, NULL) != 0)
	{
		fprintf(stderr, "Failed to initialize queue mutex\n");
		return 1;
	}

	if (pthread_cond_init(&queue.not_empty, 0))
	{
		fprintf(stderr, "failed to initialize queue condition variable");
		pthread_mutex_destroy(&queue.mutex);
		return -1;
	}

	pthread_t pool[THREAD_POOL_SIZE];
	for(int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		if(pthread_create(&pool[i], NULL, worker_function, &queue) != 0)
		{
			fprintf(stderr, "failed to create thread\n");
			return -1;
		}
	}
	
	printf("Waiting for connections sockets\n");

	while (true)
	{
		int ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		CHECK(ready, "epoll_wait failed");
		for(int i = 0; i < ready; i++)
		{
			struct event_data *event_data = events[i].data.ptr;
			if (event_data->type == EVENT_SERVER)
			{
				// Keep accepting connections until the non-blocking socket returns EAGAIN/EWOULDBLOCK,
				// meaning there are no more pending connections to accept. Then return to epoll_wait().
				while(true)
				{
					//Accept client sockets 
					int client_socket;
					client_socket = accept(server_socket, NULL, NULL);

					if (client_socket == -1)
					{
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
						{
							break;
						}

						perror("accept failed\n");
						break;
					}

					printf("Connected\n");
			    
					// make my client non blocking
					// recv() may return after reading only part of a packet.
					// epoll_wait() lets the server handle other clients while waiting for the remaining bytes.
					int client_socket_flags = fcntl(client_socket, F_GETFL, 0);
					CHECK(client_socket_flags, "F_GETFL client flags failed");
					CHECK(fcntl(client_socket, F_SETFL, client_socket_flags | O_NONBLOCK), "F_SETFL client flags failed");

					struct client_state *client_state = malloc(sizeof(struct client_state));
					if (client_state == NULL)
					{
						perror("malloc client_state failed\n");
						close(client_socket);
						continue;
					}
					client_state->socket = client_socket;
					client_state->recv_bytes = 0;
					client_state->expected_size = sizeof(struct antenna_packet_header);
					struct event_data *client_event_data = malloc(sizeof(struct event_data));
					if (client_event_data == NULL)
					{
						perror("malloc client_event_data failed\n");
						free(client_state);
						close(client_socket);
						continue;
					}
					client_event_data->type = EVENT_CLIENT;
					client_event_data->client = client_state;
					
					struct epoll_event event_client;
					event_client.events = EPOLLIN;
					event_client.data.ptr = client_event_data;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event_client) == -1)
					{
						perror("epoll_ctl failed");
						close(client_socket);
						free(client_event_data);
						free(client_state);
						continue;
					}
				}			
			}
			else
			{
				struct client_state* client = event_data->client;
				ssize_t n = recv(
					client->socket,
					client->buffer + client->recv_bytes,
					client->expected_size - client->recv_bytes,
					0
				);				
				if (n > 0)
				{
					client->recv_bytes += n;
				}
				else if (n == 0)
				{
					printf("Client disconnected\n");
					cleanup_client(epoll_fd, event_data);
					continue;
				}
				else if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
					continue;
				}
				else
				{
					perror("recv failed");
					cleanup_client(epoll_fd, event_data);
					continue;
				}

				if (client->recv_bytes == sizeof(struct antenna_packet_header))
				{
					struct antenna_packet_header header;
					memcpy(&header, client->buffer, sizeof(header));
					uint16_t payload_length = ntohs(header.payload_length);
					if (payload_length > MAX_SAMPLES_PER_PACKET)
					{
						fprintf(stderr, "invalid packet payload length; packet dropped\n");
						client->recv_bytes = 0;
						continue;
					}
					client->expected_size = sizeof(struct antenna_packet_header) +
						antenna_packet_payload_bytes(payload_length) +
						sizeof(uint16_t);
				}

				if (client->recv_bytes == client->expected_size)
				{
					struct antenna_packet packet;
					memset(&packet, 0, sizeof(packet));
					memcpy(&packet.header, client->buffer, sizeof(struct antenna_packet_header));
					memcpy(&packet.samples, client->buffer + sizeof(struct antenna_packet_header),
						antenna_packet_payload_bytes(ntohs(packet.header.payload_length)));
					memcpy(&packet.crc, client->buffer + sizeof(struct antenna_packet_header) +
						antenna_packet_payload_bytes(ntohs(packet.header.payload_length)), sizeof(uint16_t));

					uint16_t sample_count = ntohs(packet.header.payload_length);
					if (sample_count > MAX_SAMPLES_PER_PACKET)
					{
						fprintf(stderr, "invalid packet payload length; packet dropped\n");
						client->recv_bytes = 0;
						client->expected_size = sizeof(struct antenna_packet_header);
						continue;
					}

					// Magic number and version verification
					uint32_t magic_number = ntohl(packet.header.magic_number);
					if (magic_number != ANTENNA_PROTOCOL_MAGIC ||
						packet.header.version != ANTENNA_PROTOCOL_VERSION)
					{
						fprintf(stderr, "invalid packet header; packet dropped\n");
						client->recv_bytes = 0;
						client->expected_size = sizeof(struct antenna_packet_header);
						continue;
					}

					// CRC verification
					size_t crc_data_size = sizeof(packet.header) + antenna_packet_payload_bytes(sample_count);
					uint16_t received_crc = ntohs(packet.crc);
					uint16_t calculated_crc = crc16_ccitt_false((const uint8_t *)client->buffer, crc_data_size);					
					if (received_crc != calculated_crc)
					{
						fprintf(stderr, "packet CRC mismatch; packet dropped\n");
						client->recv_bytes = 0;
						client->expected_size = sizeof(struct antenna_packet_header);
						continue;
					}

					packet.header.magic_number = magic_number;
					packet.header.sequence_number = ntohs(packet.header.sequence_number);
					packet.header.payload_length = sample_count;
					packet.crc = received_crc;
					for (uint16_t j = 0; j < sample_count; j++)
					{
						packet.samples[j] = ntohs(packet.samples[j]);
					}

					if (queue_push(&queue, &packet) == -1)
					{
						fprintf(stderr, "queue is full : packet dropped\n");
					}

					client->recv_bytes = 0;
					client->expected_size = sizeof(struct antenna_packet_header);
				}
			}
		}
				
	}

	return 0;
}
