#!/bin/bash

echo -e "\nCompiling 4.5in Closed TBECS TAPPT Rev.01 data acquisition code for the LabJack T7 . . . \c"
gcc closed_tbecs_tappt_r01_daq.c -g -Wall -lLabJackM -lm -o ctt1_daq
echo -e "done!\n"

echo -e "Compiling 4.5in Closed TBECS TAPPT Rev.01 tiltmeter levelling code for the LabJack T7 . . . \c"
gcc closed_tbecs_tappt_r01_lev.c -g -Wall -lLabJackM -lm -o ctt1_lev
echo -e "done!\n"

echo -e "Compiling 4.5in Closed TBECS TAPPT Rev.01 strainmeter and tiltmeter orienting code for the LabJack T7 . . . \c"
gcc closed_tbecs_tappt_r01_ori.c -g -Wall -lLabJackM -lm -o ctt1_ori
echo -e "done!\n"

rm -f *~ > /dev/null
