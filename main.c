#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>  //sadrzi inet_ntoa()
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <limits.h>
#include "./libs/cJSON/cJSON.h"
#include "data_layer.h"
#include "queue.h"


#define SERVICE_CONF_FNAME "service.conf"
#define SENSORS_CONF_FNAME "sensors.conf"

void load_service_conf();
void load_sensors_conf();
void exit_cleanup();
void sig_int_handler();
void create_worker_threads();
void create_job_buffers();
void do_work(void *job_buffer_ptr);
void initialize_job_buffer(sensor_job_buffer* buff);
void destroy_job_buffer(sensor_job_buffer* buff);
void checkingForKeepAliveTimeInterval();
unsigned long int getMilisecondsFromTS();

unsigned short int my_udp_port = 3333;
unsigned short int my_anomaly_port = 6565;
unsigned short int gc_limit = 20;
unsigned short int db_port = 3306;
unsigned short int worker_threads_num = 10;
unsigned short int timeout_factor = 2;
char db_host[CONF_LINE_LENGTH/2] = {0};
char db_name[CONF_LINE_LENGTH/2] = {0};
char db_user[CONF_LINE_LENGTH/2] = {0};
char db_pass[CONF_LINE_LENGTH/2] = {0};
char communication_buffer[COMMUNICATION_BUFFER_SIZE] = {0};
sensor_type sensor_types[NUMBER_OF_SENSOR_TYPES];
struct queue_si *q = 0;
struct queue_si que;
pthread_t queue_thread;
pthread_t *worker_threads = 0;          //workers
sensor_job_buffer *job_buffers = 0;     //job buffers
int curr_thread = 0;
int sock = -1;

//anomaly registration stuff
int anomaly_sock = -1;
struct sockaddr_in anomaly_broadcast;
int anomaly_sin_size;

pthread_mutex_t db_insert_mutex;

