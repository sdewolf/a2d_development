// 3-Fringe Interferometer Data AcQuisition program using the LabJack T7 and LJM_eReadNames function
// for the AOFS-CC Rev.04 at the North Avant Field Borehole 4 (AVN4-ACC4)
//
// by: Scott DeWolf
//
// modified from https://labjack.com/sites/default/files/software/labjack_ljm_software_2016_05_15_i386.tar_0.gz/labjack_ljm_examples/examples/ain/dual_ain_loop.c
//
//  created: Friday, November 17, 2017 (2017321)
// modified: Thursday, December 20, 2018 (2018354)
//  history:
//           [2017321] - created document
//           [2018354] - changed network code from PB to 2J
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <LabJackM.h>
#include "LJM_StreamUtilities.h"

// function definitions
double compute_fs(int16_t SRF, int16_t SRM);
double threefringe_phase(double x, double y, double z);
void write_mseed(char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, uint8_t EF, double data, int chan_idx);
void write_mseed_header(char *fn, int SqNu, char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, uint8_t EF);
void append_mseed(char *fn, int SqNu, uint16_t SpNu, uint8_t EF, double data);
int last_mseed_seqnum(char *fn);

// global constants
const int16_t SRF = 20;               // Sample Rate Factor
const int16_t SRM = 1;                // Sample Rate Multiplier
const double pi = 3.1415926535897932; // can't live without pi!

// non-dimensional ellipse parameters
const double cx = -0.25401433640831838633999950616271;
const double cy =  0.49982098573571354105382624766207;
const double cz = -0.55350689053944612805224778639968;
const double sx =  0.47605067102574716297880286219879;
const double sy = -0.01927205902745755122795756619780;
const double sz = -0.96175425280410009598597298463574;

// global variables for phase computations
double p_old = 0, p_new = 0;
int M = 0;

// global variables for writing to miniSEED volumes
const int NumSamp[4] = {2016,2016,2016,504}; // (Record Length - Header Size) / Data Size = (2^12 - 64) / sizeof(data)
int SeqNum[4] = {0,0,0,0}, SampNum[4] = {1,1,1,1};

