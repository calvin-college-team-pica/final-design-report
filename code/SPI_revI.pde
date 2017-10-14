/* 
This is code for using
the Arduino Uno microcontroller to read (and write)
to the ADE7763 energy metering chip, via SPI

Based off the example provided at 
http://arduino.cc/en/Tutorial/BarometricPressureSensor
for using SPI to read a pressure sensor

Connections:
CS: pin 10 :ADE pin 17   
MOSI: pin 11 :ADE pin 20
MISO: pin 12 :ADE pin 19
SCK: pin 13 :ADE pin 18


Nathan Jen
06 May 2011
Engineering 340 Sr. Design Project Team PICA
*/

#include <SPI.h> 				  // include the SPI library

// ADE7763 memory register addresses
const int chan1 = 0x16; 			  // address for channel 1, RMS current
const int chan2 = 0x17;				  // address for channel 2, RMS voltage
const byte READ = 00000000;			  // read command given to communication register, page 50
const byte WRITE = 10000000;		          // write command given to communication register, page 50

// Other important values
const int currentThresh = 2;		          // set the limit for allowable current - pick a value, just for testing
      int voltageData = 0;                        // initialize to a known value - mostly for testing
      int currentData = 0;

// Any pins needed to connect master & slave
const int slaveSelect = 10;
//const int solidState = 6;			  // use this pin to activate the SSR control line once reading is happening

// Setup
void setup() {
  Serial.begin( 9600 );				  // open serial port at 9600 bps
  
  SPI.begin(); 					  // start SPI library

  SPI.setBitOrder( MSBFIRST );
  SPI.setDataMode( SPI_MODE3 );                   // spi_mode3 = high polarity and rising clock
  pinMode( slaveSelect, OUTPUT );                 // digital pot example still uses this
//pinMode( solidState, OUTPUT );	          // commented out until we can read
//analogWrite( solidState, 0 );		          // set the solidState pin to a default "off" value - commented out for same reason as above
  writeRegister( 0x0A, 0x0, 0x0 );	          // disable interrupts
//writeRegister( 0x09, )			  // select sample rates, filter enable & calibration; commented out b/c default looks acceptable
  delay( 100 ); 				  // allow ADE to set up as necessary
  
  Serial.print( "Starting Voltage is " );         // this data would be sent to the base station
  Serial.print( voltageData );
  Serial.print( "\n" );
  Serial.print( "Starting Current is " );         // as would this data
  Serial.print( currentData );			  
  Serial.print( "\n" );
}


/*
// use this loop for testing
void loop() {
  
unsigned testing = readRegister( 0x16 );
  Serial.print( "\n \n Vrms is " );
  Serial.print( testing );
  
  delay( 1000 );
}
*/
  
  
// Do Reading Here
void loop() {

unsigned voltageData = readRegister( 0x17 );	  // unsigned b/c that's what the register is formated as, should be RMS value already by nature of the register
  Serial.print( "\n Voltage is " );
  Serial.print( voltageData );			  // print to the effective Base Station
  Serial.print( "\n" );
  delay( 1000 );                                  // 1000 because we can read straight off the console at that speed

unsigned currentData = readRegister( 0x16 );
  Serial.print( "\n Current is " );
  Serial.print( currentData );		          // print to the 'Base Station'
  Serial.print( "\n" );
  delay( 1000 );  

  if( currentData < currentThresh ) {
  //analogWrite( solidState, 255 );		   // allow SSR to conduct
    Serial.print( "Current OK" );
    Serial.print( "\n" );
    Serial.print( "\n" );
  } else {
  //analogWrite( solidState, 0 );				// the shut-off commmand
    Serial.print( "WARNING: CURRENT EXCEEDS ALLOWABLE LIMIT" );
    Serial.print( "\n" );
    Serial.print( "\n" );

  }
   
}



/***************** FUNCTION DEFINITIONS ********************************/