int main(int argc, char **argv)
{
    int i, rnb; //size of received message in bytes
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in serverAddress, clientAddress;
    sensor_job_buffer *current_job_buffer;
    sensor_job *job;
    int broadcast_permission;          /* Socket opt to set permission to broadcast */

    //open log
    openlog(NULL, LOG_PID|LOG_CONS, LOG_USER);

    //create queue
    q = &que;
    queue_initialize(q);

    //load config from files
    load_service_conf();
    load_sensors_conf();
    create_job_buffers();
    create_worker_threads();

    //called by exit() for cleanup
    if(atexit(exit_cleanup) != 0)
        syslog(LOG_WARNING, "Can’t register exit_cleanup()...");

    //register signal handlers for cleanup
    signal(SIGTERM, sig_int_handler);   //komanda <kill pid> salje SIGTERM (<kill -9 pid> salje SIGKILL i ne moze da se handleuje)
    signal(SIGINT, sig_int_handler);    //<Ctrl + C> salje SIGINT
    signal(SIGQUIT, sig_int_handler);

    //scanf("%d", &i);

    bzero((char*) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(my_udp_port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(serverAddress.sin_zero), '\0', 8);

    bzero((char*) &anomaly_broadcast, sizeof(anomaly_broadcast));
    anomaly_broadcast.sin_family = AF_INET;
    anomaly_broadcast.sin_port = htons(my_anomaly_port);
    anomaly_broadcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&(anomaly_broadcast.sin_zero), '\0', 8);

    anomaly_sin_size = sizeof(struct sockaddr_in);

    //kreiranje socketa
    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom kreiranja socketa...");
        //closelog();
        exit(1);
    }

    if((anomaly_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom kreiranja anomaly socketa...");
        //closelog();
        exit(1);
    }

    /* Set socket to allow broadcast */
    broadcast_permission = 1;
    if(setsockopt(anomaly_sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcast_permission, sizeof(broadcast_permission)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom postavljanja permisija za broadcast...");
        exit(1);
    }

    if(bind(sock, (struct sockaddr*) &serverAddress, sizeof(struct sockaddr)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom povezivanja socketa...");
        //closelog();
        exit(1);
    }

    while(1)
    {
        //memset((void*)communication_buffer, '\0', COMMUNICATION_BUFFER_SIZE);

        rnb = recvfrom(sock, communication_buffer, COMMUNICATION_BUFFER_SIZE, 0, (struct sockaddr*) &clientAddress, &addrlen);

        //producer
        //printf("%d] Producer produced %s\n", curr_thread, communication_buffer);

        current_job_buffer = &job_buffers[curr_thread];
        curr_thread = (curr_thread + 1) % worker_threads_num;

        sem_wait(&current_job_buffer->free);
        sem_wait(&current_job_buffer->access);

        job = &current_job_buffer->jobs[current_job_buffer->next_in];

        printf("recv-->%s\n", communication_buffer);
        strcpy(job->actual_job, communication_buffer);
        memcpy(&job->client_info, &clientAddress, addrlen);

        current_job_buffer->next_in = (current_job_buffer->next_in + 1) % JOB_BUFFER_SIZE;

        sem_post(&current_job_buffer->access);
        sem_post(&current_job_buffer->occupied);

    }

    exit(0);
}

void exit_cleanup()
{
    int i = 0, ret;

    for(i = 0; i < worker_threads_num; ++i)
    {
        //TODO: check if worker_threads[i] is initialized at all
        ret = pthread_cancel(worker_threads[i]);
        //TODO: ok if ret==0, else error
    }

    ret = pthread_cancel(queue_thread);

    for(i = 0; i < worker_threads_num; ++i)
        destroy_job_buffer(&job_buffers[i]);

    if(sock >= 0)
        close(sock);

    if(anomaly_sock >= 0)
        close(anomaly_sock);

    if(worker_threads != 0)
        free(worker_threads);

    if(job_buffers != 0)
        free(job_buffers);

    if(q)
        queue_destroy(q);

    pthread_mutex_destroy(&db_insert_mutex);

    syslog(LOG_INFO, "exiting...");

    closelog();
}

void sig_int_handler()
{
    exit(0);
}

void create_job_buffers()
{
    //struct sensor_job_buffer *job_buffers = 0;
    int i = 0;

    job_buffers = (sensor_job_buffer*) malloc(sizeof(sensor_job_buffer) * worker_threads_num);

    if(job_buffers == 0)
    {
        syslog(LOG_ERR, "malloc() failed to allocate memory for job buffers...");
        exit(1);
    }

    for(i = 0; i < worker_threads_num; ++i)
        initialize_job_buffer(&job_buffers[i]);
}

void create_worker_threads()
{
    int i = 0, ret;

    worker_threads = (pthread_t*) malloc(sizeof(pthread_t) * worker_threads_num);

    if(worker_threads == 0)
    {
        syslog(LOG_ERR, "malloc() failed to allocate memory for worker thread handles...");
        exit(1);
    }

    for(i = 0; i < worker_threads_num; ++i)
    {
        ret = pthread_create(&worker_threads[i], 0, (void*) do_work, (void*) &job_buffers[i]);
        //TODO: ok if ret==0, else error
    }

    ret = pthread_create(&queue_thread, 0, (void*) checkingForKeepAliveTimeInterval, 0);
    if(ret != 0)
    {
        syslog(LOG_ERR, "could not create thread");
        exit(1);
    }

}

void do_work(void *job_buffer_ptr)
{
    cJSON *root, *types_array;
    char local_buff[COMMUNICATION_BUFFER_SIZE] = {0};
    char send_buff[COMMUNICATION_BUFFER_SIZE] = {0};
    char *requested_types[NUMBER_OF_SENSOR_TYPES] = {0};
    char *request_type, subscribeAnomaly = 0;
    char *output_buffer = 0;
    char anomaly_buffer[2 * COMMUNICATION_BUFFER_SIZE] = {0};
    char description[COMMUNICATION_BUFFER_SIZE] = {0};
    char subscribe_json[COMMUNICATION_BUFFER_SIZE] = {0};
    char subscribed_types[COMMUNICATION_BUFFER_SIZE] = {0};
    char broadcast_buff[COMMUNICATION_BUFFER_SIZE * 2];
    int page_offset = 0, page_size = 0;
    char *anomaly_buff;
    char do_register = 0;
    int i;
    unsigned int sense_id = 0;
    unsigned int id, old_id = -1;
    char *type, *tok;
    double x, y, z;
    sensor_instance *instance = 0;

    int k = -1;

    sensor_job_buffer *my_jobs = (sensor_job_buffer *) job_buffer_ptr;

    syslog(LOG_INFO, "worker thread is born!");

    //consumer
    while(1)
    {
        sem_wait(&my_jobs->occupied);
        sem_wait(&my_jobs->access);

        sensor_job *job = &my_jobs->jobs[my_jobs->next_out];

        sscanf(job->actual_job, "%s", local_buff);

        if(!strcasecmp(local_buff, "subscribe"))
        {
            sscanf(job->actual_job, "%s %u", local_buff, &old_id);

            do_register = 0;

            //subscribe
            if(old_id == -1)
            {
                //create new user in DB
                id = ((unsigned int) time(NULL) << 8) + ((unsigned int) rand() % 256);
                do_register = 1;
            }
            else
            {
                //check DB if user really exists
                if(check_user_exists(old_id, inet_ntoa(job->client_info.sin_addr)))
                {
                    id = old_id;
                }
                else
                {
                    //TODO: register anomaly, id doesn't exist
                    subscribeAnomaly = 1;
                    id = ((unsigned int) time(NULL) << 8) + ((unsigned int) rand() % 256);
                    do_register = 1;
                }
            }

            tok = strtok(job->actual_job, "\n");
            tok = strtok(0, "\n");

            subscribed_types[0] = '\0';
            while(tok != 0)
            {
                if(!strcasecmp("accelerometer", tok))
                {
                    k = accelerometer;
                    strcat(subscribed_types, "\"accelerometer\",");
                } else if(!strcasecmp("gyroscope", tok))
                {
                    k = gyroscope;
                    strcat(subscribed_types, "\"gyroscope\",");
                }
                else if(!strcasecmp("magnetometer", tok))
                {
                    k = magnetometer;
                    strcat(subscribed_types, "\"magnetometer\",");
                }
                else if(!strcasecmp("gps", tok))
                {
                    k = gps;
                    strcat(subscribed_types, "\"gps\",");
                }



                if(k >= 0)
                {
                    instance = (sensor_instance*) malloc(sizeof(sensor_instance));
                    //todo: check if malloc failed
                    instance->last_updated_ts = getMilisecondsFromTS();
                    instance->pinged = 0;
                    instance->next = 0;
                    pthread_mutex_init(&instance->mutex, 0);
                    instance->id = id;
                    instance->client_info = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
                    //todo: check if malloc failed
                    memcpy(instance->client_info, &job->client_info, sizeof(struct sockaddr_in));
                    instance->type = &sensor_types[k];

                    queue_enqueue(q, instance);
                }

                if(do_register)
                    register_user(id, inet_ntoa(job->client_info.sin_addr));

                k = -1;
                tok = strtok(0, "\n");
            }
            subscribed_types[strlen(subscribed_types) - 1] = '\0';
            sprintf(subscribe_json, "{\"type\":\"subscribe\", \"id\":%u, \"sensors\":[%s]}", old_id, subscribed_types);

            insert_subscribe(id, subscribe_json);

            if(subscribeAnomaly)
            {
                anomaly_buff = 0;

                sprintf(description, "User trying to subscribe with bad id <%u>.", old_id);
                anomaly_buff = get_last_reading(id, &sense_id);
                if(anomaly_buff)
                {
                    sprintf(broadcast_buff, "{\"description\":\"%s\",\"lastReading\":%s}", description, anomaly_buff);
                    insert_anomaly(sense_id, description);
                    free(anomaly_buff);
                }

                sendto(anomaly_sock, broadcast_buff, strlen(broadcast_buff) + 1, 0, (struct sockaddr*)&anomaly_broadcast, anomaly_sin_size);
            }

            sprintf(send_buff, "%u", id);
            //printf("send--->%s\n", send_buff);
            sendto(sock, send_buff, strlen(send_buff) + 1, 0, (struct sockaddr*)&job->client_info, sizeof(struct sockaddr_in));
        }
        else
        {
            strcpy(local_buff, job->actual_job);
            root = cJSON_Parse(job->actual_job);

            request_type = cJSON_GetObjectItem(root, "type")->valuestring;

            if(!strcasecmp(request_type, "upload"))
            {
                type = cJSON_GetObjectItem(root, "sensor")->valuestring;
                id = cJSON_GetObjectItem(root, "id")->valueint;
                x = cJSON_GetObjectItem(root, "x")->valuedouble;
                y = cJSON_GetObjectItem(root, "y")->valuedouble;
                z = cJSON_GetObjectItem(root, "z")->valuedouble;


                instance = queue_getWithIdType(q, id, type);

                if(instance)
                {
                    pthread_mutex_lock(&instance->mutex);

                    instance->last_updated_ts = getMilisecondsFromTS();

                    instance->pinged = 0;

                    description[0] = '\0';


                    if(instance->type->max_x < x)
                        strcat(description, " x overflow");
                    else if(instance->type->min_x > x)
                        strcat(description, " x underflow");
                    if(instance->type->max_y < y)
                        strcat(description, " y overflow");
                    else if(instance->type->min_y > y)
                        strcat(description, " y underflow");
                    if(instance->type->max_z < z)
                        strcat(description, " z overflow");
                    else if(instance->type->min_z > z)
                        strcat(description, " z underflow");

                    pthread_mutex_unlock(&instance->mutex);

                    unsigned int sense_id = insert_sensor_reading(id, local_buff, type, x, y, z);
                    //Register anomaly (out of bounds)
                    //in database, syslog, broadcast anomaly
                    if(strlen(description) > 1)
                    {
                        sprintf(anomaly_buffer, "{\"description\":\"%s\",\"lastReading\":%s}", description,
                            local_buff);
                        sendto(anomaly_sock, anomaly_buffer, strlen(anomaly_buffer) + 1, 0, (struct sockaddr*)&anomaly_broadcast, anomaly_sin_size);

                        if(sense_id != -1)
                        {
                            insert_anomaly(sense_id, description);
                        }

                        printf("send--->%s\n", anomaly_buffer);
                    }
                }
                else
                {
                    //Register anomaly (unregistered)
                    //in database, syslog, broadcast anomaly
                    sprintf(anomaly_buffer, "{\"description\":\"Unregistered user is trying to upload sensor reading! Dropping upload data!\",\"lastReading\":%s}",
                            local_buff);
                    sendto(anomaly_sock, anomaly_buffer, strlen(anomaly_buffer) + 1, 0, (struct sockaddr*)&anomaly_broadcast, anomaly_sin_size);
                    printf("send--->%s\n", anomaly_buffer);
                    syslog(LOG_WARNING, anomaly_buffer);
                    insert_anomaly(0, anomaly_buffer);
                }
            }
            else if(!strcasecmp(request_type, "download"))
            {
                page_offset = cJSON_GetObjectItem(root, "offset")->valueint;
                page_size = cJSON_GetObjectItem(root, "pageSize")->valueint;

                types_array = cJSON_GetObjectItem(root, "sensorTypes");

                if(cJSON_GetArraySize(types_array) <= NUMBER_OF_SENSOR_TYPES)
                    for (i = 0 ; i < cJSON_GetArraySize(types_array); i++)
                    {
                        cJSON * subitem = cJSON_GetArrayItem(types_array, i);
                        requested_types[i] = subitem->valuestring;
                    }

                output_buffer = get_sensor_readings(page_offset, page_size, requested_types);

                sendto(sock, output_buffer, strlen(output_buffer) + 1, 0, (struct sockaddr*)&job->client_info, sizeof(struct sockaddr_in));
                printf("send--->%s\n", output_buffer);
                if(output_buffer)
                    free(output_buffer);
            } else if(!strcasecmp(request_type, "download_anomalies"))
            {
                page_offset = cJSON_GetObjectItem(root, "offset")->valueint;
                page_size = cJSON_GetObjectItem(root, "pageSize")->valueint;

                //printf("%s\n", job->actual_job);


                output_buffer = get_anomalies(page_offset, page_size);
                if(output_buffer)
                {
                    printf("%s\n", output_buffer);
                    sendto(sock, output_buffer, strlen(output_buffer) + 1, 0, (struct sockaddr*)&job->client_info, sizeof(struct sockaddr_in));
                    printf("send--->%s\n", output_buffer);

                    free(output_buffer);
                }
            }

            cJSON_Delete(root);
        }

        my_jobs->next_out = (my_jobs->next_out + 1) % JOB_BUFFER_SIZE;

        sem_post(&my_jobs->access);
        sem_post(&my_jobs->free);
    }
}

//ako je prvi non-white character # onda se preskace linija - komentar
void load_service_conf()
{
    char buff[CONF_LINE_LENGTH] = {0}, key[CONF_LINE_LENGTH/2] = {0}, value[CONF_LINE_LENGTH/2] = {0};
    FILE *conf = fopen(SERVICE_CONF_FNAME, "r");
    int i;

    if(!conf)
    {
        syslog(LOG_ERR, "Service Configuration file can't be opened... exiting");
        closelog();
        exit(1);
    }

    while(!feof(conf))
    {
        fgets(buff, CONF_LINE_LENGTH, conf);
        sscanf(buff, "%s %s", key, value);

        for(i = 0; (buff[i] == ' ' || buff[i] == '\t') && i < CONF_LINE_LENGTH/2; ++i)
            ;

        if(i == CONF_LINE_LENGTH/2 || buff[i] == 0 || buff[i] == '\n' || buff[i] == '#')
            continue;

        if(!strcasecmp("UDP_PORT", key))
            sscanf(value, "%hu", &my_udp_port);
        else if(!strcasecmp("ANOMALY_PORT", key))
            sscanf(value, "%hu", &my_anomaly_port);
        else if(!strcasecmp("GC_LIMIT", key))
            sscanf(value, "%hu", &gc_limit);
        else if(!strcasecmp("DB_PORT", key))
            sscanf(value, "%hu", &db_port);
        else if(!strcasecmp("WORKER_THREADS_NUM", key))
            sscanf(value, "%hu", &worker_threads_num);
        else if(!strcasecmp("TIMEOUT_FACTOR", key))
            sscanf(value, "%hu", &timeout_factor);
        else if(!strcasecmp("DB_HOST", key))
        {
            //db_host = (char*) malloc(strlen(value) + 1);
            //if(db_host != 0)
                strcpy(db_host, value);
        }else if(!strcasecmp("DB_NAME", key))
        {
            //db_name = (char*) malloc(strlen(value) + 1);
            //if(db_name != 0)
                strcpy(db_name, value);
        }else if(!strcasecmp("DB_USER", key))
        {
            //db_user = (char*) malloc(strlen(value) + 1);
            //if(db_user != 0)
                strcpy(db_user, value);
        }else if(!strcasecmp("DB_PASS", key))
        {
            //db_pass = (char*) malloc(strlen(value) + 1);
            //if(db_pass != 0)
                strcpy(db_pass, value);
        }
    }

    fclose(conf);

    if(!db_host[0] || !db_name[0] || !db_user[0] || !db_pass[0])
    {
        syslog(LOG_ERR, "Service Configuration is not loaded properly, execution can't continue... exiting");
        closelog();
        exit(1);
    }
}

void load_sensors_conf()
{
    char buff[CONF_LINE_LENGTH] = {0};
    FILE *conf = fopen(SENSORS_CONF_FNAME, "r");
    int i, k = 0;
    cJSON *root;

    if(!conf)
    {
        syslog(LOG_ERR, "Sensors Configuration file can't be opened... exiting");
        closelog();
        exit(1);
    }

    for(i = 0; i < NUMBER_OF_SENSOR_TYPES; ++i)
        sensor_types[i].name[0] = '\0';

    while(!feof(conf))
    {
        fgets(buff, CONF_LINE_LENGTH, conf);

        for(i = 0; (buff[i] == ' ' || buff[i] == '\t') && i < CONF_LINE_LENGTH; ++i)
            ;

        if(i == CONF_LINE_LENGTH || buff[i] == 0 || buff[i] == '\n' || buff[i] == '#')
            continue;

        root = cJSON_Parse(buff);

        char *name = cJSON_GetObjectItem(root, "name")->valuestring;

        if(!strcasecmp("accelerometer", name))
            k = accelerometer;
        else if(!strcasecmp("gyroscope", name))
            k = gyroscope;
        else if(!strcasecmp("magnetometer", name))
            k = magnetometer;
        else if(!strcasecmp("gps", name))
            k = gps;
        else
            continue;

        strcpy(sensor_types[k].name, name);
        sensor_types[k].keep_alive = cJSON_GetObjectItem(root, "keep_alive")->valueint;
        sensor_types[k].min_x = cJSON_GetObjectItem(root, "min_x")->valuedouble;
        sensor_types[k].min_y = cJSON_GetObjectItem(root, "min_y")->valuedouble;
        sensor_types[k].min_z = cJSON_GetObjectItem(root, "min_z")->valuedouble;
        sensor_types[k].max_x = cJSON_GetObjectItem(root, "max_x")->valuedouble;
        sensor_types[k].max_y = cJSON_GetObjectItem(root, "max_y")->valuedouble;
        sensor_types[k].max_z = cJSON_GetObjectItem(root, "max_z")->valuedouble;

        cJSON_Delete(root);
    }

    fclose(conf);

    pthread_mutex_init(&db_insert_mutex, 0);
    create_tables();
}

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

    ret = sem_destroy(&buff->access);
    ret = sem_destroy(&buff->free);
    ret = sem_destroy(&buff->occupied);
    //todo: check ret value

}

unsigned long int getMilisecondsFromTS()
{
    struct timeval val;
    gettimeofday(&val, 0);

    return val.tv_sec * 1000 + val.tv_usec/1000;
}

void checkingForKeepAliveTimeInterval()
{
    long int timeStamp, sleepTime, sleepTimeFromConfig = LONG_MAX;
    char ping[PING_BUFF_LEN];
    char *anomaly_buff;
    char broadcast_buff[COMMUNICATION_BUFFER_SIZE * 2];
    int i;
    unsigned int sense_id;
    sensor_instance *si = 0, *temp = 0;
    if(!q)
    {
        syslog(LOG_ERR, "Queue is not initialized");
        exit(1);
    }
    for(i = 0; i < NUMBER_OF_SENSOR_TYPES; i++)
    {
        if(sleepTimeFromConfig > sensor_types[i].keep_alive * 1000)
            sleepTimeFromConfig = sensor_types[i].keep_alive * 1000;
    }
    timeStamp = getMilisecondsFromTS();
    while(1)
    {
        si = queue_getWithPosition(q, si);
        if (!si)
        {
            sleepTime = queue_calculateSleepTime(q, sleepTimeFromConfig, timeStamp);
            if(sleepTime <= 0)
                continue;
            else if(sleepTime < 1000)
                sleepTime = 1000;

            sleep(sleepTime / 1000);
            timeStamp = getMilisecondsFromTS();
        } else
        {
            pthread_mutex_lock(&si->mutex);
            if(timeStamp - si->last_updated_ts > (si->type->keep_alive * 1000 * timeout_factor))
            {
                temp = si->next; //????
                queue_removeWithId(q, si);

                printf("remove %u \n", si->id);
                //TODO: Register anomaly (not responding)
                //in database, syslog, broadcast anomaly
                anomaly_buff = get_last_reading_for_sensor_name(si->id, &sense_id, si->type->name);
                if(!strlen(anomaly_buff))
                {
                    free(anomaly_buff);
                    anomaly_buff = get_last_reading(si->id, &sense_id);
                    sprintf(broadcast_buff, "{\"description\":\"Sensor not responding.\",\"lastReading\":%s}", anomaly_buff);
                } else
                {
                    sprintf(broadcast_buff, "{\"description\":\"Sensor not responding.\",\"lastReading\":%s}", anomaly_buff);
                }

                insert_anomaly(sense_id, "Sensor not responding.");

                pthread_mutex_unlock(&si->mutex);
               // sprintf(broadcast_buff, "{\"description\":\"Sensor not responding.\",\"lastReading\":%s}", anomaly_buff);

                printf("%s\n", anomaly_buff);
                printf("%s\n", broadcast_buff);

                free(anomaly_buff);
                sendto(anomaly_sock, broadcast_buff, strlen(broadcast_buff) + 1, 0, (struct sockaddr*)&anomaly_broadcast, anomaly_sin_size);

                sensor_instance_destroy(si);
                si = temp;
            } else if ((!si->pinged) && (timeStamp - si->last_updated_ts > (si->type->keep_alive * 1000)))
            {
                sprintf(ping,"ping\n%s", si->type->name);
                sendto(sock, ping, strlen(ping) + 1, 0, (struct sockaddr*) si->client_info, sizeof(struct sockaddr_in));
                printf("ping %u \n", si->id);
                si->pinged = 1;
                pthread_mutex_unlock(&si->mutex);
            } else
                pthread_mutex_unlock(&si->mutex);

        }
    }
}