int main(void)
{
  // variables for error handling
  int err, handle;
  int errorAddress = INITIAL_ERR_ADDRESS;

  // variables for configuring the AINs
  enum { NUM_FRAMES_CONFIG = 12 };
  const char *aNamesConfig[NUM_FRAMES_CONFIG] = \
        {"AIN0_NEGATIVE_CH", "AIN0_RANGE", "AIN0_RESOLUTION_INDEX", "AIN0_SETTLING_US",
	 "AIN1_NEGATIVE_CH", "AIN1_RANGE", "AIN1_RESOLUTION_INDEX", "AIN1_SETTLING_US",
	 "AIN2_NEGATIVE_CH", "AIN2_RANGE", "AIN2_RESOLUTION_INDEX", "AIN2_SETTLING_US"};
  const double aValuesConfig[NUM_FRAMES_CONFIG] = {199, 10.0, 0, 0,  // AIN0 x
						   199, 10.0, 0, 0,  // AIN1 y
						   199, 10.0, 0, 0}; // AIN2 z

  // variables for reading AIN values
  enum { NUM_FRAMES_AIN = 3 };
  double aValuesAIN[NUM_FRAMES_AIN] = {0};
  const char *aNamesAIN[NUM_FRAMES_AIN] = {"AIN0", "AIN1", "AIN2"};

  // variables for getting the epoch time with microseconds
  struct timeval tv;
  uint64_t isc; uint32_t usc;
  double t, t_center, t_stop;

  // variables for getting the year, doy, hours, minutes, and seconds
  time_t t_temp;
  struct tm tt;

  // counter for data averaging
  int N = 0;
  double p = 0;
  double fs;

  // compute sample rate (in Hz)
  fs = compute_fs(SRF, SRM);

  // power cycle and open the LabJack T7 for the North Avant Field OFSI Rev.02
  system("/usr/bin/perl /home/avn4/Data/usbrelay0.pl");
  err = LJM_Open(LJM_dtT7, LJM_ctETHERNET, "470012941", &handle);
  ErrorCheck(err, "LJM_Open");

  // setup and call eWriteNames to configure AINs on the LabJack.
  err = LJM_eWriteNames(handle, NUM_FRAMES_CONFIG, aNamesConfig, aValuesConfig, &errorAddress);
  ErrorCheckWithAddress(err, errorAddress, "LJM_eWriteNames");

  // main data collection and storage (infinite) loop
  while (1)
  {
    // get epoch time in microseconds
    gettimeofday(&tv, NULL);
    isc = (uint64_t)(tv.tv_sec);
    usc = (uint32_t)(tv.tv_usec);
    t = (double)isc + (double)usc / 1000000;
    t_center = (floor(t * fs) + 1) / fs;
    t_stop = t_center + 0.5 / fs;

    // collect data for 1 sample period
    while (t < t_stop)
    {
      // read AINs from the LabJack
      err = LJM_eReadNames(handle, NUM_FRAMES_AIN, aNamesAIN, aValuesAIN, &errorAddress);
      ErrorCheckWithAddress(err, errorAddress, "LJM_eReadNames");

      // compute phase from instantaneous x,y,z
      p += threefringe_phase(aValuesAIN[0], aValuesAIN[1], aValuesAIN[2]);

      // increment loop counter
      N++;

      // update epoch time with microseconds
      gettimeofday(&tv, NULL);
      isc = (uint64_t)(tv.tv_sec);
      usc = (uint32_t)(tv.tv_usec);
      t = (double)isc + (double)usc / 1000000;
    }

    // compute average phase
    p = p / (double)N;

    // recompute isec and usec to match t_center
    isc = (uint64_t)t_center;
    usc = (uint32_t)round(1000000 * (t_center - isc));

    // compute Year, DayOfYear, Hours, Minutes, and Seconds from t_center
    t_temp = (time_t)isc;
    memcpy(&tt, gmtime(&t_temp), sizeof(struct tm));

    // display results
    printf("t = %i:%03i:%02i:%02i:%02i.%06i  N = %i  X1 = %0.5f  Y1 = %0.5f  Z1 = %0.5f  M = %i P1 = %0.5f\n", tt.tm_year+1900, tt.tm_yday+1, tt.tm_hour, tt.tm_min, tt.tm_sec, usc, N, aValuesAIN[0], aValuesAIN[1], aValuesAIN[2], M, p);

    // create or append miniSEED volume
    write_mseed("X1", "AYX", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), 1, aValuesAIN[0], 0);
    write_mseed("Y1", "AYY", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), 1, aValuesAIN[1], 1);
    write_mseed("Z1", "AYZ", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), 1, aValuesAIN[2], 2);
    write_mseed("P1", "BS1", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), 5, p, 3);

    // reset loop variables
    N = 0;
    p = 0;
  }

  // close (this will never will happen under normal operation...)
  err = LJM_Close(handle);
  ErrorCheck(err, "LJM_Close");

  return LJME_NOERROR;
}

double compute_fs(int16_t SRF, int16_t SRM)
{

  double fs;
  // If Sample rate factor > 0 and Sample rate Multiplier > 0,
  if ((SRF > 0) & (SRM > 0))
  {
    // Then nominal Sample rate = Sample rate factor X Sample rate multiplier
    fs = (double)SRF * (double)SRM;
  }
  // If Sample rate factor > 0 and Sample rate Multiplier < 0,
  else if ((SRF > 0) & (SRM < 0))
  {
    // Then nominal Sample rate = -1 X Sample rate factor / Sample rate multiplier
    fs = -1 * (double)SRF / (double)SRM;
  }
  // If Sample rate factor < 0 and Sample rate Multiplier > 0,
  else if ((SRF < 0) & (SRM > 0))
  {
    // Then nominal Sample rate = -1 X Sample rate multiplier / Sample rate factor
    fs = -1 * (double)SRM / (double)SRF;
  }
  // If Sample rate factor < 0 and Sample rate Multiplier < 0,
  else if ((SRF < 0) & (SRM < 0))
  {
    // Then nominal Sample rate = 1/ (Sample rate factor X Sample rate multiplier)
    fs = 1 / ( (double)SRF * (double)SRM );
  }
  return fs;
}

