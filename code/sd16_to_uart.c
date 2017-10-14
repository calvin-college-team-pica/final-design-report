//*****************************************************************************
//  MSP430x471xx Demo - SD16, Single Conversion on a Single Channel Using ISR
//
//                 MSP430x471xx
//             -----------------
//         /|\|              XIN|-
//          | |                 | 32kHz
//          --|RST          XOUT|-
//            |                 |
//    Vin+ -->|A2.0+            |
//    Vin- -->|A2.0-            |
//            |                 |
//            |            VREF |---+
//            |                 |   |
//            |                 |  -+- 100nF
//            |                 |  -+-
//            |                 |   |
//            |            AVss |---+
//            |                 |
// Based on TI demo code
// Modified by Avery Sterk April 2011
//******************************************************************************
#include  <msp430x471x7.h>

unsigned int result;

void main(void)
{
  volatile unsigned int i;                  // Use volatile to prevent removal
                                            // by compiler optimization

  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
  FLL_CTL0 |= XCAP14PF;                     // Configure load caps

  do
  {
    IFG1 &= ~OFIFG;                         // Clear OSCFault flag
    for (i = 0x47FF; i > 0; i--);           // Time for flag to set
  }
  while ((IFG1 & OFIFG));                   // OSCFault flag still set?

  P2SEL |= BIT4+BIT5;                       // P2.4,5 = USCI_A0 RXD/TXD
  UCA0CTL1 |= UCSSEL_1;                     // CLK = ACLK
  UCA0BR0 = 0x03;                           // 32k/9600 - 3.41
  UCA0BR1 = 0x00;                           //
  UCA0MCTL = 0x06;                          // Modulation
  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
  IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt

  SD16CTL = SD16REFON+SD16SSEL0;            // 1.2V ref, SMCLK
  SD16CCTL2 |= SD16SNGL+SD16IE ;            // Single conv, enable interrupt
  for (i = 0; i < 0x3600; i++);             // Delay for 1.2V ref startup

  while (1)
  {
    _NOP();                                 // SET BREAKPOINT HERE    
    SD16CCTL2 |= SD16SC;                    // Set bit to start conversion                                      
    __bis_SR_register(LPM0_bits + GIE);     // Enter LPM0 w/ interrupts
  }
}

#pragma vector=SD16A_VECTOR
__interrupt void SD16AISR(void)
{
  switch (SD16IV)
  {
  case 2:                                   // SD16MEM Overflow
    break;
  case 4:                                   // SD16MEM0 IFG
    break;
  case 6:                                   // SD16MEM1 IFG     
    break;
  case 8:                                   // SD16MEM2 IFG
    result = SD16MEM2;                      // Save CH2 results (clears IFG)
//    UCA0TXBUF = (result >> 9); // bitshift result down by 9 bits, so it's at least ASCII
	UCA0TXBUF = (SD16MEM2 >> 12) + 0x41; // map top 4 bits of 16-bit measurement to 'A'-'P'.
    break;
  }

  __bic_SR_register_on_exit(LPM0_bits);        // Exit LPM0
}

