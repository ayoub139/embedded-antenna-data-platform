#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include "antenna_simulator.h"
#include <queue.h>

#define MAX_EVENTS 10

struct client_state {
	int socket;
	char buffer[sizeof(struct antenna_packet)];
	int recv_bytes;
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

        printf("Worker processing packet ID: %d\n", packet.id);
        printf("Sequence number: %d\n", packet.sequence_number);

        printf("Data:\n");
        for (int i = 0; i < MAX_SAMPLES_PER_PACKET; i++)
        {
            printf("%d ", packet.samples[i]);
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
	CHECK(listen(server_socket, 100), "listen failed");

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

	pthread_t worker;
	if(pthread_create(&worker, NULL, worker_function, &queue) != 0)
	{
		fprintf(stderr, "failed to create thread\n");
		return -1;
	}


	while (true)
	{
		printf("Waiting for connections sockets\n");
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
					sizeof(struct antenna_packet) - client->recv_bytes,
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

				if (client->recv_bytes == sizeof(struct antenna_packet))
				{
					struct antenna_packet *packet =
						(struct antenna_packet *)client->buffer;

					if(queue_push(&queue, packet) == -1)
					{
						fprintf(stderr, "queue is full : packet dropped\n");
					}

					client->recv_bytes = 0;
				}
			}
		}
				
	}

	return 0;
}
