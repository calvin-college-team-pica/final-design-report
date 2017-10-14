/* emeter_rc.c
 * 
 * A program to read from the SD16, convert to real-world units, then
 *   output to the UART and the LCD
 *
 * Written 26 April 2011 thru 7 May 2011 by Avery Sterk
 * for Calvin College Senior Design Team 01
 * 
 * Hardware: Texas Instruments MSP430F47197, with
 *   * SynchroSystems LCD screen on S00..S39 and COM0..3
 *   * 10 uF capactor between LCDCAP and ground
 *   * Abracon ABLS 16.000 MHz oscillator as Xtal 2, ~6pF shunt caps
 *   * 32,768 Hz clock as Xtal 1, 12pF shunt caps
 *   * MAX233A RS232 UART on P2.4-5
 *   * 0.1uF capcitor between pins VREF and AVss
 *   * Current-sense network inputs on ADC channels 0,1,2
 *   * Voltage network input on ADC channels 3,4,5
 *****************************************************************************/
 
 // how many seconds to wait before the backlight goes to sleep
 #define BACKLIGHT_AWAKE 5
 // declare how many meaurements to take before stopping
 // as we use float, this can be large
 #define COUNT_TO 120
 #define COUNT_TO_FLOAT 120.0;
 /** begin scale factors ******/
 // irms: (measured_i_rms * 0.6V)/(measured_v_rms * 0x8000)
 // here, 360mA *600mV / (6.9mVRMS * 0x8000) = 0.000955 A / unit
 #define irms_scale_factor 0.000955
 // vrms: (line_VRMS * 0.6V )/(net_VRMS * 0x8000)
 // here, (120 V * 0.6V)/(0.185V * 0x8000) = 0.01188 V / unit
 #define vrms_scale_factor 0.01188
 /* power scale factor: compute via product of the other two */
 // here, 0.0955mA * 0.01188 V * A / 1000mA = 0.0000011345
 #define power_scale_factor 0.000011345
 /**** end scale factors ******/
 // number of hours per ACLK tick
 #define energy_factor 0.000000008477 // 2^(-15)/3600 Hz
 
 
 #include "msp430x471x7.h"
 #include "lcd.h"
 #include <math.h>
 // remember, 16 bit ints and 32-bit longs
 /* 32-bit floats */
 volatile double p1vsum, p1isum, p1psum; // can be changed in an ISR
 volatile double p2vsum, p2isum, p2psum;
 volatile double p3vsum, p3isum, p3psum;
 volatile unsigned int count; // 16-bit int
 volatile float tmpf; // just a placeholder
 volatile long tmpl; // just another placeholder
 volatile short uptime_hours, uptime_minutes, uptime_seconds, backlight_state;
 volatile char uptime_separator;
 
 void print_units_to_lcd(double value,char units);
 void double_fmt(double value, char* buf, int length);
 void print_uart(char* str); 
 
 void main(void) {
 	volatile unsigned int i;				// don't compile away, may change
 	volatile unsigned char byte;
	float mean;
 	double p1energy, p2energy, p3energy, total_energy;
 	double rms;
 	unsigned int time;
 	char display_mode = 'P'; // statistic to display
 	char buffer[256]; // buffer, you'd better not be able to overrun this.
 	double ptotal;
 	
 	
 	WDTCTL = WDTPW | WDTHOLD;				// stop the watchdog
 	FLL_CTL0 |= XCAP14PF;					// configure capacitors, TI value
 	FLL_CTL1 |= SELS;						// select 16Mhz crystal for SMCLK
 	
 	/* wait for crystal to stabilize */
 	do {
 		IFG1 &= ~OFIFG;				// clear Oscillator fault flag
 		for (i = 0x47FF; i > 0; i--);		// wait for a while to stabilize
 	}
 	while ((IFG1 & OFIFG)); // while the oscillator fault keeps flagging
 	
 	/* initialize UART */
 	P2SEL |= BIT4 + BIT5;					// P2.4 and P2.5 are UART pins
 	UCA0CTL1 |= UCSSEL_2;					// Use SMCLK for UART timing
 	UCA0BR0 = 138;							// UART tick every 138 SMCLK ticks
 	UCA0BR1 = 0x00;							// 16MHz / 15200Baud ~= 138
	UCA0MCTL = UCBRS_7;						// modulation for 115200 baud
	
	/* initialize Synchro LCD */
	// control: ACLK/64 speed = 32768/64 = 512Hz, a 4-Mux display
	LCDACTL = LCDFREQ_32 | LCD4MUX | LCDSON; // TODO: Test using the 64 clock divider instead of 32
	LCDAVCTL0 = LCDCPEN;   // enable charge pump
	LCDAVCTL1 = VLCD_11;   // set output to 3.02V
	LCDAPCTL0 = 0xFF; // use all pins S00-S31
	LCDAPCTL1 = LCDS32 | LCDS36; // and S32-D39
	// Port1: LED output
    P1OUT = BIT2 + BIT4 + BIT5 + BIT6; // Signal LEDs off, backlight LED on
    P1DIR = BIT0 + BIT2 + BIT4 + BIT5 + BIT6;
    P1IES = 0x00;
    P1IE  = 0x00;
    P1SEL = 0x00;
	// Port4: LCD peripheral
    P4OUT = 0x00;
    P4DIR = 0x00;
    P4SEL = 0xFF; // port 4, all pins are controlled by the LCD_A controller 
	// Port5: LCD peripheral
    P5SEL = BIT1 + BIT2 + BIT3 + BIT4; // all of these belong to the LCD_A device
    
	/* initialize SD16 ADC */
	SD16CTL = SD16REFON + SD16SSEL0 + SD16DIV_3;		// 1.2V reference, use SMCLK /3
		/* configure SD16's to use single conversion, 2's-complement form, and group */
	SD16CCTL0 |= SD16SNGL + SD16GRP + SD16DF; // group to next channel (CH 1)
	SD16CCTL1 |= SD16SNGL + SD16GRP + SD16DF; // group to next channel (CH 2)
	SD16CCTL2 |= SD16SNGL + SD16GRP + SD16DF; // group to next channel (CH 3)
	SD16CCTL3 |= SD16SNGL + SD16GRP + SD16DF; // group to next channel (CH 4)
//	SD16CCTL3 |= SD16SNGL + SD16IE + SD16DF; // interrupt when done
	SD16CCTL4 |= SD16SNGL + SD16GRP + SD16DF; // group to next channel (CH 5)
	SD16CCTL5 |= SD16SNGL + SD16IE + SD16DF; // interrupt when done
	for (i=0x3600; i > 0; i--); 			// wait for reference to settle 
	
	/* initialize timer */
	uptime_minutes = 0;
	uptime_hours = 0;
	uptime_seconds = 0;
	uptime_separator = ':';
	TACTL = TASSEL_1; // use ACLK, which is 32kHz.
	TBCCTL0 = CCIE;                           // CCR0 interrupt enabled
  	TBCCR0 = 0x4000;
  	TBCTL = TBSSEL_1 + MC_2 + ID_1;           // ACLK, continuous mode, / 2
	
	/* initialize variables */
	p1energy = 0; // never explicitly set elsewhere.
	p2energy = 0;
	p3energy = 0;
	
	/* turn LCD on */
	backlight_state = BACKLIGHT_AWAKE;
	LcdOn();
	LcdClear();
	LcdShowStr("  00 W", LCD_PRIM_AREA, 0, STEADY);
	LcdShowStr("0000", LCD_SEC_AREA, 0, STEADY);
	LcdShowChar(uptime_separator, LCD_SEC_PUNCT_AREA, 0, STEADY);
	LcdShowChar(display_mode, LCD_MODE_AREA, 0, STEADY);
	
	/* start UART */
	IE2 |= UCA0RXIE;						// Enable USCI_A0 RX interrupt
	UCA0CTL1 &= ~UCSWRST;					// UART state machine out of reset
	
	/********** MAIN BEHAVIOR LOOP ******************/
	/************************************************/
	while( UCA0RXBUF != 'q' ) { // until we receive 'q' over RS232
		/* init variables */
		count = 0;
		p1psum = p1vsum = p1isum = 0;
		p2psum = p2vsum = p2isum = 0;
		p3psum = p3vsum = p3isum = 0;
		ptotal = 0;
		
		/* change display modes if the button is pressed */
		i = 0;
		while(P2IN & BIT3) // debouncer for the button press
			if (i < 10000)
				i++;
			else break; // done with this loop
		if ( (backlight_state > 0) && (i > 8000) ) { // yes, you can release the button faster than it will wait.
			backlight_state = BACKLIGHT_AWAKE;
			LcdClear();
			switch (display_mode) {
			case 'P':
				display_mode = 'E';
				LcdShowStr("Wh", LCD_PRIM_AREA, 4, STEADY);
				break;
			case 'E':
				display_mode = 'V';
				LcdShowStr(" V", LCD_PRIM_AREA, 4, STEADY);
				break;
			case 'V':
				display_mode = 'I';
				LcdShowStr("mA", LCD_PRIM_AREA, 4, STEADY);
				break;
			case 'I': default:
				display_mode = 'P';
				LcdShowStr(" W", LCD_PRIM_AREA, 4, STEADY);
				break;
			} // end switch
			LcdShowChar(display_mode, LCD_MODE_AREA, 0, STEADY);
		} else if (i > 8000) {// wake the backlight
			backlight_state = BACKLIGHT_AWAKE; // reset the timer
			P1OUT |= BIT2; // turn it back on	
		}
		
		/* Turn Timer On */
		TACTL |= MC_2;

		/* activate peripherals */
		SD16CCTL5 |= SD16SC;				// start SD16A conversion	
		__bis_SR_register(LPM0_bits + GIE); // light sleep, with interrupts
		// SD16A  ISR will collect data into the "sum" variables
		// and 	wake us back up where we went to sleep.
		
		/* wakeup routine */
		P1OUT += 0x70; // adds the blinky lights
		
		// output power to uart
		mean = p1psum * power_scale_factor / COUNT_TO_FLOAT;
		double_fmt(mean, buffer, 4);
		print_uart(buffer);
		ptotal += mean;
		// compute channel 1 energy
		TACTL |= MC_0; // turn off clock
		time = TAR; // read elapsed time
		p1energy += energy_factor * (mean * time); // compute energy
		TACTL &= TACLR; // clear the timer
				
		mean = p2psum * power_scale_factor / COUNT_TO_FLOAT;
		p2energy += energy_factor * (mean * time);
		while(!(IFG2 & UCA0TXIFG));
		UCA0TXBUF = ' '; // space to separate
		double_fmt(mean, buffer, 4);
		print_uart(buffer);
		ptotal += mean;
		
		
		mean = p3psum * power_scale_factor / COUNT_TO_FLOAT;
		p3energy += energy_factor * (mean * time);
		while(!(IFG2 & UCA0TXIFG));
		UCA0TXBUF = ' '; // space to separate
		double_fmt(mean, buffer, 4);
		print_uart(buffer);
		ptotal += mean;
		
		total_energy = p1energy + p2energy + p3energy;
		while(!(IFG2 & UCA0TXIFG));
		UCA0TXBUF = ' '; // space to separate
		double_fmt(ptotal, buffer, 4);
		print_uart(buffer);
		
		if ( display_mode == 'V' ) {
			mean = p1vsum / COUNT_TO_FLOAT;
			rms = vrms_scale_factor * sqrt(mean); // take the square root
			print_units_to_lcd(rms,'V');
		} else if (display_mode == 'I' ) {
			mean = p1isum / COUNT_TO_FLOAT;
			rms = irms_scale_factor * sqrt(mean);
			print_units_to_lcd(rms,'A');
		} else if (display_mode == 'E' ) {
			print_units_to_lcd(total_energy,'E');
		} else { // assume power
			print_units_to_lcd(ptotal,'W');
		}
		
		/* end this line of text */
		while(!(IFG2 & UCA0TXIFG));
		UCA0TXBUF = 0x0A;					// "newline"
		while(!(IFG2 & UCA0TXIFG));
		UCA0TXBUF = 0x0D;					// "carriage return"
		_NOP(); // breakpoint here if you need it
	} // end while condition; shut things down
	LcdClear();
	LcdOff();
	P1OUT = 0x70; // turn off the signal LEDs
	while(!(IFG2 & UCA0TXIFG));
	__bis_SR_register(LPM3_bits); // go into a deep sleep, no interruptions
} // end void main()

