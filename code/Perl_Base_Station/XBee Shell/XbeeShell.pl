#!/usr/bin/perl
# Paul Miller's XBee shell.  Copyright 2009 Paul Miller - LGPL
#
# This is very much ripped off from the cookbook though, that license
# (if any) may apply.  http://poe.perl.org/?POE_Cookbook/Serial_Ports
#
# POE makes this so easy.  Yes, you could do it with blocking
# read/writes and select() and rts/cts checks and things.  Would you
# really want to?
use warnings;
use strict;
use POE qw(Wheel::ReadWrite Filter::Stream);
use Symbol qw(gensym);
use Device::SerialPort;
use Term::ReadKey;
my $dev = uc(shift || "usb0");
$dev = "/dev/tty$dev";
die "no such dev: $dev" unless -c $dev;
my $lck = $dev;
$lck =~ s/\/dev\//\/var\/lock\/LCK../;

# POE is very fun.  You don't write the program flow at all in POE,
# you simply name the things you want to happen and ... they do!
POE::Session->create(
  package_states => [main => [qw(_start got_port got_console got_error byebye)]]
);
POE::Kernel->run();
exit 0;    # notice, run() doesn't return until the program is done.

sub _start {
  my ($kernel, $heap) = @_[KERNEL, HEAP];

  # The XBee won't work quite right in line mode, not if you want to
  # be able to interact with the +++ command mode.  Also, I have mine
  # set to 115200, you'd have to change the baud rate to use this if
  # yours is set to default.
  # ATBD 7
  ReadMode 'ultra-raw';

  # ultra-raw gets really serious about grabbing the actual bytes
  # from the serial port.
  my $handle = gensym();
  my $port = tie(*$handle, "Device::SerialPort", $dev, 0, $lck);
  unless ($port) {
    my $err = $!;
    ReadMode 'restore';
    $err = "lockfile exists: $lck" if $err =~ m/File exists/;
    die "can't open port: $err";
  }
  $port->datatype('raw');
  $port->baudrate(115200);    # the baud rate, default is 9600
  $port->databits(8);
  $port->stopbits(1);
  $port->parity("none");
  #$port->handshake("rts");
 # $port->write_settings;

  # We have two "wheels," streams that handle input and output.   This
  # type of wheel is specifically for reading and writing at the same
  # time, in an event based manner.
  $heap->{port}       = $port;
  $heap->{port_wheel} = POE::Wheel::ReadWrite->new(
    Handle     => $handle,
    Filter     => $heap->{output_stream} = POE::Filter::Stream->new,
    InputEvent => "got_port",
    ErrorEvent => "got_error",
  );
  $heap->{port_wheel}->put("[enter: $ENV{USER}\@$ENV{HOSTNAME}:$dev]\r");
  $heap->{cons_wheel} = POE::Wheel::ReadWrite->new(
    InputHandle  => \*STDIN,
    OutputHandle => \*STDOUT,
    Filter       => $heap->{input_stream} = POE::Filter::Stream->new,
    InputEvent   => "got_console",
    ErrorEvent   => "got_error",
  );
  $heap->{cons_wheel}->put("Press ^D to stop.\r\n");
}

sub got_port {
  my ($heap, $data) = @_[HEAP, ARG0];

  # We've received something from the XBee, so we reformulate it for the
  # terminal a little and pass it to the console.
  #$data =~ s/\x0d/\x0d\x0a/g;
  #$data =~ s/([^\x0d\x0a[:print:]])/"0x" . unpack("H*", $1)/eg;
  $heap->{cons_wheel}->put($data);
}

sub got_console {
  my ($heap, $data) = @_[HEAP, ARG0];

  # we've received something from the console
  if ($data =~ s/(\x03|\x04|\x1c)//g) {    # 3 ^C, 4 ^D, 28 ^|
        # if we receive a ^C, ^D or ^|, break out of the program
    ReadMode 'restore';
    $heap->{port_wheel}->put("[exit: $ENV{USER}\@$ENV{HOSTNAME}:$dev]\r");
    $heap->{cons_wheel}->put("Bye!\r\n");
    $poe_kernel->delay("byebye", 1);
  }

  # echo the input locally
  $heap->{port_wheel}->put($data) if $data;

  # reformulate new input for the XBee a little and pass it to the
  # serial device.
  $data =~ s/\x0d/\x0d\x0a/g;
  $data =~ s/([^\x0d\x0a[:print:]])/"0x" . unpack("H*", $1)/eg;
  $heap->{cons_wheel}->put($data) if $data;
}

sub byebye {
  my $heap = $_[HEAP];

  # once the wheels are gone, POE will notice there's nothing left to
  # do and the program will exit
  delete $heap->{cons_wheel};
  delete $heap->{port_wheel};
}

sub got_error {
  my $heap = $_[HEAP];
  $heap->{cons_wheel}->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $heap->{cons_wheel}->put("bye!");
  delete $heap->{cons_wheel};
  delete $heap->{port_wheel};
}
