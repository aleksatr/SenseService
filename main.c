#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> //sadrzi definiciju struct sockaddr_in
#include <arpa/inet.h>  //sadrzi inet_ntoa()
#include <sys/stat.h>
#include <time.h>
#include <syslog.h>

//#define MY_PORT 8001
#define CONF_LINE_LENGTH 512
#define INPUT_BUF_SIZE 10000
//#define GC_LIMIT 20                     //maksimalan broj child processa koji nisu pocisceni
//#define UNRES_CONN_QUEUE_LEN 5000       //maksimalna duzina reda neresenih konekcija. argument za sistemski poziv listen()
//#define LOG_NAME "service.log"
#define CONF_FNAME "conf.dat"

unsigned short my_udp_port = 3333;
unsigned int gc_limit = 20;
unsigned short db_port = 3306;
char *db_host = 0;
char *db_name = 0;
char *db_user = 0;
char *db_pass = 0;

int main(int argc, char **argv)
{
    int sock, newSock, i, num_of_cp_waiting = 0; //broj child processa ciji status nije prikupio proces roditelj
    pid_t childPid;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in serverAddress, clientAddress;

    //open log
    openlog(NULL, LOG_PID|LOG_CONS, LOG_USER);

    //load config from file CONF_FNAME
    loadConfig();

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(my_udp_port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    //kreiranje socketa
    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom kreiranja socketa... exiting ");
        closelog();
        exit(1);
    }

    if(bind(sock, (struct sockaddr*) &serverAddress, sizeof(struct sockaddr)) < 0)
    {
        syslog(LOG_ERR, "Doslo je do greske prilikom povezivanja socketa... exiting ");
        closelog();
        exit(1);
    }

    while(1)
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
    }

    close(sock);
    closelog();

    return 0;
}

//ako je prvi non-white character # onda se preskace linija - komentar
void loadConfig()
{
    char buff[CONF_LINE_LENGTH] = {0}, key[CONF_LINE_LENGTH/2] = {0}, value[CONF_LINE_LENGTH/2] = {0};
    FILE *conf = fopen(CONF_FNAME, "r");
    int i;

    if(!conf)
    {
        syslog(LOG_ERR, "Configuration file can't be opened... exiting");
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
            sscanf(value, "%u", &my_port);
        else if(!strcasecmp("GC_LIMIT", key))
            sscanf(value, "%u", &gc_limit);
        else if(!strcasecmp("DB_PORT", key))
            sscanf(value, "%u", &db_port);
        else if(!strcasecmp("DB_HOST", key))
        {
            db_host = (char*) malloc(strlen(value) + 1);
            if(db_host != 0)
                strcpy(db_host, value);
        }else if(!strcasecmp("DB_NAME", key))
        {
            db_name = (char*) malloc(strlen(value) + 1);
            if(db_name != 0)
                strcpy(db_name, value);
        }else if(!strcasecmp("DB_USER", key))
        {
            db_user = (char*) malloc(strlen(value) + 1);
            if(db_user != 0)
                strcpy(db_user, value);
        }else if(!strcasecmp("DB_PASS", key))
        {
            db_pass = (char*) malloc(strlen(value) + 1);
            if(db_pass != 0)
                strcpy(db_pass, value);
        }
    }

    fclose(conf);

    if(!db_host || !db_name || !db_user || !db_pass)
    {
        syslog(LOG_ERR, "Configuration is not loaded properly, execution can't continue... exiting");
        closelog();
        exit(1);
    }
}