/* format a measurment with the given units for the LCD screen
 * Receives: value, a 32-bit float; units, a character for the units
 * Effect: determines the range of value, then outputs three digits of 
 *         the value in a way fit for the LCD screen:
 *         with XYZ digits, p an SI prefix, and u the unit character,
 *         either: X.YZ pu
 *         or:     XYZ pu
 *         whichever fits the 10^3n better.   */
void print_units_to_lcd(double value, char units) {
	long long_prefix = 0;
	long long_suffix = 0;
	int prefix_order = 0;
	int three_digits = 0;
	char decimal = ' '; // space means no decimal, '.' means decimal.
	/* negative sign */
	if (value < 0 ) { // are we negative? 
		LcdShowChar('-',LCD_PRIM_AREA,0,STEADY); // if so, show the negative sign
		value = - value; // take the absolute value
	} else LcdShowChar(' ',LCD_PRIM_AREA,0,STEADY); // if not, clear it

	/* figure out the output using long values */
	long_prefix = value; // implicit ftoi
	long_suffix = (value - long_prefix) * 1000000; // should give us 6 decimal places
	// figure out which prefix to use
	if (long_prefix == 0) { // everything is in decimal places
		if ( long_suffix >= 1000 ) { // >= 1 milli-unit
			prefix_order = -1;
			if (long_suffix >= 10000) // >= 10 milli-units, cannot use decimal
				three_digits = long_suffix / 1000;
			else {
				three_digits = long_suffix / 10;
				decimal = '.';
			}
		} else {
			prefix_order = -2;
			three_digits = long_suffix; // < 1000, so we're good
		}
	} else { // we have something > 1 unit
		if (long_prefix < 10) { // mix units and decimals
			three_digits = 100*long_prefix + (long_suffix / 10000); // 1 lead, 2 follow digits
			decimal = '.';
		} else if (long_prefix < 1000)
			three_digits = long_prefix;
		else {
			 prefix_order = 1;
			 if (long_prefix < 10000) { // use decimal point with kilo-units
			 	decimal = '.';
			 	three_digits = long_prefix / 10;
			 } else // no decimal point with kilo-units
			 	three_digits = long_prefix / 1000;
		}
	}
	LcdShowChar(units,LCD_PRIM_AREA,5,STEADY); // show units
	LcdShowChar(decimal,LCD_PRIM_PUNCT_AREA,0,STEADY); // decimal
	switch (prefix_order) { // write prefix
	case 0: LcdShowChar(' ',LCD_PRIM_AREA,4,STEADY); break;
	case 1: LcdShowChar('k',LCD_PRIM_AREA,4,STEADY); break;
	case -1: LcdShowChar('m',LCD_PRIM_AREA,4,STEADY); break;
	case -2: LcdShowChar('u',LCD_PRIM_AREA,4,STEADY); break;
	default: LcdShowChar('?',LCD_PRIM_AREA,4,STEADY); break;
	} // end switch
	if (three_digits >= 1000) { // something went wrong
		LcdShowStr("???",LCD_PRIM_AREA,1,STEADY);
	}
	if (three_digits >= 100) {
		LcdShowChar(0x30 + (three_digits / 100), LCD_PRIM_AREA,1,STEADY);
		three_digits %= 100;
		LcdShowChar(0x30 + (three_digits / 10), LCD_PRIM_AREA,2,STEADY);
		LcdShowChar(0x30 + (three_digits % 10), LCD_PRIM_AREA,3,STEADY);
	} else { // < 100
		LcdShowChar(' ',LCD_PRIM_AREA,1,STEADY);
		if (three_digits >= 10) // output the 10's place
			LcdShowChar(0x30 + (three_digits / 10), LCD_PRIM_AREA,2,STEADY);
		else
			LcdShowChar(' ', LCD_PRIM_AREA,2,STEADY);
		LcdShowChar(0x30 + (three_digits % 10), LCD_PRIM_AREA,3,STEADY);
	}
	
}

