// Vaisala WXT520 SN: M2310478 data collection program
//
// by: Scott DeWolf
//
// modified from user sawdust's answer at https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
//
//  created: Thursday, October 19, 2017 (2017292)
// modified: Thursday, December 20, 2018 (2018354)
//  history:
//           [2017292] - created document
//           [2018115] - major revision to include direct recording to miniSEED
//           [2018272] - changed air pressure from VDV to VDO
//           [2018354] - changed network code from PB to 2J
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>

// function definitions
double compute_fs(int16_t SRF, int16_t SRM);
void write_mseed(char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, float data, int chan_idx);
void write_mseed_header(char *fn, int SqNu, char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001);
void append_mseed(char *fn, int SqNu, uint16_t SpNu, float data);
int last_mseed_seqnum(char *fn);

// global constants
const int16_t SRF = 2;                // Sample Rate Factor
const int16_t SRM = -10;              // Sample Rate Mutiplier
const uint8_t EF = 4;                 // Encoding Format (4 = 32-bit float)
const double pi = 3.1415926535897932; // can't live without pi!

// global variables for writing to miniSEED volumes
const int NumSamp[7] = {1008,1008,1008,1008,1008,1008,1008}; // (Record Length - Header Size) / Data Size = (2^12 - 64) / sizeof(data)
int SeqNum[7] = {0,0,0,0,0,0,0}, SampNum[7] = {1,1,1,1,1,1,1};

int set_interface_attribs(int fd, int speed)
{
  struct termios tty;

  // populate tty struct with default values
  tcgetattr(fd, &tty);

  // setup input and output baud rates
  cfsetospeed(&tty, (speed_t)speed);
  cfsetispeed(&tty, (speed_t)speed);

  // setup other serial configs
  tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls
  tty.c_cflag &= ~CSIZE;           // bit mask (what is that?)
  tty.c_cflag |= CS8;              // 8-bit characters
  tty.c_cflag &= ~PARENB;          // no parity bit
  tty.c_cflag &= ~CSTOPB;          // only need 1 stop bit
  tty.c_cflag &= ~CRTSCTS;         // no hardware flowcontrol

  // setup for non-canonical mode
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tty.c_oflag &= ~OPOST;

  // fetch bytes as they become available
  tty.c_cc[VMIN] = 58;
  tty.c_cc[VTIME] = 1;

  // set serial port attributes
  tcsetattr(fd, TCSANOW, &tty);

  return 0;
}

int main()
{

  // variables for getting the epoch time with microseconds
  struct timeval tv;
  uint64_t isc; uint32_t usc;
  double t, t_center, t_stop;

  // variables for getting the year, doy, hours, minutes, and seconds
  time_t t_temp;
  struct tm tt;

  // variables for data collection/averaging
  char buf[200];
  int fd, i, N = 0;
  double fs;
  int Dm;
  float Sm, Ta, Ua, Pa, Ri, Hi;
  double wdx = 0, wdy = 0, ws = 0, ko = 0, io = 0, dv = 0, ro = 0, rh = 0;
  double wd;

  // compute sample rate (in Hz)
  fs = compute_fs(SRF, SRM);

  // open serial port
  fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_SYNC);

  // configure serial port: baudrate 19200, 8 bits, no parity, 1 stop bit
  set_interface_attribs(fd, B19200);

  // discard first 2 WXT520 serial buffer reads
  for (i = 0; i < 2; i++)
    read(fd, buf, sizeof(buf) - 1);

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
      // read WXT520 serial buffer
      read(fd, buf, sizeof(buf) - 1);

      // parse WXT520 serial buffer
      sscanf(buf, "0R0,Dm=%dD,Sm=%fM,Ta=%fC,Ua=%fP,Pa=%fH,Ri=%fM,Hi=%fM", &Dm, &Sm, &Ta, &Ua, &Pa, &Ri, &Hi);

      // running sum for averaging data
      wdx += cos(pi * (double)Dm / 180); // x-wind direction: wdx
      wdy += sin(pi * (double)Dm / 180); // y-wind direction: wdy
      ws += Sm;                          //       wind speed: ws
      ko += Ta;                          //  air temperature: ko
      io += Ua;                          //     air humidity: io
      dv += Pa;                          //     air pressure: dv
      ro += Ri;                          //   rain intensity: ro
      rh += Hi;                          //   hail intensity: rh

      // increment loop counter
      N++;

      // update epoch time with microseconds
      gettimeofday(&tv, NULL);
      isc = (uint64_t)(tv.tv_sec);
      usc = (uint32_t)(tv.tv_usec);
      t = (double)isc + (double)usc / 1000000;
    }

    // compute averages
    wdx = wdx / (double)N;
    wdy = wdy / (double)N;
    wd = 180 * atan2(wdy, wdx) / pi;
    if (wd < 0)
      wd = wd + 360;
    ws = ws / (double)N;
    ko = ko / (double)N;
    io = io / (double)N;
    dv = (dv / 10) / (double)N; // convert from hPa to kPa
    ro = ro / (double)N;
    rh = rh / (double)N;

    // recompute isec and usec to match t_center
    isc = (uint64_t)t_center;
    usc = (uint32_t)round(1000000 * (t_center - isc));

    // compute Year, DayOfYear, Hours, Minutes, and Seconds from t_center
    t_temp = (time_t)isc;
    memcpy(&tt, gmtime(&t_temp), sizeof(struct tm));

    // display results
    printf("t = %i:%03i:%02i:%02i:%02i.%06i  N = %i  wd = %0.1f  ws = %3.2f  ko = %0.2f  io = %0.2f  do = %0.2f  ro = %0.1f  rh = %0.1f\n", tt.tm_year+1900, tt.tm_yday+1, tt.tm_hour, tt.tm_min, tt.tm_sec, usc, N, wd, ws, ko, io, dv, ro, rh);

    // create or append miniSEED volume
    write_mseed("M1", "VWD", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)wd, 0);
    write_mseed("M1", "VWS", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)ws, 1);
    write_mseed("M1", "VKO", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)ko, 2);
    write_mseed("M1", "VIO", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)io, 3);
    write_mseed("M1", "VDO", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)dv, 4);
    write_mseed("M1", "VRO", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)ro, 5);
    write_mseed("M1", "VRH", (uint16_t)tt.tm_year+1900, (uint16_t)tt.tm_yday+1, (uint8_t)tt.tm_hour, (uint8_t)tt.tm_min, (uint8_t)tt.tm_sec, (uint16_t)(usc/100), (float)rh, 6);

    // reset loop variables
    N = 0;
    wdx = 0;
    wdy = 0;
    ws = 0;
    ko = 0;
    io = 0;
    dv = 0;
    ro = 0;
    rh = 0;
  }

  // close (this will never happen under normal operation)
  close(fd);
  return 0;
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

void write_mseed(char *LI, char *CI, uint16_t Yr, uint16_t DoY, uint8_t Hr, uint8_t Mn, uint8_t Sc, uint16_t S0001, float data, int chan_idx)
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

void append_mseed(char *fn, int SqNu, uint16_t SpNu, float data)
{
  // open file
  FILE *fid;
  fid = fopen(fn, "r+");

  // seek to and update sample number in header block
  fseek(fid, (SqNu-1)*4096+30, SEEK_SET);
  fwrite(&SpNu, sizeof(SpNu), 1, fid);

  // seek to and write latest sample to data block
  fseek(fid, (SqNu-1)*4096+64+(SpNu-1)*sizeof(data), SEEK_SET);
  fwrite(&data, sizeof(data), 1, fid);

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
