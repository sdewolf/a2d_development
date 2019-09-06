// Applied Geomechanics LILY 8209 tiltmeter orienting program
//
// by: Scott DeWolf
//
// modified from user sawdust's answer at https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
//
//  created: Wednesday, September 29, 2017 (2017272)
// modified: Wednsday, September 29, 2017 (2017272)
//  history:
//           [2017272] - created document
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

  // variables for averaging the electronic compass azimuths (CCW from North! bastards!)
  double az_sum = 0, az_sq_sum = 0;
  double az_mean, az_var;
  int i;

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

  // variables for data collection
  unsigned char buf[200];
  int fd;
  char az_char[8];

  // open serial port
  fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_SYNC);

  // configure serial port: baudrate 19200, 8 bits, no parity, 1 stop bit
  set_interface_attribs(fd, B19200);

  // discard first 10 LILY serial buffer reads
  for (i = 0; i < 10; i++)
    read(fd, buf, sizeof(buf) - 1);

  printf("\n");
  for (i = 0; i < 100; i++)
  {
    // get epoch time in microseconds
    gettimeofday(&tv, NULL);
    isc = (uint64_t)(tv.tv_sec);
    usc = (uint32_t)(tv.tv_usec);
    t = (double)isc + (double)usc / 1000000;

    // read LILY serial buffer
    read(fd, buf, sizeof(buf) - 1);

    // parse LILY serial buffer
    sprintf(az_char, "%c%c%c%c%c%c", buf[19], buf[20], buf[21], buf[22], buf[23], buf[24]);
    printf("t = %9.4f \t angle = %0.6f degrees \t (%03i of 100).\n", t, 360 - atof(az_char), i + 1);

    // compute running totals for mean and standard deviation
    t_sum += t;
    az_sum += 360 - atof(az_char);
    az_sq_sum += (360 - atof(az_char)) * (360 - atof(az_char));
  }

  // compute and display azimuth mean and variance
  t = t_sum / 100;
  az_mean = az_sum / 100;
  az_var = az_sq_sum / 100 - az_mean * az_mean;
  printf("\nt = %9.6f \t Azimuth = %0.6f +/- %0.6f degrees.\n", t, az_mean, sqrt(az_var));

  // get year and day number
  gmtime_r(&rawtime, &ft);
  rawtime = time(NULL);

  // create /home/station/LabJack/data/yyyy path if it is not present
  sprintf(path, "/home/avn4/LabJack/data/%4i", ft.tm_year+1900);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create /home/station/LabJack/data/yyyy/ddd path if it is not present
  sprintf(path, "/home/avn4/LabJack/data/%4i/%03i", ft.tm_year+1900, ft.tm_yday+1);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // write results to a file
  sprintf(fn, "%s/avn4-lil2-%4i%03i-ori.txt", path, ft.tm_year+1900, ft.tm_yday+1);
  fid = fopen(fn, "a");
  fprintf(fid, "t = %9.6f \t Azimuth = %0.6f +/- %0.6f degrees.\n", t, az_mean, sqrt(az_var));
  fclose(fid);  

  // return
  printf("\n");
  return 0;
}