/* double_fmt gives a c-string representation of a double */
void double_fmt(double value, char* buf, int want_places) {
	int place = 0;
	long long_value = 0;
	long tens_place = 1;
	int num_pre_decimal = 0;
	char byte = '\0';
	if ( value < 0 ) {
		buf[place++] = '-';
		value = - value; // invert the value
	}
	long_value = value; // implicit ftoi;
	// figure out how many places it is
	while ( long_value >= tens_place ) {
		tens_place *= 10;
		num_pre_decimal++;
	}
	while ( num_pre_decimal >= 0 ) {
		byte = 0x30 + ( long_value / tens_place );
		buf[place++] = byte;
		long_value %= tens_place;
		tens_place /= 10;
		num_pre_decimal--;
		want_places--;	
	}
	if ( want_places > 0 ) {
		buf[place++] = '.';
	}
	long_value = 1000000 * value;
	long_value %= 1000000; // up to six places after the decimal!
	tens_place = 100000;
	if ( want_places > 6 ) {
		want_places = 6; // nothing more than six more places
	}
	while ( want_places > 0 ) {
		byte = 0x30 + (long_value / tens_place);
		buf[place++] = byte;
		long_value %= tens_place;
		tens_place /= 10;
		want_places--;
	}
	buf[place] = '\0'; // null-terminate 
}

