#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

#include <semaphore.h>

#define COMMUNICATION_BUFFER_SIZE 1024
#define JOB_BUFFER_SIZE 100

struct sensor_type
{
   char  name[256];
   unsigned int  keep_alive;
   double  min_x;
   double  min_y;
   double  min_z;   // 0 for gps sensor (unused)
   double  max_x;
   double  max_y;
   double  max_z;   // 0 for gps sensor (unused)
};

struct sensor_instance
{
    long int id;
    struct sensor_type *type;
    struct sockaddr_in *client_info;//ip adresa klijenta, tako nesto?!? struct sockaddr_in *client_info?
    long int last_updated_ts; //unix timestamp in miliseconds gettimeofday() system call
    //mutex
    //struct sensor_instance *next;
};

typedef struct
{
    char actual_job[COMMUNICATION_BUFFER_SIZE];
    struct sockaddr_in client_info;
} sensor_job;

typedef struct
{
    sensor_job jobs[JOB_BUFFER_SIZE];
    int next_in;
    int next_out;
    sem_t access;
    sem_t free;
    sem_t occupied;
} sensor_job_buffer;



void initialize_job_buffer(sensor_job_buffer* buff)
{
    int ret;

    buff->next_in = 0;
    buff->next_out = 0;

    ret = sem_init(&buff->access, 0, 1);
    ret = sem_init(&buff->free, 0, JOB_BUFFER_SIZE);
    ret = sem_init(&buff->occupied, 0, 0);
    //to do: check ret value
}

void destroy_job_buffer(sensor_job_buffer* buff)
{
    int ret;

    sem_destroy(&buff->access);
    sem_destroy(&buff->free);
    sem_destroy(&buff->occupied);
}

enum sensor_enum
{
    accelerometer,
    gyroscope,
    magnetometer,
    gps,
    NUMBER_OF_SENSOR_TYPES
};

#endif // SENSORS_H_INCLUDED
