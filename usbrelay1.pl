use Device::SerialPort;

$portName = "/dev/ttyACM0";
 				 
$serPort = new Device::SerialPort($portName, quiet) || die "Could not open the port specified";

# configure the port	   
$serPort->baudrate(9600);
$serPort->parity("none");
$serPort->databits(8);
$serPort->stopbits(1);
$serPort->handshake("none");
$serPort->buffers(4096, 4096); 
$serPort->lookclear();
$serPort->purge_all;

# send "relay off" command to the device
$serPort->write("relay off 1\r");

sleep(5);

$serPort->purge_all;

$serPort->write("relay on 1\r");

sleep(5);
