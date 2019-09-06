#!/bin/bash

echo -e "\nCompiling Applied Geomechanics LILY 8008 data acquisition code using RS422 . . . \c"
gcc lily_8008_daq.c -g -Wall -lm -o lil1_daq
echo -e "done!\n"

echo -e "Compiling Applied Geomechanics LILY 8008 orienting code using RS422 . . . \c"
gcc lily_8008_ori.c -g -Wall -lm -o lil1_ori
echo -e "done!\n"

rm -f *~ > /dev/null
