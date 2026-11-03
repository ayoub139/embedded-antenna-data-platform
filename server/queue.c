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
	pthread_cond_broadcast(&queue->not_empty);
	queue->tail = (queue->tail + 1) % Q_CAPACITY;
	pthread_mutex_unlock(&queue->mutex);

	return 1;
}

int queue_pop(struct packet_queue* queue, struct antenna_packet *packet)
{
	pthread_mutex_lock(&queue->mutex);
	//condition variables can wake up without a signal. 
	//Therefore, the worker must check the queue again before taking a packet.
	//We use a while here and not if because a thread can wake up even if the 
	//condition variable is no longer true ( it's called a spurious wakeup)
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