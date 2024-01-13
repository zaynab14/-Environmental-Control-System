#!/bin/bash

gcc -o weather weather_program.c -lcurl -ljson-c

#Set the path to your C executable
AUTO=./weather

#Run the C code to fetch weather data and parse it
$AUTO

#sleep for an hour(3600 seconds)
sleep 3600

#Running the script again after 1 hour
exec "$0"
