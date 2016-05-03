#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> //sadrzi definiciju struct sockaddr_in
#include <arpa/inet.h>  //sadrzi inet_ntoa()
#include <sys/stat.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <pthread.h>
#include "./libs/cJSON/cJSON.h"
#include "sensors.h"

//#define MY_PORT 8001
#define CONF_LINE_LENGTH 512
//#define INPUT_BUF_SIZE 10000
//#define GC_LIMIT 20                     //maksimalan broj child processa koji nisu pocisceni
//#define UNRES_CONN_QUEUE_LEN 5000       //maksimalna duzina reda neresenih konekcija. argument za sistemski poziv listen()
//#define LOG_NAME "service.log"
#define SERVICE_CONF_FNAME "service.conf"
#define SENSORS_CONF_FNAME "sensors.conf"

void load_service_conf();
void load_sensors_conf();
void exit_cleanup();
void sig_int_handler();
void create_worker_threads();
void create_job_buffers();
void do_work(void *job_buffer_ptr);

unsigned short int my_udp_port = 3333;
unsigned short int gc_limit = 20;
unsigned short int db_port = 3306;
unsigned short int worker_threads_num = 10;
char db_host[CONF_LINE_LENGTH/2] = {0};
char db_name[CONF_LINE_LENGTH/2] = {0};
char db_user[CONF_LINE_LENGTH/2] = {0};
char db_pass[CONF_LINE_LENGTH/2] = {0};
struct sensor_type sensor_types[NUMBER_OF_SENSOR_TYPES];
pthread_t *worker_threads = 0;
sensor_job_buffer *job_buffers = 0;
int curr_thread = 0;
int sock = -1;

int main(int argc, char **argv)
{
    int i, num_of_cp_waiting = 0; //broj child processa ciji status nije prikupio proces roditelj
    pid_t childPid;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in serverAddress, clientAddress;

    //open log
    openlog(NULL, LOG_PID|LOG_CONS, LOG_USER);

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

    scanf("%d", &i);

    bzero((char*) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(my_udp_port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(serverAddress.sin_zero), '\0', 8);

    //kreiranje socketa
    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom kreiranja socketa...");
        //closelog();
        exit(1);
    }

    if(bind(sock, (struct sockaddr*) &serverAddress, sizeof(struct sockaddr)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom povezivanja socketa...");
        //closelog();
        exit(1);
    }

    /*while(1)
    {


        if((childPid = fork()) < 0)
        {
            syslog(LOG_INFO, "Greska pri kreiranju procesa za komunikaciju sa klijentom");
        }
        else
        {
            ++num_of_cp_waiting;
            if(!childPid)
            {
                //proces dete
                int brojPoslatihBajtova, brojPrimljenihBajtova, statusNo, outputLen;
                char inputBuf[INPUT_BUF_SIZE] = {0}, *outputBuf = 0;
                unsigned char *picBuff;
                int check = -1;

                close(sock); //nije nam vise potreban u ovom procesu
                //komunikacija sa klijentom....slanje i primanje poruka

                brojPrimljenihBajtova = recv(newSock, (void*) inputBuf, INPUT_BUF_SIZE, 0);
                printf("Server je primio zahtev duzine %d B sa adrese [%s] \n", brojPrimljenihBajtova, inet_ntoa(clientAddress.sin_addr));
                statusNo = obradiZahtev(inputBuf, &outputBuf, &outputLen);
                if(statusNo > 0)
                {
                    statusNo = obradiGresku(statusNo, &outputBuf, &outputLen);
                }
                else if(statusNo < 0)
                {
                    brojPoslatihBajtova = send(newSock, outputBuf, outputLen, 0);
                    if(brojPoslatihBajtova < 0)
                        printf("Doslo je do greske prilikom slanja podataka :S\n");


                    picBuff = malloc(PIC_BUFF_SIZE * sizeof(char));
                    if(!picBuff)
                    {
                        printf("malloc() failed! picBuff\n");
                    }
                    else
                    {
                        //primi poruku i dekodiraj je
                        recv(newSock, (void*) picBuff, (size_t)PIC_BUFF_SIZE, 0);
                        //printf("chech = %d\n", check);
                        while((check = obradiWebSockFrame(picBuff)) > 0)
                        {
                            if(check > 1)
                                send(newSock, picBuff, check, 0);
                            //printf("chech = %d\n", check);
                            recv(newSock, (void*) picBuff, (size_t)PIC_BUFF_SIZE, 0);
                            //printf("primio\n");
                        }
                        //printf("chech = %d\n", check);
                        //recv(newSock, (void*) picBuff, (size_t)PIC_BUFF_SIZE, 0);
                        //obradiWebSockFrame(picBuff);
                        //printf("\nPrimio sam: %s\n", picBuff);

                        free(picBuff);
                    }

                }

                if(!statusNo)
                {
                    brojPoslatihBajtova = send(newSock, outputBuf, outputLen, 0);
                    if(brojPoslatihBajtova < 0)
                        printf("Doslo je do greske prilikom slanja podataka :S\n");
                }

                if(outputBuf)
                        free(outputBuf);

                exit(0);
            }
        }

        //close(newSock);

        if(num_of_cp_waiting == gc_limit)
        {
            for(i = 0; i < gc_limit; ++i)
                wait(0);

            syslog(LOG_INFO, "Ocistio sam 20 zombija");

            num_of_cp_waiting = 0;
        }
    }*/

    exit(0);
}

void exit_cleanup()
{
    int i = 0, ret;

    for(i = 0; i < worker_threads_num; ++i)
    {
        //to do: check if worker_threads[i] is initialized at all
        ret = pthread_cancel(worker_threads[i]);
        //to do: ok if ret==0, else error
    }

    if(sock >= 0)
        close(sock);

    if(worker_threads != 0)
        free(worker_threads);

    //to do: call void destroy_job_buffer(struct sensor_job_buffer* buff)
    if(job_buffers != 0)
        free(job_buffers);

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
        //to do: ok if ret==0, else error
    }

}

void do_work(void *job_buffer_ptr)
{
    sensor_job_buffer *my_jobs = (sensor_job_buffer *) job_buffer_ptr;

    syslog(LOG_INFO, "worker thread is born!");

    //consumer
    while(1)
    {
        sem_wait(&my_jobs->occupied);
        sem_wait(&my_jobs->access);

        //do your shit
        sensor_job *job = &my_jobs->jobs[my_jobs->next_out];

        //citaj job->actual_job
        //enqueue if subscribe and return id
        //else insert into db

        my_jobs->next_out = (my_jobs->next_out + 1) % JOB_BUFFER_SIZE;
        //

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
        else if(!strcasecmp("GC_LIMIT", key))
            sscanf(value, "%hu", &gc_limit);
        else if(!strcasecmp("DB_PORT", key))
            sscanf(value, "%hu", &db_port);
        else if(!strcasecmp("WORKER_THREADS_NUM", key))
            sscanf(value, "%hu", &worker_threads_num);
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
}

