#include "data_layer.h"

extern char db_host[CONF_LINE_LENGTH/2];
extern char db_name[CONF_LINE_LENGTH/2];
extern char db_user[CONF_LINE_LENGTH/2];
extern char db_pass[CONF_LINE_LENGTH/2];
extern unsigned short int db_port;

void insert_sensor_reading(long int id, char *client_address, char *sensor_type, double x, double y, double z)
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

    sprintf(query_string, "INSERT INTO MESTO(client_id, client_address, sensor_type, x_value, y_value, z_value, ts) VALUES (%ld, '%s', '%s', %f, %f, %f, NULL)",
                id, client_address, sensor_type, x, y, z);

    if (mysql_query(con, query_string))
    {
        syslog(LOG_ERR, "Query execution failed: %s", mysql_error(con));
        syslog(LOG_ERR, "Query string: %s", query_string);
        mysql_close(con);
        return;
    }

    mysql_close(con);
}
