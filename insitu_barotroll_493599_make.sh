#!/bin/bash

echo -e "\nCompiling In-Situ BaroTROLL SN: 493599 data acquisition code using RS485 . . . \c"
#gcc insitu_barotroll_493599_daq.c -g -Wall -lm -o ww29_daq
gcc  insitu_barotroll_493599_daq.c -g -Wall -lm -o w29_daq  `pkg-config --cflags --libs libmodbus`
echo -e "done!\n"

rm -f *~ > /dev/null
