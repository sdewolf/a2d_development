// 4.5in Closed TBECS TAPPT Rev.02 tiltmeter leveling program
//
// by: Scott DeWolf
//
// modified from https://labjack.com/sites/default/files/software/labjack_ljm_software_2016_05_15_i386.tar_0.gz/labjack_ljm_examples/examples/dio/single_dio_write.c
//
//  created: Monday, June 26, 2017 (2017177)
// modified: Monday, March 19, 2018 (2018078)
//  history:
//           [2017177] - created document
//           [2018078] - dunno
//

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "LabJackM.h"
#include "LJM_Utilities.h"

int lev0(const char * aNamesConfig[8], const double aValuesConfig[8], const char * aNamesAIN[2], double aValuesAIN[2], const char * aNamesDIO[2], double V0[4], char name[1]);

int main()
{

  // variables for configuring and manipulating the x-tiltmeter AINs and DIOs
  const char * xNamesConfig[8] = \
    {"AIN0_NEGATIVE_CH", "AIN0_RANGE", "AIN0_RESOLUTION_INDEX", "AIN0_SETTLING_US",
     "AIN8_NEGATIVE_CH", "AIN8_RANGE", "AIN8_RESOLUTION_INDEX", "AIN8_SETTLING_US"};
  const double xValuesConfig[8] = {199, 10.0, 0, 0,
                                   199, 10.0, 0, 0};
  double xValuesAIN[1] = {0};
  const char * xNamesAIN[2] = {"AIN0", "AIN8"};
  const char * xNamesDIO[2] = {"FIO0", "FIO1"};

  // variables for configuring and manipulating the y-tiltmeter AINs and DIOs
  const char * yNamesConfig[8] = \
    {"AIN1_NEGATIVE_CH", "AIN1_RANGE", "AIN1_RESOLUTION_INDEX", "AIN1_SETTLING_US",
     "AIN9_NEGATIVE_CH", "AIN9_RANGE", "AIN9_RESOLUTION_INDEX", "AIN9_SETTLING_US"};
  const double yValuesConfig[8] = {199, 10, 0, 0,
                                   199, 10, 0, 0};
  double yValuesAIN[1] = {0};
  const char * yNamesAIN[2] = {"AIN1", "AIN9"};
  const char * yNamesDIO[2] = {"FIO2", "FIO3"};
  char * name[2] = {"x", "y"};

  // level x tiltmeter
  printf("\n------------------------\n");
  printf("- leveling x-tiltmeter -\n");
  printf("------------------------\n\n");
  double X0[4] = {0.021118, -0.016270, 0.926724, -5.042287};
  lev0(xNamesConfig, xValuesConfig, xNamesAIN, xValuesAIN, xNamesDIO, X0, name[0]);

  // level y tiltmeter
  printf("\n------------------------\n");
  printf("- leveling y-tiltmeter -\n");
  printf("------------------------\n\n");
  double Y0[4] = {0.021055, -0.030252, 0.951344, -5.184880};
  lev0(yNamesConfig, yValuesConfig, yNamesAIN, yValuesAIN, yNamesDIO, Y0, name[1]);

  // return
  printf("\n");
  return 0;
}

