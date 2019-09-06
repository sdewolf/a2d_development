#!/bin/bash

echo -e "\nCompiling Applied Geomechanics LILY 8209 data acquisition code using RS422 . . . \c"
gcc lily_8209_daq.c -g -Wall -lm -o lil2_daq
echo -e "done!\n"

echo -e "Compiling Applied Geomechanics LILY 8209 orienting code using RS422 . . . \c"
gcc lily_8209_ori.c -g -Wall -lm -o lil2_ori
echo -e "done!\n"

rm -f *~ > /dev/null
