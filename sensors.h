#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

#include <semaphore.h>
#include <netinet/in.h> //sadrzi definiciju struct sockaddr_in
#include <pthread.h>
#define CONF_LINE_LENGTH 512
#define COMMUNICATION_BUFFER_SIZE 1024
#define JOB_BUFFER_SIZE 100
#define PING_BUFF_LEN 64

typedef struct
{
   char  name[256];
   unsigned int  keep_alive;
   double  min_x;
   double  min_y;
   double  min_z;   // 0 for gps sensor (unused)
   double  max_x;
   double  max_y;
   double  max_z;   // 0 for gps sensor (unused)
} sensor_type;

typedef struct sensor_instance
{
    long int id;
    sensor_type *type;
    struct sockaddr_in *client_info;//ip adresa klijenta, tako nesto?!? struct sockaddr_in *client_info?
    long int last_updated_ts; //unix timestamp in miliseconds gettimeofday() system call
    pthread_mutex_t mutex;
    struct sensor_instance *next;
    char pinged;
} sensor_instance;

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

enum sensor_enum
{
    accelerometer,
    gyroscope,
    magnetometer,
    gps,
    NUMBER_OF_SENSOR_TYPES
};

inline char* string_from_sensor_enum(enum sensor_enum se)
{
    const char* strings[] = { "accelerometer", "gyroscope", "magnetometer", "gps"};

    return strings[se];
};


#endif // SENSORS_H_INCLUDED
