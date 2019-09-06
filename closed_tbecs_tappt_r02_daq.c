// 4.5in Closed TBECS TAPPT Rev.02 data collection program
//
// by: Scott DeWolf
//
// modified from https://labjack.com/sites/default/files/software/labjack_ljm_software_2016_05_15_i386.tar_0.gz/labjack_ljm_examples/examples/ain/dual_ain_loop.c
//
//  created: Monday, June 26, 2017 (2017177)
// modified: Wednesday, March 6, 2019 (2019065)
//  history:
//           [2016239] - created document
//           [2018115] - major revision to include direct recording to miniSEED
//           [2018272] - changed output from calibrated float32 to uncalibrated int32
//           [2018354] - changed network code from PB to 2J
//           [2019064] - changed output from uncalibrated int32 to uncalibrated float32
//           [2019065] - changed output from uncalibrated float32 to calibrated int32 (1 count = 1E-12 m)
//

#include <stdio.h>
#include <time.h>
#include <math.h>
#include "LabJackM.h"
#include "LJM_Utilities.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

// function definitions
double compute_fs(int16_t SRF, int16_t SRM);
void write_mseed(char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, double data, int chan_idx);
void write_mseed_header(char *fn, int SqNu, char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001);
void append_mseed(char *fn, int SqNu, uint16_t SpNu, double data);
int last_mseed_seqnum(char *fn);

// global constants
const int16_t SRF = 2;                // Sample Rate Factor
const int16_t SRM = -10;              // Sample Rate Mutiplier
const uint8_t EF = 3;                 // Encoding Format (3 = 32-bit signed integer)

// global variables for writing to miniSEED volumes
const int NumSamp[7] = {1008,1008,1008,1008,1008,1008,1008}; // (Record Length - Header Size) / Data Size = (2^12 - 64) / sizeof(data)
int SeqNum[7] = {0,0,0,0,0,0,0}, SampNum[7] = {1,1,1,1,1,1,1};

