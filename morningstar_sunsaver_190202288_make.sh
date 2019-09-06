#!/bin/bash

echo -e "\nCompiling Morningstar SunSaver SN: 190202288 data acquisition code using MODBUS . . . \c"
#gcc morningstar_sunsaver_190202288_daq.c -g -Wall -lm -o mss1_daq
gcc  morningstar_sunsaver_190202288_daq.c -g -Wall -lm -o mss1_daq  `pkg-config --cflags --libs libmodbus`
echo -e "done!\n"

rm -f *~ > /dev/null
