#ifndef DATA_LAYER_H_INCLUDED
#define DATA_LAYER_H_INCLUDED

#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include "my_global.h"
#include "mysql.h"
#include "sensors.h"

unsigned int insert_sensor_reading(unsigned int user_id, char *json, char *sensor_type, double x, double y, double z);

void create_tables();

void register_user(unsigned int id, char* client_address);
int check_user_exists(unsigned int id, char* client_address);

char* get_sensor_readings(int page_offset, int page_size, char **requested_types);
void insert_anomaly(unsigned int sense_id, char* description);
void insert_subscribe(unsigned int sense_id, char* json);
char* get_last_reading(unsigned int user_id, unsigned int *sense_id);
char* get_last_reading_for_sensor_name(unsigned int user_id, unsigned int *sense_id, char* sensor_name);
char* get_anomalies(int page_offset, int page_size);


#endif // DATA_LAYER_H_INCLUDED