void print_uart(char * str) {
	while (*str != '\0') { // until we reach the null character,	
		while(!(IFG2 & UCA0TXIFG)); // wait for the transmission to finish
		UCA0TXBUF = *str; // send the current character
		*str = '\0'; // clear this character
		str++; // next character
	}
	while(!(IFG2 & UCA0TXIFG)); // wait for the transmission to finish
}

/* SD16 ADC interrupt service routine */
#pragma vector=SD16A_VECTOR
__interrupt void SD16AISR(void) { // service the SD16A interrupt
	// just in case we don't want to waste time on the stack,
        // use register variables
	register int current;
	register int voltage;
	switch (SD16IV) { // who caused the interrupt?
		default: break;
		case 2: break; // SD16AMEM overflow, change your input?
		case 4: case 6: case 8: case 10: case 12: case 14:
		// SD16 done converting, save the value
			SD16CCTL5 &= ~SD16SC; // shut off the SD16
			/**** phase 1 ****/
			current = SD16MEM3; // implicitly clears any interrupt flag
			p1isum += (long) current * current; // square, change to float, add to sum
			voltage = SD16MEM0;
			p1vsum += (long) voltage * voltage;
			p1psum += (long) voltage * current;
			/**** phase 2 ****/
			current = SD16MEM4;
			p2isum += (long) current * current;
			voltage = SD16MEM1;
			p2vsum += (long) voltage * voltage;
			p2psum += (long) voltage * current;
			/**** phase 3 ****/
			current = SD16MEM5;
			p3isum += (long) current * current;
			voltage = SD16MEM2;
			p3vsum += (long) voltage * voltage;
			p3psum += (long) voltage * current;
			/* add to the count, ensure we cleared the interrupt */
			count ++; // increment count
			SD16CCTL5 &= ~SD16IFG; // hang the interrupt manually
			break;
	}
	if ( count >= COUNT_TO ) {
		__bic_SR_register_on_exit(LPM0_bits); // wake the processor from sleep
	} else {
		SD16CCTL5 |= SD16SC; // turn on the SD16
	}
} // end SD16A ISR

