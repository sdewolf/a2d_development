// 4.5in Closed TBECS TAPPT Rev.01 strainmeter and tiltmeter orienting program
//
// by: Scott DeWolf
//
// modified from https://labjack.com/sites/default/files/software/labjack_ljm_software_2016_05_15_i386.tar_0.gz/labjack_ljm_examples/examples/dio/single_dio_write.c
//
//  created: Wednesday, September 21, 2016 (2016265)
// modified: Tuesday, March 6, 2019 (2019064)
//  history:
//           [2016265] - created document
//           [2018078] - dunno
//           [2019064] - updated with compass calibration info (finally)
//

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "LabJackM.h"
#include "LJM_Utilities.h"

const double   a0 = 0.7136815527321586;
const double  x00 = 2.4178873230388165;
const double   b0 = 0.6351991799973854;
const double  y00 = 2.4546674370174899;
const double  p00 = 0.1625971448130479;
const double p000 = 3.8460341632529942;

int main()
{

  // variables for configuring the magnetic compass AINs
  const char * aNamesConfig[8] = \
    {"AIN12_NEGATIVE_CH", "AIN12_RANGE", "AIN12_RESOLUTION_INDEX", "AIN12_SETTLING_US",
     "AIN13_NEGATIVE_CH", "AIN13_RANGE", "AIN13_RESOLUTION_INDEX", "AIN13_SETTLING_US"};
  const double aValuesConfig[8] = {199, 10, 0, 0,
				   199, 10, 0, 0};
  double aValuesAIN[2] = {0};
  const char * aNamesAIN[2] = {"AIN12", "AIN13"};

  // variables for converting the magnetic compass AINs to azimuth (CW from North)
  int i;
  double sint, cost;
  double pi = acos(-1);
  double angle_sum = 0, angle_sq_sum = 0;
  double angle, angle_mean, angle_var;

  // variables for getting the epoch time with microseconds
  struct timeval tv;
  uint64_t isc; uint32_t usc;
  double t, t_sum = 0;

  // variables for yearday filenames
  time_t rawtime = time(NULL);
  struct tm ft;
  char fn[200];
  FILE *fid;

  // variables for making year/day directories
  struct stat st = {0};
  char path[200];

  // T7 handle and variables for error handling
  int handle;
  int errorAddress = INITIAL_ERR_ADDRESS;

  // power cycle and open the LabJack T7 for the Simpson Bull Farm 4.5in Closed TBECS TAPPT Rev.01
  system("/usr/bin/perl /home/sbf0/LabJack/usbrelay0.pl");
  LJM_Open(LJM_dtT7, LJM_ctETHERNET, "470011723", &handle);

  // setup and call eWriteNames to configure AINs on the LabJack
  LJM_eWriteNames(handle, 8, aNamesConfig, aValuesConfig, &errorAddress);

  printf("\n");
  for (i = 0; i < 1000; i++)
  {
    // get epoch time in microseconds
    gettimeofday(&tv, NULL);
    isc = (uint64_t)(tv.tv_sec);
    usc = (uint32_t)(tv.tv_usec);
    t = (double)isc + (double)usc / 1000000;

    // read AINs from the LabJack
    LJM_eReadNames(handle, 2, aNamesAIN, aValuesAIN, &errorAddress);

    // compute angle less its offset and display output
    sint=(aValuesAIN[0]-x00)/(a0*cos(p00))-(aValuesAIN[1]-y00)*tan(p00)/b0;
    cost=(aValuesAIN[1]-y00)/b0;
    angle = 180 * (atan2(sint, cost)-p000) / pi;
    if (angle < 0)
      angle = angle + 360;
    printf("t = %9.4f \t x = %0.6f \t y = %0.6f \t angle = %0.6f degrees.\n", t, aValuesAIN[0], aValuesAIN[1], angle);

    // compute running totals for mean and standard deviation
    t_sum += t;
    angle_sum += angle;
    angle_sq_sum += angle * angle;
  }

  // compute and display angle mean and variance
  t = t_sum / 1000;
  angle_mean = angle_sum / 1000;
  angle_var = angle_sq_sum / 1000 - angle_mean * angle_mean;
  printf("\nt = %9.6f \t Angle = %0.6f +/- %0.6f degrees.\n", t, angle_mean, sqrt(angle_var));

  // get year and day number
  gmtime_r(&rawtime, &ft);
  rawtime = time(NULL);

  // create /home/station/LabJack/data/yyyy path if it is not present
  sprintf(path, "/home/sbf0/Data/%4i", ft.tm_year+1900);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create /home/station/LabJack/data/yyyy/ddd path if it is not present
  sprintf(path, "/home/sbf0/Data/%4i/%03i", ft.tm_year+1900, ft.tm_yday+1);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // write results to a file
  sprintf(fn, "%s/2J.SBF2.E1.AYO.%4i.%03i.txt", path, ft.tm_year+1900, ft.tm_yday+1);
  fid = fopen(fn, "a");
  fprintf(fid, "t = %9.6f \t Angle = %0.6f +/- %0.6f degrees.\n", t, angle_mean, sqrt(angle_var));
  fclose(fid);  

  // return
  printf("\n");
  return 0;
}
