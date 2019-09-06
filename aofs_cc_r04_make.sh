#!/bin/bash

echo -e "\nCompiling AOFS-CC Rev.04 data acquisition code for the LabJack T7 . . . \c"
gcc aofs_cc_r04_daq.c -g -Wall -lLabJackM -lm -o acc4_daq
echo -e "done!\n"

rm -f *~ > /dev/null