int lev0(const char * aNamesConfig[8], double const aValuesConfig[8], const char * aNamesAIN[2], double aValuesAIN[2], const char * aNamesDIO[2], double V0[4], char name[1])
{

  // variable for absolute tilt calculation
  double Tilt;

  // variables for getting the epoch time with microseconds
  struct timeval tv;
  uint64_t isc; uint32_t usc;
  double t;

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

  // power cycle and open the LabJack T7 for the North Avant Field 4.5in Closed TBECS TAPPT Rev.02
  system("/usr/bin/perl /home/avn3/LabJack/usbrelay0.pl");
  LJM_Open(LJM_dtT7, LJM_ctETHERNET, "470012892", &handle);

  // setup and call eWriteNames to configure AINs on the LabJack
  LJM_eWriteNames(handle, 8, aNamesConfig, aValuesConfig, &errorAddress);

  // toggle the DIOs to prevent runaway after poweup
  LJM_eWriteName(handle, aNamesDIO[0], 1);
  LJM_eWriteName(handle, aNamesDIO[1], 1);

  // read AINs from the LabJack
  LJM_eReadNames(handle, 2, aNamesAIN, aValuesAIN, &errorAddress);

  while(floor(abs(1000*aValuesAIN[0])) > 0)
  {
    if(aValuesAIN[0] < 0)
    {
      LJM_eWriteName(handle, aNamesDIO[0], 0);                         // initiate move +
      usleep(1000);                                                    // let move + for some number of microseconds
      LJM_eWriteName(handle, aNamesDIO[0], 1);                         // stop move +
      LJM_eReadNames(handle, 2, aNamesAIN, aValuesAIN, &errorAddress); // recheck position
      Tilt = V0[0]*pow(aValuesAIN[1],3)+V0[1]*pow(aValuesAIN[1],2)+V0[2]*aValuesAIN[1]+V0[3];
      printf("Tilt < 0 \t Sensor = %0.6f Volts \t Slider Voltage = %0.6f Volts \t Tilt = %0.4f degrees\n", aValuesAIN[0], aValuesAIN[1], Tilt);
      if(Tilt < -5)
      {
        printf("Tiltmeter has hit the - stop.\n");
	LJM_eWriteName(handle, aNamesDIO[1], 0);
	usleep(1000000);
	LJM_eWriteName(handle, aNamesDIO[1], 1);
	break;
      }
    }
    if(aValuesAIN[0] > 0)
    {
      LJM_eWriteName(handle, aNamesDIO[1], 0);                         // initiate move -
      usleep(1000);                                                    // let move - for some number of microseconds
      LJM_eWriteName(handle, aNamesDIO[1], 1);                         // stop move -
      LJM_eReadNames(handle, 2, aNamesAIN, aValuesAIN, &errorAddress); // recheck position
      Tilt = V0[0]*pow(aValuesAIN[1],3)+V0[1]*pow(aValuesAIN[1],2)+V0[2]*aValuesAIN[1]+V0[3];
      printf("Tilt > 0 \t Sensor = %0.6f Volts \t Slider Voltage = %0.6f Volts \t Tilt = %0.4f degrees\n", aValuesAIN[0], aValuesAIN[1], Tilt);
      if(Tilt > 5)
      {
        printf("Tiltmeter has hit the + stop.\n");
	LJM_eWriteName(handle, aNamesDIO[0], 0);
	usleep(1000000);
	LJM_eWriteName(handle, aNamesDIO[0], 1);
        break;
      }
    }
  }

  // get epoch time in microseconds
  gettimeofday(&tv, NULL);
  isc = (uint64_t)(tv.tv_sec);
  usc = (uint32_t)(tv.tv_usec);
  t = (double)isc + (double)usc / 1000000;

  // get year and day number
  gmtime_r(&rawtime, &ft);
  rawtime = time(NULL);

  // create /home/station/LabJack/data/yyyy path if it is not present
  sprintf(path, "/home/avn3/LabJack/data/%4i", ft.tm_year+1900);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create /home/station/LabJack/data/yyyy/ddd path if it is not present
  sprintf(path, "/home/avn3/LabJack/data/%4i/%03i", ft.tm_year+1900, ft.tm_yday+1);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // write results to a file
  sprintf(fn, "%s/avn3-ctt2-%4i%03i-lev.txt", path, ft.tm_year+1900, ft.tm_yday+1);
  fid = fopen(fn, "a");
  fprintf(fid, "t = %9.6f \t %s-tilt = %0.6f degrees.\n", t, name, Tilt);
  fclose(fid);

  LJM_Close(handle);
  return 0;
}
