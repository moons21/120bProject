
#include <avr/io.h>



// ----------------------------------	Shift register function	 ----------------------------------
// First Parameter: Make sure to pass in ADDRESS of the port you desire to update
// Second paramter: Address of the shift register clock, incase its different
// This allows for maximum flexibility, in case there are multiple shift registers that need to be modified

// Last four parameters are the pins the shift register is connected to
void transmit_data(volatile uint8_t *port,volatile uint8_t *srclock, unsigned char data, 
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
	// set RCLK = 1. Rising edge copies data from �Shift� register to �Storage� register
	*srclock |= 0x01 << LATCH;
	// clears all lines in preparation of a new transmission
	*port = 0x00;
}

// Last four parameters are the pins the shift register is connected to
void transmit_short(volatile uint8_t *port,volatile uint8_t *srclock, unsigned short data, 
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
	// set RCLK = 1. Rising edge copies data from �Shift� register to �Storage� register
	*srclock |= 0x01 << LATCH;
	// clears all lines in preparation of a new transmission
	*port = 0x00;
}