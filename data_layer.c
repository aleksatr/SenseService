#include "data_layer.h"

extern char db_host[CONF_LINE_LENGTH/2];
extern char db_name[CONF_LINE_LENGTH/2];
extern char db_user[CONF_LINE_LENGTH/2];
extern char db_pass[CONF_LINE_LENGTH/2];
//table ids
extern pthread_mutex_t db_insert_mutex;
//extern my_ulonglong next_users_id = 1;
//extern my_ulonglong next_senses_id = 1;
//
sensor_type sensor_types[NUMBER_OF_SENSOR_TYPES];
extern unsigned short int db_port;


void create_tables()
{
    int i;
    char query_string[COMMUNICATION_BUFFER_SIZE] = {0};
    MYSQL *con = mysql_init(0);

    if (con == 0)
    {
        syslog(LOG_ERR, "mysql_init(0) failed: %s", mysql_error(con));
        return;
    }

    if (mysql_real_connect(con, db_host, db_user, db_pass, db_name, db_port, 0, 0) == 0)
    {
        syslog(LOG_ERR, "Connection to MariaDB server failed: %s", mysql_error(con));
        syslog(LOG_ERR, "db_host: \"%s\", db_name: \"%s\", db_user: \"%s\", db_pass: \"%s\", db_port: %d",
                db_host, db_name, db_user, db_pass, db_port);

        mysql_close(con);
        return;
    }

    sprintf(query_string,  "CREATE TABLE IF NOT EXISTS users \
                            ( \
                                id INTEGER AUTO_INCREMENT, \
                                client_id INTEGER UNIQUE, \
                                client_address VARCHAR(100), \
                                created_ts TIMESTAMP, \
                                PRIMARY KEY (id) \
                            )");

    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
        syslog(LOG_ERR, "Query string: %s", query_string);
        mysql_close(con);
        //return;
    }

    sprintf(query_string,  "CREATE TABLE IF NOT EXISTS senses \
                            ( \
                                id INTEGER AUTO_INCREMENT, \
                                user_id INTEGER, \
                                json VARCHAR(1024), \
                                ts TIMESTAMP, \
                                PRIMARY KEY (id), \
                                FOREIGN KEY (user_id) REFERENCES users(client_id) ON DELETE SET NULL \
                            )");

    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
        syslog(LOG_ERR, "Query string: %s", query_string);
        mysql_close(con);
        //return;
    }

    sprintf(query_string,  "CREATE TABLE IF NOT EXISTS anomaly \
                            ( \
                                id INTEGER AUTO_INCREMENT, \
                                senses_id INTEGER NOT NULL, \
                                description VARCHAR(100), \
                                PRIMARY KEY (id), \
                                FOREIGN KEY (senses_id) REFERENCES senses(id) ON DELETE CASCADE \
                            )");

    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
        syslog(LOG_ERR, "Query string: %s", query_string);
        mysql_close(con);
        //return;
    }

    for(i = 0; i < NUMBER_OF_SENSOR_TYPES; ++i)
        if(sensor_types[i].name[0] != '\0')
        {
            sprintf(query_string,  "CREATE TABLE IF NOT EXISTS %s \
                                    ( \
                                        id INTEGER AUTO_INCREMENT, \
                                        senses_id INTEGER NOT NULL, \
                                        x_value DOUBLE NOT NULL, \
                                        y_value DOUBLE, \
                                        z_value DOUBLE, \
                                        PRIMARY KEY (id), \
                                        FOREIGN KEY (senses_id) REFERENCES senses(id) ON DELETE CASCADE \
                                    )", sensor_types[i].name);

            if (mysql_query(con, query_string))
            {
                syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
                syslog(LOG_ERR, "Query string: %s", query_string);
                mysql_close(con);
                continue;
            }
        }

    mysql_close(con);
}

