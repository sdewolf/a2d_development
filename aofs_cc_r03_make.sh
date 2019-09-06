#!/bin/bash

echo -e "\nCompiling AOFS-CC Rev.03 data acquisition code for the LabJack T7 . . . \c"
gcc aofs_cc_r03_daq.c -g -Wall -lLabJackM -lm -o acc3_daq
echo -e "done!\n"

rm -f *~ > /dev/null