// Read function, without specifying how many bytes we get back
// all of the Serial.print() commands were used for debugging
  unsigned int readRegister( byte thisRegister ) {
  int result = 0x1;
//   Serial.print( "\n Result: " );
//   Serial.print( result );
  SPI.setDataMode( SPI_MODE3 );
  digitalWrite( slaveSelect, LOW );
  byte dataToSend = READ | thisRegister;
  int printDataToSend = int( dataToSend );
   Serial.print( "\n dataToSend: " );
   Serial.print( printDataToSend );
  delay( 10 );                                      // give it a little time to actually transfer all the data
  result = SPI.transfer( dataToSend );
   Serial.print( "\n First result: " );
   Serial.print( result );
   
  // get the second byte and add to the result
  int second1Result = result << 8;
   Serial.print( "\n Shifted result: " );
   Serial.print( second1Result );  
   
  int result2 = SPI.transfer( 0x00 );
   Serial.print( "\n Second result: " );
   Serial.print( result2 );  
   
  int second2Result = second1Result | result2;
   Serial.print( "\n Or-ed: " );
   Serial.print( second2Result );
  
   
  // get the third byte & add to the result - this is the last shift needed since the registers are 24 bits
  int third1Result = second2Result << 8;
   Serial.print( "\n Shifted result2: " );
   Serial.print( third1Result );

  int result3 = SPI.transfer( 0x00 );
   Serial.print( "\n Third result: " );
   Serial.print( result3 );
   
  int third2Result = third1Result | result3;
   Serial.print( "\n Final result: " );
   Serial.print( third2Result );

  SPI.setDataMode( SPI_MODE2 );                   
  digitalWrite( slaveSelect, HIGH );
  return( second2Result );
}

 
// Write function, as defined by Nathan Jen
void writeRegister( byte thisRegister, byte msbValue, byte lsbValue ) {
  byte destination = WRITE | thisRegister;	        // combine the write command and the specified register
  digitalWrite( slaveSelect, LOW );  	        	// select the chip
  SPI.transfer( destination );				// send destination to communication register
  SPI.transfer( msbValue );				// send the first half of the data
  SPI.transfer( lsbValue );				// send the second half of the data
  digitalWrite( slaveSelect, HIGH );			// deselect the chip
}



/****************** EXAMPLE CODE ****************************************/

/* Read from or write to register from the SCP1000:
// Taken directly from example code

unsigned int readRegister(byte thisRegister, int bytesToRead ) {
  byte inByte = 0;           // incoming byte from the SPI
  unsigned int result = 0;   // result to return
  Serial.print(thisRegister, BIN);
  Serial.print("\t");
  // SCP1000 expects the register name in the upper 6 bits
  // of the byte. So shift the bits left by two bits:
  thisRegister = thisRegister << 2;
  // now combine the address and the command into one byte
  byte dataToSend = thisRegister & READ;
  Serial.println(thisRegister, BIN);
  // take the chip select low to select the device:
  digitalWrite(chipSelectPin, LOW);
  // send the device the register you want to read:
  SPI.transfer(dataToSend);
  // send a value of 0 to read the first byte returned:
  result = SPI.transfer(0x00);
  // decrement the number of bytes left to read:
  bytesToRead--;
  // if you still have another byte to read:
  if (bytesToRead > 0) {
    // shift the first byte left, then get the second byte:
    result = result << 8;
    inByte = SPI.transfer(0x00);
    // combine the byte you just got with the previous one:
    result = result | inByte;
    // decrement the number of bytes left to read:
    bytesToRead--;
  }
  // take the chip select high to de-select:
  digitalWrite(chipSelectPin, HIGH);
  // return the result:
  return(result);
}


//Sends a write command to SCP1000 
// This also directly from example code
void writeRegister(byte thisRegister, byte thisValue) {

  // SCP1000 expects the register address in the upper 6 bits
  // of the byte. So shift the bits left by two bits:
  thisRegister = thisRegister << 2;
  // now combine the register address and the command into one byte:
  byte dataToSend = thisRegister | WRITE;

  // take the chip select low to select the device:
  digitalWrite(chipSelectPin, LOW);

  SPI.transfer(dataToSend); //Send register location
  SPI.transfer(thisValue);  //Send value to record into register

  // take the chip select high to de-select:
  digitalWrite(chipSelectPin, HIGH);
}
*/ // end of their read and write commands given in example code
