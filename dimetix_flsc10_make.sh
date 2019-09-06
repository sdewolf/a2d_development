#!/bin/bash

echo -e "\nCompiling Dimetix FLS-C10 data acquisition code on /dev/ttyUSB0 . . . \c"
gcc dimetix_flsc10_daq.c -g -Wall -lm -DDISPLAY_STRING -o dfls_daq
echo -e "done!\n"

rm -f *~ > /dev/null
