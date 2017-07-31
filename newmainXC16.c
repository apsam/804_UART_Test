/*
 * File:   newmainXC16.c
 * Author: samue
 *
 * Created on May 31, 2017, 3:05 PM
 */

//Uncomment is not using the serial console to debug
#define SERIAL_DEBUG

#include "xc.h"
//#include "p33FJ128MC804.h"
#include <PPS.h>

//Configuration Register - FOSCSEL
#pragma config  IESO    = 0     //Disable any internal to external osc switch config
#pragma config  FNOSC   = 1     //Select FRC osc with PLL at reset

//Configuration Register - FOSC
#pragma config  FCKSM       = 3 //Osc switch fail safe modes disabled
#pragma config  OSCIOFNC    = 0 //OSCO pin is now a general purpose IO pin
#pragma config  POSCMD      = 3 //Disable primary osc (HS, XT, EC)

//Configure In-Circuit Programmer, WDT
#pragma config  FWDTEN      = 0 //Watchdog Timer is disabled
#pragma config  ICS         = 3 //Program through PGEC1/PGED1 ports

//These are now defined in PPS.h
//#define PPSUnLock   __builtin_write_OSCCONL(OSCCON & 0xBF)
//#define PPSLock     __builtin_write_OSCCONL(OSCCON | 0x40)

#define PPSIn(fn, pin)		iPPSInput(IN_FN_PPS##fn, IN_PIN_PPS##pin)
#define PPSOut(fn, pin)		iPPSOutput(OUT_PIN_PPS##pin, OUT_FN_PPS##fn)
    

#define DELAY_105uS asm volatile ("REPEAT, #4201"); Nop();	//105 uS delay
/*
void __attribute__((__interrupt__))_U1TXInterrupt(void){
	
}
*/

void __attribute__((interrupt, no_auto_psv))_U1TXInterrupt(void){
	IFS0bits.U1TXIF = 0;	//Clear TX interrupt flag
	//U1TXREG = 'X';			//Transmit one character
	LATAbits.LATA3 = 1;
}

/*
 *
 *	FP = Clock source for all the peripherals
 *  FCY = Clock source for CPU
 *
 *
 */
void clockSetup(){
	//Clock source definition - A single FRC internal clock
    OSCTUNbits.TUN      = 0;    //Select FRC = 7.37 MHz
    
    //CLKDIV register
    CLKDIVbits.FRCDIV   = 0;    //FRCDIVN = FRC/1 = 7.37MHz = FIN
    CLKDIVbits.PLLPRE   = 0;    //FIN/2 = 7.37/2 = 3.685MHz
    PLLFBDbits.PLLDIV   = 38;   //FVCO = 3.68*40 = 147.4MHz
    CLKDIVbits.PLLPOST  = 3;    //FRCPLL = 147.4/8 = 18.42MHz = FOSC and 
                                    //FP = 18.42/2 = 9.21MHz
	//The following controls the speed of the program loop....
		//0 is faster than 3
	//CLKDIVbits.PLLPOST  = 0;    //FRCPLL = 147.4/8 = 18.42MHz = FOSC and 
    CLKDIVbits.DOZE     = 0;    //FCY = FP/1 = 9.21MHz
    CLKDIVbits.DOZEN    = 1;    //DOZE<2:0> will decide the ratio between FP and FCY
    
    //ACLKCON Register = Auxiliary clock control for DAC
    ACLKCONbits.AOSCMD	 = 0;    //Aux clock is disabled
	ACLKCONbits.SELACLK  = 0;    //Select PLL Output
    ACLKCONbits.APSTSCLR = 7;    //FA = FOSC/1 = 18.42 MHz
}

/*
 *	Peripheral Pin Select
 *		Use PPS to give special functionality to certain pins
 *	For UART: See Table 11.1 & 11.2 on page 167
 */
void pinSetup(){
	PPSUnLock;
	//Outputs  
	PPSOut(_U1TX, _RP12);		//Pin10:	RP12 is U1TX
	PPSOut(_U1RTS, _RP13);		//Pin11:	RP13 is U1RTS (Read To Send)
		
	//Inputs
	PPSIn(_U1RX, _RP14);		//Pin14:	RP14 is U1RX
	PPSIn(_U1CTS, _RP15);		//Pin15:	RP15 is U1CTS	(Clear To Send)
	PPSLock;
}

void ioSetup(){
    //AD1PCFGL = 0xFFFF;          //AN0-AN8 are now digital 
	//Inputs
	/*
	//Only Pin 19 needs to be converted to digital as it's default type analog
		//Pins 30 and 31 are not analog.
	AD1PCFGLbits.PCFG0 = 1;		//Pin19:	AN0 is now digital type
	AD1PCFGLbits.PCFG1 = 1;		//Pin20:	AN1 is now digital type
    TRISAbits.TRISA0=1;			//Pin19:	RA0 as digital input
	TRISAbits.TRISA1=1;			//Pin20:	RA1 as digital input
	*/
	
	//Outputs 
	TRISAbits.TRISA2=0;			//Pin30:	RA2 as digital output
    TRISAbits.TRISA3=0;			//Pin31:	RA3 as digital output
	
	//UART Pin setup?
	//	UUART TX and RX pins are configured by the UART control registers
}

/*
 *
 * 
 *	Assign a value to the Baud Rate Register (UxBRG)
 */
void baudSetup(int gen){
	//FP = 4MHz
	//Desired baud = 9600
	//Result = 25
	/*
	 * Equation:
	 *	UxBRG = ((FP/Baud)/(16)) - 1
	 */
	U1BRG = gen;
}

/*
 *
 * Configure the UART control registers.
 */
void uartSetup(){
	//Mode Register
	U1MODEbits.UARTEN = 0;	//Disable UART
	U1MODEbits.URXINV = 1;	//U1RX idle state is 0
	U1MODEbits.BRGH = 0;	//16 clock bits per period (Standard)
	U1MODEbits.PDSEL = 0;	//8 bit data, no parity
	U1MODEbits.STSEL = 0;	//1 stop bit
	U1MODEbits.ABAUD = 0;	//Auto-baud disabled
	
	//Set the baud value
	baudSetup(25);
	
	//Status Register
	U1STAbits.UTXISEL0 = 0;	//Interrupt after one TX char is transmitted
	U1STAbits.UTXISEL1 = 0;	//
	
	IEC0bits.U1TXIE	= 1;	//UTX Interrupt triggers once all data has been sent
	
	U1MODEbits.UARTEN = 1;	//Enable UART
	U1STAbits.UTXEN = 1;	//Enable UART TX
}

int main(void) {
	int i = 0;
	clockSetup();
	pinSetup();
	ioSetup();
	#ifdef SERIAL_DEBUG
	uartSetup();
	#endif
	
	//Turn off LED
	LATAbits.LATA2 = 0;		//For checking UART transmit
	LATAbits.LATA3 = 0;		//For checking UART interrupt
	
	//Delay for 105 uS before sending the first char
	DELAY_105uS;
	U1TXREG = 'D';		//Send out one character
	
	while(1){
		LATAbits.LATA2 = 1;
		for(i = 0; i < 50; i++){
			DELAY_105uS;
		}
		U1TXREG = 'A';		//Send out one character
		LATAbits.LATA2 = 0;
		LATAbits.LATA3 = 0;
		for(i = 0; i < 500; i++){
			DELAY_105uS;
		}
	}
}