double threefringe_phase(double x, double y, double z)
{
  // declare intermediate variables
  double cosp, sinp;

  // compute phase
  cosp = cx * x + cy * y + cz * z;
  sinp = sx * x + sy * y + sz * z;
  p_new = atan2(sinp, cosp) + 2 * M * pi;

  // unwrap phase
  if (abs(p_new - p_old) > pi)
  {
    if (p_new < p_old)
    {
      M++;
      p_new = p_new + 2 * pi;
    }
    else // (p_new > p_old)
    {
      M--;
      p_new = p_new - 2 * pi;
    }
  }
  p_old = p_new;

  // return instantaneous phase
  return p_new;
}

void write_mseed(char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, uint8_t EF, double data, int chan_idx)
{
  // variables for making yearday filename and year/day directory
  struct stat st = {0};
  char path[200];
  char fn[200];

  // create /home/station/Data/yyyy path if it is not present
  sprintf(path, "/home/avn4/Data/%4i", Yr);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create /home/station/LabJack/data/yyyy/ddd path if it is not present
  sprintf(path, "/home/avn4/Data/%4i/%03i", Yr, DoY);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create full path and filename
  sprintf(fn, "%s/2J.AVN4.%s.%s.%i.%03i.mseed", path, LI, CI, Yr, DoY);

  // the file doesn't exist, i.e., this is the first time writing to a new file:
  // 1) on startup
  // 2) at the beginning of a new day
  // 3) or it was deleted during data acquisition (how rude!)
  if ( (stat(fn, &st) == -1) )
  {
    SeqNum[chan_idx] = 1;
    SampNum[chan_idx] = 1;
    write_mseed_header(fn, SeqNum[chan_idx], LI, CI, Yr, DoY, Hr, Mn, Sc, S0001, EF);
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], EF, data);
    SampNum[chan_idx]++;
  }
  // the file exists but SeqNum = 0, e.g., data acquisition is restarted during a given day
  else if ( (stat(fn, &st) == 0) & (SeqNum[chan_idx] == 0) )
  {
    SeqNum[chan_idx] = last_mseed_seqnum(fn);
    write_mseed_header(fn, SeqNum[chan_idx], LI, CI, Yr, DoY, Hr, Mn, Sc, S0001, EF);
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], EF, data);
    SampNum[chan_idx]++;
  }
  // the first sample to be written to a new data record block
  else if (SampNum[chan_idx] == 1)
  {
    write_mseed_header(fn, SeqNum[chan_idx], LI, CI, Yr, DoY, Hr, Mn, Sc, S0001, EF);
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], EF, data);
    SampNum[chan_idx]++;
  }
  // the last sample to be written to an existing data record block
  else if (SampNum[chan_idx] == NumSamp[chan_idx])
  {
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], EF, data);
    SeqNum[chan_idx]++;
    SampNum[chan_idx] = 1;
  }
  // just a normal file write, i.e., adding data to the end of data record block in an existing file 
  else // if (SampNum[chan_idx] < NumSamp)
  {
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], EF, data);
    SampNum[chan_idx]++;
  }
}

