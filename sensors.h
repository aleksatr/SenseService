#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

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
};

enum sensor_enum
{
    accelerometer,
    gyroscope,
    magnetometer,
    gps,
    NUMBER_OF_SENSOR_TYPES
};

#endif // SENSORS_H_INCLUDED