int main()
{

  // variables for error handling
  int err, handle;
  int errorAddress = INITIAL_ERR_ADDRESS;

  // variables for configuring the AINs
  enum { NUM_FRAMES_CONFIG = 32 };
  const char * aNamesConfig[NUM_FRAMES_CONFIG] = \
        {"AIN0_NEGATIVE_CH", "AIN0_RANGE", "AIN0_RESOLUTION_INDEX", "AIN0_SETTLING_US",
	 "AIN1_NEGATIVE_CH", "AIN1_RANGE", "AIN1_RESOLUTION_INDEX", "AIN1_SETTLING_US",
	 "AIN2_NEGATIVE_CH", "AIN2_RANGE", "AIN2_RESOLUTION_INDEX", "AIN2_SETTLING_US",
	 "AIN3_NEGATIVE_CH", "AIN3_RANGE", "AIN3_RESOLUTION_INDEX", "AIN3_SETTLING_US",
	 "AIN4_NEGATIVE_CH", "AIN4_RANGE", "AIN4_RESOLUTION_INDEX", "AIN4_SETTLING_US",
	 "AIN5_NEGATIVE_CH", "AIN5_RANGE", "AIN5_RESOLUTION_INDEX", "AIN5_SETTLING_US",
	 "AIN6_NEGATIVE_CH", "AIN6_RANGE", "AIN6_RESOLUTION_INDEX", "AIN6_SETTLING_US",
	 "AIN7_NEGATIVE_CH", "AIN7_RANGE", "AIN7_RESOLUTION_INDEX", "AIN7_SETTLING_US"};
  const double aValuesConfig[NUM_FRAMES_CONFIG] = {199, 10.0, 12, 0,  // AIN0 +x tilt
						   199, 10.0, 12, 0,  // AIN1 +y tilt
						   199, 10.0, 12, 0,  // AIN2 0-degree horizontal strain
						   199, 10.0, 12, 0,  // AIN3 120-degree horizontal strain
						   199, 10.0, 12, 0,  // AIN4 240-degree horizontal strain
						   199, 10.0, 12, 0,  // AIN5 vertical strain
						     7, 10.0, 12, 0,  // AIN6 +Temperature
						   199, 10.0, 12, 0}; // AIN7 -Temperature

  // variables for reading AIN values
  enum { NUM_FRAMES_AIN = 7 };
  double aValuesAIN[NUM_FRAMES_AIN] = {0};
  const char * aNamesAIN[NUM_FRAMES_AIN] = {"AIN0", "AIN1", "AIN2", "AIN3", "AIN4", "AIN5", "AIN6"};

  // variables for getting the epoch time with microseconds
  struct timeval tv;
  uint64_t isc; uint32_t usc;
  double t, t_center, t_stop;

  // variables for getting the year, doy, hours, minutes, and seconds
  time_t t_temp;
  struct tm tt;

  // variables for data collection/averaging
  int N = 0;
  double ax = 0, ay = 0, s1 = 0, s2 = 0, s3 = 0, sz = 0, kd = 0;
  double fs;

  // compute sample rate (in Hz)
  fs = compute_fs(SRF, SRM);

  // power cycle and open the LabJack T7 for the North Avant Field 4.5in Closed TBECS TAPPT Rev.02
  system("/usr/bin/perl /home/avn3/LabJack/usbrelay0.pl");
  err = LJM_Open(LJM_dtT7, LJM_ctETHERNET, "470012892", &handle);
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

      // running sum for averaging data
      ax += aValuesAIN[0];
      ay += aValuesAIN[1];
      s1 += aValuesAIN[2];
      s2 += aValuesAIN[3];
      s3 += aValuesAIN[4];
      sz += aValuesAIN[5];
      kd += aValuesAIN[6];

      // increment loop counter
      N++;

      // update epoch time with microseconds
      gettimeofday(&tv, NULL);
      isc = (uint64_t)(tv.tv_sec);
      usc = (uint32_t)(tv.tv_usec);
      t = (double)isc + (double)usc / 1000000;
    }

    // compute average tilts, strains and temperatures
    ax = ax / (double)N;
    ay = ay / (double)N;
    s1 = s1 / (double)N;
    s2 = s2 / (double)N;
    s3 = s3 / (double)N;
    sz = sz / (double)N;
    kd = kd / (double)N;

    // apply calibrations
    //ax=590.646944*pow(ax,5)-125.962646*pow(ax,4)- 388.571950*pow(ax,3)+ 2367.366696*pow(ax,2)+237500.099002*ax-   782.0485880+35342.4707140044;
    //ay=490.301594*pow(ay,5)-183.396247*pow(ay,4)-1034.807065*pow(ay,3)+ 1677.877650*pow(ay,2)+257743.474138*ay+   2810.266251-17608.0703885046;
    //s1= 10.143672*pow(s1,5)-206.419941*pow(s1,4)+1643.712411*pow(s1,3)-10850.807271*pow(s1,2)+246426.761913*s1+3963805.447578-2964317.77565509;
    //s2=  6.102497*pow(s2,5)-126.301530*pow(s2,4)+ 974.903101*pow(s2,3)- 9738.866283*pow(s2,2)+261056.128754*s2+3959927.660665-5461293.29323092;
    //s3=  1.832994*pow(s3,5)- 34.801200*pow(s3,4)+ 458.782375*pow(s3,3)-10505.894894*pow(s3,2)+271199.827043*s3+3961077.599171-4608677.90802965;
    //sz= 13.751110*pow(sz,5)-294.892644*pow(sz,4)+2264.236547*pow(sz,3)- 9894.831480*pow(sz,2)+226365.048828*sz+3959323.691049-4273908.93351376;

    // recompute isec and usec to match t_center
    isc = (uint64_t)t_center;
    usc = (uint32_t)round(1000000 * (t_center - isc));

    // compute Year, DayOfYear, Hours, Minutes, and Seconds from t_center
    t_temp = (time_t)isc;
    memcpy(&tt, gmtime(&t_temp), sizeof(struct tm));

    // display results
    printf("t = %i:%03i:%02i:%02i:%02i.%06i  N = %i  ax = %0.5f  ay = %0.5f  s1 = %0.5f  s2 = %0.5f  s3 = %0.5f  sz = %0.5f  kd = %0.5f\n", tt.tm_year+1900, tt.tm_yday+1, tt.tm_hour, tt.tm_min, tt.tm_sec, usc, N, ax, ay, s1, s2, s3, sz, kd);

    // apply calibrations (1 count = 1E-12 m)
    ax=179507.3225190506200306*pow(ax,5)-38282.1201270477613434*pow(ax,4)-118093.4076375856238883*pow(ax,3)+719481.6821073147002608*pow(ax,2)+72180186.9633625894784927*ax+10522637.8121735211461782;
    ay=149010.7198039616923779*pow(ay,5)-55737.1362173277884722*pow(ay,4)-314494.8896113458322361*pow(ay,3)+509934.6190561389084905*pow(ay,2)+78332481.6705755293369293*ay-4494977.0440397597849369;
    s1=515.2985601768933748*pow(s1,5)-10486.1330110110302485*pow(s1,4)+83500.5904828658094630*pow(s1,3)-551221.0093463045777753*pow(s1,2)+12518479.5051706321537495*s1+50771368.7720859646797180;
    s2=310.0068297741872811*pow(s2,5)-6416.1177080221868891*pow(s2,4)+49525.0775287273354479*pow(s2,3)-494734.4071620409376919*pow(s2,2)+13261651.3407142497599125*s2-76301788.5156911313533783;
    s3=93.1160882272134529*pow(s3,5)-1767.9009701423797196*pow(s3,4)+23306.1446351256599883*pow(s3,3)-533699.4606133586494252*pow(s3,2)+13776951.2137680370360613*s3-32834530.5010114014148712;
    sz=698.5563976893054132*pow(sz,5)-14980.5462963050813414*pow(sz,4)+115023.2165908774477430*pow(sz,3)-502657.4391966568073258*pow(sz,2)+11499344.4804664254188538*sz-16256409.8159562051296234;
    kd=214748364.8*(kd+0.895270586013794);

    // create or append miniSEED volume
    write_mseed("E1", "VAX", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), ax, 0);
    write_mseed("E1", "VAY", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), ay, 1);
    write_mseed("E1", "VS1", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), s1, 2);
    write_mseed("E1", "VS2", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), s2, 3);
    write_mseed("E1", "VS3", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), s3, 4);
    write_mseed("E1", "VSZ", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), sz, 5);
    write_mseed("E1", "VKD", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), kd, 6);

    // reset loop variables
    N = 0;
    ax = 0;
    ay = 0;
    s1 = 0;
    s2 = 0;
    s3 = 0;
    sz = 0;
    kd = 0;
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

