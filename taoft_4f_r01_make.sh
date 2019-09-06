#!/bin/bash

echo -e "\nCompiling TAOFT-4F Rev.01 data acquisition code for the LabJack T7 . . . \c"
gcc taoft_4f_r01_daq.c -g -Wall -lLabJackM -lm -o t4f1_daq
echo -e "done!\n"

rm -f *~ > /dev/null
