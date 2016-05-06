#ifndef DATA_LAYER_H_INCLUDED
#define DATA_LAYER_H_INCLUDED

#include <syslog.h>
#include "mariadb/my_global.h"
#include "mariadb/mysql.h"
#include "sensors.h"

void insert_sensor_reading(long int id, char *client_address, char *sensor_type, double x, double y, double z);

//to do: read_from_db

#endif // DATA_LAYER_H_INCLUDED
