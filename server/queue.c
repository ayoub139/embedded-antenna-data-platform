#include <queue.h>


int queue_push(struct packet_queue* queue, const struct antenna_packet* packet)
{
	pthread_mutex_lock(&queue->mutex);
	if(queue->count == Q_CAPACITY)
	{
		pthread_mutex_unlock(&queue->mutex);
		return -1;
	}
	
	queue->items[queue->tail].packet = *packet;
	queue->count++;
	pthread_cond_signal(&queue->not_empty);
	queue->tail = (queue->tail + 1) % Q_CAPACITY;
	pthread_mutex_unlock(&queue->mutex);

	return 1;
}

int queue_pop(struct packet_queue* queue, struct antenna_packet *packet)
{
	pthread_mutex_lock(&queue->mutex);
	//condition variables can wake up without a signal. 
	//Therefore, the worker must check the queue again before taking a packet.
	while (queue->count == 0)
	{
		pthread_cond_wait(&queue->not_empty, &queue->mutex);
	}
	
	*packet = queue->items[queue->head].packet;
	queue->count--;
	queue->head = (queue->head + 1) % Q_CAPACITY;
	pthread_mutex_unlock(&queue->mutex);

	return 1;
}