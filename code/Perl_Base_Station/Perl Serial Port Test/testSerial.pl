#! /bin/perl

use Device::SerialPort;

use strict;
use warnings;

my $serialPort = Device::SerialPort->new("/dev/ttyS0");
$serialPort->databits(8);
$serialPort->baudrate(115200);
$serialPort->parity("none");
$serialPort->stopbits(1);

#my $count = 0;

#for( my $i = 0; $i < 20; $i++){
#    print "Transmit: >" . $i . "<\n";
#    $serialPort->write("$i\r");
#}

while(1){
    my $char = $serialPort->lookfor();
    if ($char){
	print "Received >" . $char . "<\n";
    }
}