void write_mseed(char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, double data, int chan_idx)
{
  // variables for making yearday filename and year/day directory
  struct stat st = {0};
  char path[200];
  char fn[200];

  // create /home/station/Data/yyyy path if it is not present
  sprintf(path, "/home/avn3/Data/%4i", Yr);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create /home/station/LabJack/data/yyyy/ddd path if it is not present
  sprintf(path, "/home/avn3/Data/%4i/%03i", Yr, DoY);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create full path and filename
  sprintf(fn, "%s/2J.AVN3.%s.%s.%i.%03i.mseed", path, LI, CI, Yr, DoY);

  // the file doesn't exist, i.e., this is the first time writing to a new file:
  // 1) on startup
  // 2) at the beginning of a new day
  // 3) or it was deleted during data acquisition (how rude!)
  if ( (stat(fn, &st) == -1) )
  {
    SeqNum[chan_idx] = 1;
    SampNum[chan_idx] = 1;
    write_mseed_header(fn, SeqNum[chan_idx], LI, CI, Yr, DoY, Hr, Mn, Sc, S0001);
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], data);
    SampNum[chan_idx]++;
  }
  // the file exists but SeqNum = 0, e.g., data acquisition is restarted during a given day
  else if ( (stat(fn, &st) == 0) & (SeqNum[chan_idx] == 0) )
  {
    SeqNum[chan_idx] = last_mseed_seqnum(fn);
    write_mseed_header(fn, SeqNum[chan_idx], LI, CI, Yr, DoY, Hr, Mn, Sc, S0001);
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], data);
    SampNum[chan_idx]++;
  }
  // the first sample to be written to a new data record block
  else if (SampNum[chan_idx] == 1)
  {
    write_mseed_header(fn, SeqNum[chan_idx], LI, CI, Yr, DoY, Hr, Mn, Sc, S0001);
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], data);
    SampNum[chan_idx]++;
  }
  // the last sample to be written to an existing data record block
  else if (SampNum[chan_idx] == NumSamp[chan_idx])
  {
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], data);
    SeqNum[chan_idx]++;
    SampNum[chan_idx] = 1;
  }
  // just a normal file write, i.e., adding data to the end of data record block in an existing file 
  else // if (SampNum[chan_idx] < NumSamp)
  {
    append_mseed(fn, SeqNum[chan_idx], (uint16_t)SampNum[chan_idx], data);
    SampNum[chan_idx]++;
  }
}

void write_mseed_header(char *fn, int SqNu, char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001)
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
  fprintf(fid, "AVN3 ");                 // Station Identifier Code
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

void append_mseed(char *fn, int SqNu, uint16_t SpNu, double data)
{
  // open file
  FILE *fid;
  fid = fopen(fn, "r+");

  // seek to and update sample number in header block
  fseek(fid, (SqNu-1)*4096+30, SEEK_SET);
  fwrite(&SpNu, sizeof(SpNu), 1, fid);

  // convert double to int32
  int32_t data32;
  data32 = (int32_t)data;  

  // seek to and write latest sample to data block
  fseek(fid, (SqNu-1)*4096+64+(SpNu-1)*sizeof(data32), SEEK_SET);
  fwrite(&data32, sizeof(data32), 1, fid);

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

