

test -d bin/ || mkdir -p bin/
test -d obj/ || mkdir -p obj/
test -d obj/libs/cJSON || mkdir -p obj/libs/cJSON
cp sensors.conf bin/
cp service.conf bin/

gcc -Wall -O2 -I/usr/include/mysql/ -c data_layer.c -o obj/data_layer.o
gcc -Wall -O2 -I/usr/include/mysql/ -c libs/cJSON/cJSON.c -o obj/libs/cJSON/cJSON.o
gcc -Wall -O2 -I/usr/include/mysql/ -c main.c -o obj/main.o
g++  -o bin/SenseService obj/data_layer.o obj/libs/cJSON/cJSON.o obj/main.o  -lpthread -lssl -lcrypto -ldl -lz /usr/lib/libmysqlclient.so

rm -rf obj