void insert_sensor_reading(int user_id, char *json, char *sensor_type, double x, double y, double z)
{
    int i;
    my_ulonglong curr_senses_id = 1;
    char query_string[COMMUNICATION_BUFFER_SIZE] = {0};
    MYSQL *con = mysql_init(0);

    if (con == 0)
    {
        syslog(LOG_ERR, "mysql_init(0) failed: %s", mysql_error(con));
        return;
    }

    if (mysql_real_connect(con, db_host, db_user, db_pass, db_name, db_port, 0, 0) == 0)
    {
        syslog(LOG_ERR, "Connection to MariaDB server failed: %s", mysql_error(con));
        syslog(LOG_ERR, "db_host: \"%s\", db_name: \"%s\", db_user: \"%s\", db_pass: \"%s\", db_port: %d",
                db_host, db_name, db_user, db_pass, db_port);

        mysql_close(con);
        return;
    }

    sprintf(query_string, "INSERT INTO senses(user_id, json, ts) VALUES (%d, '%s', NULL)",
                user_id, json);

    pthread_mutex_lock(&db_insert_mutex);
    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
        syslog(LOG_ERR, "Query string: %s", query_string);
        mysql_close(con);
        pthread_mutex_unlock(&db_insert_mutex);
        return;
    }

    curr_senses_id = mysql_insert_id(con);
    pthread_mutex_unlock(&db_insert_mutex);

    sprintf(query_string, "INSERT INTO %s(senses_id, x_value, y_value, z_value) VALUES (%ld, %f, %f, %f)",
                sensor_type, curr_senses_id, x, y, z);

    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
        syslog(LOG_ERR, "Query string: %s", query_string);
        mysql_close(con);
        return;
    }

    mysql_close(con);
}

char* get_sensor_readings(int page_offset, int page_size, char **requested_types)
{
    int num_fields, i, j = 0;
    char temp_buff[COMMUNICATION_BUFFER_SIZE] = {0};
    MYSQL_RES *result;
    MYSQL_ROW row;
    MYSQL *con = mysql_init(0);
    char query_string[COMMUNICATION_BUFFER_SIZE] = "SELECT client_id, sensor_type, x_value, y_value, z_value, ts FROM Senses WHERE sensor_type in (";
    char *output_buff = 0;

    if (con == 0)
    {
        syslog(LOG_ERR, "mysql_init(0) failed: %s", mysql_error(con));
        return 0;
    }

    if (mysql_real_connect(con, db_host, db_user, db_pass, db_name, db_port, 0, 0) == 0)
    {
        syslog(LOG_ERR, "Connection to MariaDB server failed: %s", mysql_error(con));
        syslog(LOG_ERR, "db_host: \"%s\", db_name: \"%s\", db_user: \"%s\", db_pass: \"%s\", db_port: %d",
                db_host, db_name, db_user, db_pass, db_port);

        mysql_close(con);
        return 0;
    }

    //build query string
    for(i = 0; i < NUMBER_OF_SENSOR_TYPES && requested_types[i] != 0; ++i)
    {
        if(!i)
            sprintf(temp_buff, "\"%s\"", requested_types[i]);
        else
        {
            strcat(temp_buff, ", \"");
            strcat(temp_buff, requested_types[i]);
            strcat(temp_buff, "\"");
        }
    }

    strcat(query_string, temp_buff);
    sprintf(temp_buff, ") LIMIT %d OFFSET %d", page_size, page_offset);
    strcat(query_string, temp_buff);


    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "mysql_query() failed: %s", mysql_error(con));
        mysql_close(con);
        return 0;
    }

    result = mysql_store_result(con);

    if (result == 0)
    {
        syslog(LOG_ERR, "mysql_store_result() failed: %s", mysql_error(con));
        mysql_close(con);
        return 0;
    }

    num_fields = mysql_num_fields(result);
    output_buff = (char*) malloc(COMMUNICATION_BUFFER_SIZE * page_size);

    output_buff[0] = '[';
    output_buff[1] = '\0';

    j = 0;
    while ((row = mysql_fetch_row(result)))
    {
        sprintf(temp_buff, "%s{\"id\":%s, \"sensor\":\"%s\", \"x\":%s, \"y\":%s, \"z\":%s, \"timestamp\":\"%s\"}",
                (j++ ? ", " : ""), row[0], row[1], row[2], row[3], row[4], row[5]);

        strcat(output_buff, temp_buff);
    }

    strcat(output_buff, "]");

    mysql_free_result(result);
    mysql_close(con);

    return output_buff;
}

