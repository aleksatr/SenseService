#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED
#include "sensors.h"
#include <pthread.h>

struct queue_si
{
    int numberOfElements;
    struct sensor_instance *head, *tail;
    pthread_mutex_t mutex;
};

char queue_isEmpty(struct queue_si *q)
{
    return q->numberOfElements == 0;
}

void queue_initialize(struct queue_si *q)
{
    q->numberOfElements = 0;
    q->head = q->tail = 0;
    pthread_mutex_init(&q->mutex, 0);
}

void queue_destroy(struct queue_si *q)
{
    //TODO: odradi ovo, oslobodi memoriju
    struct sensor_instance *si = q->head;
    while(si)
    {
        si = queue_removeWithId(q, si->id);
    }
    pthread_mutex_destroy(&q->mutex);
}

void queue_enqueue(struct queue_si *q, struct sensor_instance *si)
{
    if(!si)
        return;
    pthread_mutex_lock(&q->mutex);
    if(q->numberOfElements)
    {
        q->tail->next = si;
		q->tail = tail->next;
	} else
	{
        head = tail = si;
	}
	q->numberOfElements++;
    pthread_mutex_unlock(&q->mutex);
}

struct sensor_instance* queue_getWithPosition(struct queue_si* q, int position)
{
    struct sensor_instance *si;
    int i;
    pthread_mutex_lock(&q->mutex);
    si = q->head;
    for(i = 0; i < position && !si; i++)
        si = si->next;
    pthread_mutex_unlock(&q->mutex);
    return si;
}

struct sensor_instance* queue_getWithId(struct queue_si *q, long int id)
{
    struct sensor_instance *si;
    pthread_mutex_lock(&q->mutex);
    for(si = q->head; si != 0 && si->id != id; si = si->next);
    pthread_mutex_unlock(&q->mutex);
    return si;
}

struct sensor_instance* queue_removeWithId(struct queue_si *q, long int id)
{
    struct sensor_instance *si, *tmp;
    pthread_mutex_lock(&q->mutex);
    si = q->head;
    if(si->id == id)
    {
        q->head = si->next;
        if(q->tail == si)
            q->tail = 0;
        si->next = 0;
        q->numberOfElements--;
        pthread_mutex_unlock(&q->mutex);
        return si;
    }
    tmp = si;
    for(si = si->next; si != 0; si = si->next)
    {
        if(si->id == id)
        {
            tmp->next = si->next;
            if(si == q->tail)
            {
                q->tail = tmp;
            }
            tmp->next = si->next;
            si->next = 0;
            q->numberOfElements--;

            pthread_mutex_unlock(&q->mutex);
            return si;
        }
        tmp = si;
    }
    pthread_mutex_unlock(&q->mutex);
    return si;
}

unsigned int queue_calculateSleepTime(struct queue_si *q, unsigned int timeFromConfig, unsigned int currentTimeStamp)
{
    int i, passedTime;
    struct sensor_instance *si;
    pthread_mutex_lock(&q->mutex);
    si = q->head;
    for(i = 0; i < q->numberOfElements && !si ; i++)
    {
        passedTime = (si->type->keep_alive * 1000 - currentTimeStamp - si->last_updated_ts);
        if(timeFromConfig > passedTime)
            timeFromConfig = passedTime;
    }
    pthread_mutex_unlock(&q->mutex);
    return timeFromConfig;
}

#endif // QUEUE_H_INCLUDED
