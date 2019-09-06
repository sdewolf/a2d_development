#!/bin/bash

echo -e "\nCompiling 4.5in Closed TBECS TAPPT Rev.02 data acquisition code for the LabJack T7 . . . \c"
gcc closed_tbecs_tappt_r02_daq.c -g -Wall -lLabJackM -lm -o ctt2_daq
echo -e "done!\n"

echo -e "Compiling 4.5in Closed TBECS TAPPT Rev.02 tiltmeter levelling code for the LabJack T7 . . . \c"
gcc closed_tbecs_tappt_r02_lev.c -g -Wall -lLabJackM -lm -o ctt2_lev
echo -e "done!\n"

echo -e "Compiling 4.5in Closed TBECS TAPPT Rev.02 strainmeter and tiltmeter orienting code for the LabJack T7 . . . \c"
gcc closed_tbecs_tappt_r02_ori.c -g -Wall -lLabJackM -lm -o ctt2_ori
echo -e "done!\n"

rm -f *~ > /dev/null
