/*
	SNES CONTROLLER IMPLEMENTATION
	Controller uses a shift register for button inputs
	Bits: B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R, 
		  0, 1,      2,  3,     4,  5,   6,      7,   9, 10,11,12
*/

#include <avr/io.h>
#include "timer.h"
#include "bit.h"
#include "shiftregister.h"

// Controller Ports
#define PORTSNES PORTA
#define PINSNES PINA
// Pins the snes controller is connected to
#define CLOCK_SNES 0
#define LATCH_SNES 1
#define DATA_SNES 2

// SNES controller data
unsigned short controllerData = 0; // 16 bits

void readSNES();	// Function prototype
unsigned short _SetBit(unsigned short pin, unsigned short number, unsigned short bin_value);
void daisy(unsigned short data);

// Defines Shift register 0
#define PORTSR PORTC
#define SRCLOCK PORTC

#define CLEAR 3	// LOW = clears the register. HIGH = keeps values
#define DATA_SR 0
#define LATCH 2
#define SRCLK 4
#define OE_SR 1

int main(void)
{
	DDRA = 0x03;	PORTA = 0x04;	// clock&latch is output, but the data pin is input
	DDRC = 0xFF;	PORTC = 0x00;	// port b is output (LED's for testing)
	
	// Controller set bits
	PORTSNES = SetBit(PINSNES, LATCH_SNES, 0);	// Set latch low
	PORTSNES = SetBit(PINSNES, CLOCK_SNES, 0);	// Set pin low
	
	// Timer
	TimerSet(50);	// 50 ms for controller input
	TimerOn();
	unsigned short i = 0;
	
	unsigned short oldData = 0;
    while (1) 
    {
		oldData = controllerData;
		readSNES();
		
		
		while(!TimerFlag);
		TimerFlag = 0;
		daisy(controllerData);
		
    }
}

void readSNES(){
	unsigned char i = 0;	// Loop variable
	
	controllerData = 0x00;
	// Latch new data
	PORTSNES |= 0x02;	// Latch
	PORTSNES = 0x00;
	
	// Read the buttons
	for(i = 0; i < 16; i += 1){
		PORTSNES = 0x00;	// unset clock
		controllerData |= (~PINSNES & (0x01 << DATA_SNES))? 0x01 << i : 0x00 << i;
		PORTSNES = 1 << CLOCK_SNES;	// set clock to read bit
	}
}

// Modified setbit function
unsigned short _SetBit(unsigned short pin, unsigned short number, unsigned short bin_value)
{
	return (bin_value ? pin | (0x01 << number) : pin & ~(0x01 << number));
}

void daisy(unsigned short data){
	transmit_short(&PORTSR, &SRCLOCK, data, DATA_SR, LATCH, SRCLK, CLEAR);
}