int check_user_exists(int id, char* client_address)
{
    int num_fields, idFromDb = -2;
    MYSQL_RES *result;
    MYSQL_ROW row;
    MYSQL *con = mysql_init(0);
    char query_string[COMMUNICATION_BUFFER_SIZE] = {0};
    char temp_buff[COMMUNICATION_BUFFER_SIZE] = {0};

    if (con == 0)
    {
        syslog(LOG_ERR, "mysql_init(0) failed: %s", mysql_error(con));
        return 0;
    }

    if (mysql_real_connect(con, db_host, db_user, db_pass, db_name, db_port, 0, 0) == 0)
    {
        syslog(LOG_ERR, "Connection to MariaDB server failed: %s", mysql_error(con));
        syslog(LOG_ERR, "db_host: \"%s\", db_name: \"%s\", db_user: \"%s\", db_pass: \"%s\", db_port: %d",
                db_host, db_name, db_user, db_pass, db_port);

        mysql_close(con);
        return 0;
    }

    sprintf(query_string, "%s %d", "SELECT id, client_address FROM users where client_id = ", id);
    //build query string

    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "mysql_query() failed: %s", mysql_error(con));
        mysql_close(con);
        return 0;
    }

    result = mysql_store_result(con);

    if (result == 0)
    {
        syslog(LOG_ERR, "mysql_store_result() failed: %s", mysql_error(con));
        mysql_close(con);
        return 0;
    }

    num_fields = mysql_num_fields(result);

    while ((row = mysql_fetch_row(result)))
    {
       // sprintf(temp_buff, "%s{\"id\":%s, \"sensor\":\"%s\", \"x\":%s, \"y\":%s, \"z\":%s, \"timestamp\":\"%s\"}",
        //        (j++ ? ", " : ""), row[0], row[1], row[2], row[3], row[4], row[5]);
        sscanf(row[0], "%d", &idFromDb);
        sscanf(row[1], "%s", temp_buff);

        if (strcmp(client_address, temp_buff))
        {
            sprintf(query_string, "UPDATE users set client_address = \'%s\' WHERE id = %d", client_address, idFromDb);

            if (mysql_query(con, query_string))
            {
                printf("Query update failed: %s \n", mysql_error(con));
                printf("Query string: %s \n", query_string);
                mysql_close(con);
                return -1;
            }
        }
    }

    mysql_free_result(result);
    mysql_close(con);

    return idFromDb != -2;
}

void register_user(int id, char* client_address)
{
    int i;
    char query_string[COMMUNICATION_BUFFER_SIZE] = {0};
    MYSQL *con = mysql_init(0);

    if (con == 0)
    {
        syslog(LOG_ERR, "mysql_init(0) failed: %s", mysql_error(con));
        return;
    }

    if (mysql_real_connect(con, db_host, db_user, db_pass, db_name, db_port, 0, 0) == 0)
    {
        syslog(LOG_ERR, "Connection to MariaDB server failed: %s", mysql_error(con));
        syslog(LOG_ERR, "db_host: \"%s\", db_name: \"%s\", db_user: \"%s\", db_pass: \"%s\", db_port: %d",
                db_host, db_name, db_user, db_pass, db_port);

        mysql_close(con);
        return;
    }

    sprintf(query_string, "INSERT INTO users(client_id, client_address, created_ts) VALUES (%ld, '%s', NULL)",
                id, client_address);

    if (mysql_query(con, query_string))
    {
        printf("Query execution failed: %s \n", mysql_error(con));
        printf("Query string: %s \n", query_string);
        mysql_close(con);
        return;
    }

    mysql_close(con);
}
