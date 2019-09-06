// Applied Geomechanics LILY 8008 data collection program
//
// by: Scott DeWolf
//
// modified from user sawdust's answer at https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
//
//  created: Wednesday, September 27, 2017 (2017270)
// modified: Thursday, September 28, 2017 (2017271)

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

const double fs = 1.0; // sample rate (in Hz)

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

  // variables for yearday filenames
  int year, day;
  time_t rawtime = time(NULL);
  struct tm ft;
  char fn[200];
  FILE *fid;

  // variables for making year/day directories
  struct stat st = {0};
  char path[200];

  // variables for data collection/averaging
  unsigned char buf[200];
  int fd, i, N = 0;
  char ae1_char[8], an1_char[8], rkd_char[6];
  double ae1 = 0, an1 = 0, rkd = 0;

  // open serial port
  fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_SYNC);

  // configure serial port: baudrate 19200, 8 bits, no parity, 1 stop bit
  set_interface_attribs(fd, B19200);

  // discard first 10 LILY serial buffer reads
  for (i = 0; i < 10; i++)
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
      // read LILY serial buffer
      read(fd, buf, sizeof(buf) - 1);

      // parse LILY serial buffer
      sprintf(ae1_char, "%c%c%c%c%c%c%c%c", buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
      sprintf(an1_char, "%c%c%c%c%c%c%c%c", buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17]);
      sprintf(rkd_char, "%c%c%c%c%c%c", buf[26], buf[27], buf[28], buf[29], buf[30], buf[31]);

      // running sum for averaging data
      ae1 += atof(ae1_char);
      an1 += atof(an1_char);
      rkd += atof(rkd_char);

      // increment loop counter
      N++;

      // update epoch time with microseconds
      gettimeofday(&tv, NULL);
      isc = (uint64_t)(tv.tv_sec);
      usc = (uint32_t)(tv.tv_usec);
      t = (double)isc + (double)usc / 1000000;
    }

    // compute average tilts, strains and temperatures
    ae1 = ae1 / (double)N;
    an1 = an1 / (double)N;
    rkd = rkd / (double)N;

    // recompute isec and usec to match t_center
    isc = (uint64_t)t_center;
    usc = (uint32_t)round(1000000 * (t_center - isc));

    // display results
    printf("t = %9.4f  N = %i  ae1 = %0.5f  an1 = %0.5f  rkd = %0.5f\n", t_center, N, ae1, an1, rkd);

    // get year and day number
    gmtime_r(&rawtime, &ft);
    rawtime = time(NULL);
    year = ft.tm_year+1900;
    day =  ft.tm_yday+1;

    // create /home/station/LabJack/data/yyyy path if it is not present
    //sprintf(path, "/home/sbf1/LabJack/data/%4i", year);
    sprintf(path, "/home/lab2/LabJack/data/%4i", year);
    if (stat(path, &st) == -1)
      mkdir(path, 0755);

    // create /home/station/LabJack/data/yyyy/ddd path if it is not present
    //sprintf(path, "/home/sbf1/LabJack/data/%4i/%03i", year, day);
    sprintf(path, "/home/lab2/LabJack/data/%4i/%03i", year, day);
    if (stat(path, &st) == -1)
      mkdir(path, 0755);

    // write binary data
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-isc.bin", path, year, day);
    fid = fopen(fn, "ab");
    fwrite(&isc, sizeof(isc), 1, fid);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-usc.bin", path, year, day);
    fid = fopen(fn, "ab");
    fwrite(&usc, sizeof(usc), 1, fid);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-ae1.bin", path, year, day);
    fid = fopen(fn, "ab");
    fwrite(&ae1, sizeof(ae1), 1, fid);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-an1.bin", path, year, day);
    fid = fopen(fn, "ab");
    fwrite(&an1, sizeof(an1), 1, fid);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-rkd.bin", path, year, day);
    fid = fopen(fn, "ab");
    fwrite(&rkd, sizeof(rkd), 1, fid);
    fclose(fid);

    /* write text data
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-isc.txt", path, year, day);
    fid = fopen(fn, "a");
    fprintf(fid, "%lu\n", isc);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-usc.txt", path, year, day);
    fid = fopen(fn, "a");
    fprintf(fid, "%06u\n", usc);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-ae1.txt", path, year, day);
    fid = fopen(fn, "a");
    fprintf(fid, "%0.32f\n", ae1);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-an1.txt", path, year, day);
    fid = fopen(fn, "a");
    fprintf(fid, "%0.32f\n", an1);
    fclose(fid);
    sprintf(fn, "%s/sbf1-lil1-%4i%03i-rkd.txt", path, year, day);
    fid = fopen(fn, "a");
    fprintf(fid, "%0.32f\n", rkd);
    fclose(fid);*/

    // reset loop variables
    N = 0;
    ae1 = 0;
    an1 = 0;
    rkd = 0;
  }

  // close (this will never happen under normal operation)
  close(fd);

  return 0;
}
