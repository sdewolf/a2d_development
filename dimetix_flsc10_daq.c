// Dimetix FLS-C10 data collection program
//
// by: Scott DeWolf
//
// modified from user sawdust's answer at https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
//
//  created: Wednesday, February 20, 2019 (2019051)
// modified: Wednesday, February 20, 2019 (2019051)
//  history:
//           [2019051] - created document
//

#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// as its name implies...
int set_interface_attribs(int fd)
{
  struct termios tty;

  if (tcgetattr(fd, &tty) < 0) {
    printf("Error from tcgetattr: %s\n", strerror(errno));
    return -1;
  }

  cfsetospeed(&tty, B19200);  // set baudrate
  cfsetispeed(&tty, B19200);

  tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS7;         // 7-bit
  tty.c_cflag |= PARENB;      // even parity
  tty.c_cflag &= ~CSTOPB;     // 1 stop bit
  tty.c_cflag &= ~CRTSCTS;    // no flow control

  // setup for canonical mode
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ISIG | IEXTEN);
  tty.c_lflag &= (ICANON);
  tty.c_oflag &= ~OPOST;

  // fetch bytes as they become available
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    printf("Error from tcsetattr: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

// main data collection function
int main()
{
  // variables for getting data from serial port
  char *portname = "/dev/ttyUSB0";
  int fd, dur, N = 0;
  char buf[255], des[255];
  float D, T, I;
  double d = 0, d2 = 0;

  // variables for getting the epoch time with microseconds
  struct timeval tv;
  uint64_t isc; uint32_t usc;
  double t_start, t_now;

  // variables for making yearday filename and year/day directory
  struct stat st = {0};
  char path[200];
  char fn[200];
  FILE *fid;

  // variables for getting the year, doy, hours, minutes, and seconds
  time_t t_temp;
  struct tm tt;

  // open serial port
  fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    printf("Error opening %s: %s\n", portname, strerror(errno));
    return -1;
  }
  // baudrate 19200, 7 bits, even parity, 1 stop bit, no flow control
  set_interface_attribs(fd);

  // ask user for duration
  printf("\nMaximum time to record (integer number of seconds): ");
  scanf("%d",&dur);

  // ask user for strainmeter name
  printf("\nStrainmeter designation: ");
  scanf("%s",des);
  printf("\n");

  // get starting time
  gettimeofday(&tv, NULL);
  isc = (uint64_t)(tv.tv_sec);
  usc = (uint32_t)(tv.tv_usec);
  t_start = (double)isc + (double)usc / 1000000;

  // compute Year, DayOfYear, Hours, Minutes, and Seconds
  t_temp = (time_t)isc;
  memcpy(&tt, gmtime(&t_temp), sizeof(struct tm));

  // create /home/station/Data/yyyy path if it is not present
  //sprintf(path, "/home/sm/data/%4i", tt.tm_year+1900);
  sprintf(path, "/home/sdewolf/Data/%4i", tt.tm_year+1900);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create /home/station/LabJack/data/yyyy/ddd path if it is not present
  //sprintf(path, "/home/sm/data/%4i/%03i", tt.tm_year+1900, DoY);
  sprintf(path, "/home/sdewolf/Data/%4i/%03i", tt.tm_year+1900, tt.tm_yday+1);
  if (stat(path, &st) == -1)
    mkdir(path, 0755);

  // create full path and filename
  //sprintf(fn, "/home/sm/data/%4i/%03i/%s.%4i%03i.%02i.%02i.%02i.txt", tt.tm_year+1900, tt.tm_yday+1, des, tt.tm_year+1900, tt.tm_yday+1, tt.tm_hour, tt.tm_min, tt.tm_sec);
  sprintf(fn, "/home/sdewolf/Data/%4i/%03i/%s.%4i%03i.%02i.%02i.%02i.txt", tt.tm_year+1900, tt.tm_yday+1, des, tt.tm_year+1900, tt.tm_yday+1, tt.tm_hour, tt.tm_min, tt.tm_sec);
  fid = fopen(fn, "a");

  // main data collection loop
  while (t_now-t_start<dur)
  {
    // increment loop counter
    N++;

    // request length data
    write(fd, "s0g\n", 4);
    tcdrain(fd);
    read(fd,buf,255); 
    sscanf(buf, "g0g+%f", &D);
    D=D/10000;
    d+=(double)D;
    d2+=pow((double)D,2);

    // request device temperature
    write(fd, "s0t\n", 4);
    tcdrain(fd);
    read(fd,buf,255);
    sscanf(buf, "g0t+%f", &T);
    T=T/10;

    // request received intensity
    write(fd, "s0m+0\n", 6);
    tcdrain(fd);
    read(fd,buf,255); 
    sscanf(buf, "g0m+%f", &I);

    // get current time
    gettimeofday(&tv, NULL);
    isc = (uint64_t)(tv.tv_sec);
    usc = (uint32_t)(tv.tv_usec);
    t_now = (double)isc + (double)usc / 1000000;

    // compute Year, DayOfYear, Hours, Minutes, and Seconds
    t_temp = (time_t)isc;
    memcpy(&tt, gmtime(&t_temp), sizeof(struct tm));

    // display output
    printf("Time = %i:%03i:%02i:%02i:%02i.%06i UTC     N = %03i     Length = %0.4f m     Laser Temp = %2.1f C     Signal = %0.2f%%\n", tt.tm_year+1900, tt.tm_yday+1, tt.tm_hour, tt.tm_min, tt.tm_sec, usc, N, D, T, 100*I/4E6);

    // write data to file
    fprintf(fid, "%lf,%0.4f,%2.1f,%0.0f\n", t_now, D, T, I);

  }            
  fclose(fid);

  printf("\nEDM Measurement: %1.6f +/- %1.6f m\n\n",d/(double)N,sqrt(d2/(double)N-pow(d/(double)N,2)));

}
