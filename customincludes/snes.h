

/*
	SNES CONTROLLER IMPLEMENTATION
	This function gets controller input data from an snes controller
	Parameter meanings: 
		port - the port the controller is connected to
		pin - the pin the controller is connected to (NEEDED TO READ THE DATA)
		
		clock - cycles through the bits in the register
		latch - latches whatever data values are currently in the data registers
		data - input data from the controller
		
		
		OUTPUT: 16 bit unsigned short
		Controller uses a shift register for button inputs
		Bits: B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R,
		0, 1,      2,  3,     4,  5,   6,      7,   9, 10,11,12
		
*/






#include <avr/io.h>



unsigned short readSNES(volatile uint8_t *port,volatile uint8_t *pin, 
unsigned short CLOCK_SNES, unsigned char LATCH_SNES, unsigned char DATA_SNES){
	
	unsigned char i = 0;	// Loop variable
	unsigned short controller = 0;	// Holds controller data
	
	// Latch new data
	*port |= 0x02;	// Latch
	*port = 0x00;
	
	// Read the buttons
	for(i = 0; i < 16; i += 1){
		*port = 0x00;	// unset clock
		controller |= (~*pin & (0x01 << DATA_SNES))? 0x01 << i : 0x00 << i;
		*port = 1 << CLOCK_SNES;	// set clock to read bit
	}
	return controller;
}