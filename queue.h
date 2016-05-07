#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED
#include "sensors.h"
#include <pthread.h>

typedef struct queue_si
{
    int numberOfElements;
    sensor_instance *head, *tail;
    pthread_mutex_t mutex;
};

sensor_instance* queue_removeWithId(struct queue_si *q, long int id);

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

void sensor_instance_destroy(sensor_instance *si)
{
    if(si)
    {
        free(si->client_info);
        pthread_mutex_destroy(&si->mutex);
        free(si);
    }
}

void queue_destroy(struct queue_si *q)
{
    int id;
    sensor_instance *si = q->head;
    while(si)
    {
        id = si->next->id;
        sensor_instance_destroy(si);
        si = queue_removeWithId(q, id);
    }
    pthread_mutex_destroy(&q->mutex);
}

void queue_enqueue(struct queue_si *q, sensor_instance *si)
{
    if(!si)
        return;

    pthread_mutex_lock(&q->mutex);

    if(q->numberOfElements)
    {
        q->tail->next = si;
		q->tail = q->tail->next;
	} else
	{
        q->head = q->tail = si;
	}

	q->numberOfElements++;
    pthread_mutex_unlock(&q->mutex);
}

sensor_instance* queue_getWithPosition(struct queue_si* q, sensor_instance* si)
{
    sensor_instance* ret;
    pthread_mutex_lock(&q->mutex);
    if(!si)
        ret = q->head;
    else
        ret = si->next;
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

sensor_instance* queue_getWithId(struct queue_si *q, long int id)
{
    sensor_instance *si;
    pthread_mutex_lock(&q->mutex);
    for(si = q->head; si != 0 && si->id != id; si = si->next);
    pthread_mutex_unlock(&q->mutex);
    return si;
}

sensor_instance* queue_getWithIdType(struct queue_si *q, long int id, const char *type)
{
    sensor_instance *si;
    pthread_mutex_lock(&q->mutex);
    for(si = q->head; si != 0 && si->id != id && strcasecmp(type, si->type->name); si = si->next);
    pthread_mutex_unlock(&q->mutex);
    return si;
}

sensor_instance* queue_removeWithId(struct queue_si *q, long int id)
{
    sensor_instance *si, *tmp;
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
            //tmp->next = si->next;
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

long int queue_calculateSleepTime(struct queue_si *q, long int timeFromConfig, long int currentTimeStamp)
{
    long int i, passedTime;
    sensor_instance *si;
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
