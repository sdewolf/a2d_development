#!/bin/bash

echo -e "\nCompiling Vaisala WXT520 SN: M2310477 data acquisition code using RS232 . . . \c"
gcc vaisala_wxt520_m2310477_daq.c -g -Wall -lm -o met1_daq
echo -e "done!\n"

rm -f *~ > /dev/null
