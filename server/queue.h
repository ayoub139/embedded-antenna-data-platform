#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <antenna_simulator.h>

#define Q_CAPACITY 256

struct queue_item {
	struct antenna_packet packet;
};

struct packet_queue{
	struct queue_item items[Q_CAPACITY];
	int head;
	int tail;
	int count; 

	pthread_mutex_t mutex;
	pthread_cond_t  not_empty;
};

int queue_push(struct packet_queue* queue, const struct antenna_packet* packet);
int queue_pop(struct packet_queue* queue, struct antenna_packet *packet);


#endif