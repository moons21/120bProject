
#include <avr/io.h>



// Important note: This function does NOT implement the output enable pin of the shift register

// To see value on shift register: set OE pin to low. Otherwise, set it to high.

/*
	This function outputs data onto an 8 bit shift register
	Parameter meanings:
		port - the port the shift register is connected to
		srclock - the port the srclock is connected to
		data - the value that is being pushed onto the registers
		
		other parameters are pins to something:
		DATA_SR - Pin on the microcontroller associated with shift register data pin
		LATCH - Pin on the microcontroller associated with shift register LATCH pin
		SRCLK - Pin on the microcontroller associated with shift register SRCLK
		CLEAR - Pin on the microcontroller associated with shift register CLEAR pin
*/
void transmit_data(volatile uint8_t *port,unsigned char data, 
unsigned char DATA_SR, unsigned char LATCH, unsigned char SRCLK, unsigned char CLEAR) {
	unsigned char i;
	for (i = 0; i < 8 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		*port = 0x01 << CLEAR;
		// set SER = next bit of data to be sent.
		//PORTSR |= ((data >> i) & (0x01 << DATA_SR));
		*port |= ((data >> i) & 0x01)? (0x01 << DATA_SR) : 0;
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		*port |= 0x01 << SRCLK;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	*port |= 0x01 << LATCH;
	// clears all lines in preparation of a new transmission
	*port = 0x00;
}

// Last four parameters are the pins the shift register is connected to
void transmit_short(volatile uint8_t *port, unsigned short data, 
unsigned char DATA_SR, unsigned char LATCH, unsigned char SRCLK, unsigned char CLEAR) {
	unsigned char i;
	for (i = 0; i < 16 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		*port = 0x01 << CLEAR;
		// set SER = next bit of data to be sent.
		//PORTSR |= ((data >> i) & (0x01 << DATA_SR));
		*port |= ((data >> i) & 0x01)? (0x01 << DATA_SR) : 0;
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		*port |= 0x01 << SRCLK;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	*port |= 0x01 << LATCH;
	// clears all lines in preparation of a new transmission
	*port = 0x00;
}