void write_mseed_header(char *fn, int SqNu, char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, uint8_t EF)
{
  // create (if necessary) and open file, scan to the end, write zeros, and rewind
  FILE *fid;
  struct stat st = {0};
  if (stat(fn, &st) == -1)
    fid = fopen(fn, "w+");
  else
    fid = fopen(fn, "r+");
  fseek(fid, (SqNu-1)*4096, SEEK_SET);
  char buff[4096] = {0};
  fwrite(buff, 1, 4096, fid);
  fseek(fid, (SqNu-1)*4096, SEEK_SET);

  // Fixed Section of Data Header (48 bytes)
  fprintf(fid, "%06i", SqNu);            // Sequence Number
  fprintf(fid, "D");                     // Data Quality Indicator
  fprintf(fid, " ");                     // Reserved Byte
  fprintf(fid, "AVN4 ");                 // Station Identifier Code
  fprintf(fid, "%s", LI);                // Location Identifier
  fprintf(fid, "%s", CI);                // Channel Identifier
  fprintf(fid, "2J");                    // Network Code
  fwrite(&Yr, sizeof(Yr), 1, fid);       // Year
  fwrite(&DoY, sizeof(DoY), 1, fid);     // Day of Year
  fwrite(&Hr, sizeof(Hr), 1, fid);       // Hours
  fwrite(&Mn, sizeof(Mn), 1, fid);       // Minutes
  fwrite(&Sc, sizeof(Sc), 1, fid);       // Seconds
  fprintf(fid, " ");                     // Skip 1 Byte (unused)
  fwrite(&S0001, sizeof(S0001), 1, fid); // Seconds0001 (why not microseconds?)
  uint16_t NoS = 0;
  fwrite(&NoS, sizeof(NoS), 1, fid);     // Number of Samples (none so far...)
  fwrite(&SRF, sizeof(SRF), 1, fid);     // Sample Rate Factor
  fwrite(&SRM, sizeof(SRM), 1, fid);     // Sample Rate Multiplier
  uint8_t AID = 0;
  fwrite(&AID, sizeof(AID), 1, fid);     // Activity Flags
  fwrite(&AID, sizeof(AID), 1, fid);     // IO Flags
  fwrite(&AID, sizeof(AID), 1, fid);     // Data Quality Flags
  uint8_t NBF = 1;
  fwrite(&NBF, sizeof(NBF), 1, fid);     // Number Blockettes to Follow
  int32_t TC = 0;
  fwrite(&TC, sizeof(TC), 1, fid);       // Time Correction
  uint16_t OBD = 64;
  fwrite(&OBD, sizeof(OBD), 1, fid);     // Offset to the Beginning of Data
  uint16_t OFB = 48;
  fwrite(&OFB, sizeof(OFB), 1, fid);     // Offset to the First Blockette
  uint16_t BT = 1000;
  fwrite(&BT, sizeof(BT), 1, fid);       // Blockette Type

  // [1000] Data Only SEED Blockette (8 bytes)
  uint16_t ONB = 0;
  fwrite(&ONB, sizeof(ONB), 1, fid);     // Offset to the Next Blockette
  fwrite(&EF, sizeof(EF), 1, fid);       // Encoding Format
  uint8_t WO = 0;
  fwrite(&WO, sizeof(WO), 1, fid);       // Word Order (0 = little endian)
  uint8_t DRL = 12;
  fwrite(&DRL, sizeof(DRL), 1, fid);     // Data Record Length (12, since 2^12 = 4096)
  uint8_t Res = 0;
  fwrite(&Res, sizeof(Res), 1, fid);     // Reserved

  // close file
  fclose(fid);
}

void append_mseed(char *fn, int SqNu, uint16_t SpNu, uint8_t EF, double data)
{
  // open file
  FILE *fid;
  fid = fopen(fn, "r+");

  // seek to and update sample number in header block
  fseek(fid, (SqNu-1)*4096+30, SEEK_SET);
  fwrite(&SpNu, sizeof(SpNu), 1, fid);

  // if Encoding Format = 1 (implying int16_t) convert fringe data to 16-bit
  int16_t fringe;
  if (EF == 1)
  {
    // convert to int16_t (these calibration numbers are specific to LabJack T7 470012941)
    fringe = (int16_t)((data + 10.57964801788330078125) / 0.000315479119308292865753173828125 - 33540.1015625);
    // seek to and write latest sample to data block
    fseek(fid, (SqNu-1)*4096+64+(SpNu-1)*sizeof(fringe), SEEK_SET);
    fwrite(&fringe, sizeof(fringe), 1, fid);
  }
  // otherwise write phase data (EF = 5 = float64)
  else
  {
    // seek to and write latest sample to data block
    fseek(fid, (SqNu-1)*4096+64+(SpNu-1)*sizeof(data), SEEK_SET);
    fwrite(&data, sizeof(data), 1, fid);
  }

  // close file
  fclose(fid);
}

int last_mseed_seqnum(char *fn)
{
  // open file and seek to the end
  FILE *fid;
  fid = fopen(fn, "r");
  fseek(fid, 0, SEEK_END);

  // compute new Sequence Number from file size and Data Record Length
  int SqNu;
  SqNu = ftell(fid) / 4096 + 1;

  // close file and return Sequence Number
  fclose(fid);
  return SqNu;
}
