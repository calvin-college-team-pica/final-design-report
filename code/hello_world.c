/*************************************************************
 * MSP430F47197 Hello World Demo
 * Description: Toggle LED located at P5.1 on MSP430 Dev Board
 * Hardware Setup: See diagram below, requires only JTAG clk.
 *
 *                  MSP430x471x7
 *             -----------------
 *         /|\|                 |
 *          | |                 |
 *          --|RST              |
 *            |                 |
 *            |             P5.1|-->LED
 * Author: Kendrick Wiersma -- kendrick.g.wiersma@gmail.com
 */

#include "msp430x471x7.h"

void main(void) {
  volatile unsigned int i;
  
  WDTCTL = WDTPW + WDTHOLD; // Ask the watchdog timer to please hold for us
  P5DIR |= BIT1;            // Set P5.1 to be an output
  
  while(1) {
    P5OUT ^= BIT1;
    for(i=50000; i > 0; i--); //delay for a bit
  }
}