/* Timer B ISR */
#pragma vector=TIMERB0_VECTOR
__interrupt void Timer_B (void)
{
	int tmp;
	uptime_seconds++;
	if ( uptime_seconds == 60) {
		uptime_seconds = 0;
		uptime_minutes++;
	}
	if (uptime_minutes == 60) {
		uptime_minutes = 0;
		uptime_hours++;
	}
	uptime_hours %= 24; // if 24 or more, reduce to less than 24
	TBCCR0 += 0x4000;
	uptime_separator ^= 0x1A; // ':' to ' ' and vice-versa
	LcdShowChar(uptime_separator,LCD_SEC_PUNCT_AREA,0,STEADY);
	
	LcdShowChar(0x30 + (uptime_minutes % 10),LCD_SEC_AREA,3,STEADY);
	LcdShowChar(0x30 + (uptime_minutes / 10),LCD_SEC_AREA,2,STEADY);
	LcdShowChar(0x30 + (uptime_hours % 10),LCD_SEC_AREA,1,STEADY);
	LcdShowChar(0x30 + (uptime_hours / 10),LCD_SEC_AREA,0,STEADY);
	/* dial settings */
	tmp = (2 << (uptime_seconds / 5)) - 1; // add a hand to the dial every 5 seconds
	LcdShowSegs(tmp, LCD_DIAL_AREA, 0, STEADY);
	
	if (backlight_state > 0) 
		backlight_state--;
	if (backlight_state == 0) // if it's now "asleep"
		P1OUT &= ~BIT2; // turn off the LCD backlight
}
