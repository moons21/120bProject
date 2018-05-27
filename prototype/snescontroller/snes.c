

#include <avr/io.h>
#include "timer.h"
#include "bit.h"
#include "shiftregister.h"
#include "snes.h"

// -----------------------------	Controller Defines	-----------------------------	
// Controller Ports
#define PORTSNES PORTA
#define PINSNES PINA
// Pins the snes controller is connected to
#define CLOCK_SNES 0
#define LATCH_SNES 1
#define DATA_SNES 2
// -----------------------------	Defines Shift register 0	-----------------------------	
#define PORTSR PORTC
#define SRCLOCK PORTC
#define CLEAR 3	// LOW = clears the register. HIGH = keeps values
#define DATA_SR 0
#define LATCH 2
#define SRCLK 4
#define OE_SR 1

// -----------------------------	Prototypes	-----------------------------	

unsigned short _readSNES();	// Helper function for snes controller header
void daisy(unsigned short data);	// controller output via register daisy chain

int main(void)
{
	DDRA = 0x03;	PORTA = 0x04;	// clock&latch is output, but the data pin is input
	DDRC = 0xFF;	PORTC = 0x00;	// port b is output (LED's for testing)
	
	// Controller set bits	(MAKE SURE TO INCLUDE IN PROJECT!!!!!!!)
	PORTSNES = SetBit(PINSNES, LATCH_SNES, 0);	// Set latch low
	PORTSNES = SetBit(PINSNES, CLOCK_SNES, 0);	// Set pin low
	
	// Timer
	TimerSet(50);	// 50 ms for controller input
	TimerOn();
	
    while (1) 
    {
		while(!TimerFlag);
		TimerFlag = 0;
		daisy(_readSNES());
    }
}

unsigned short _readSNES(){
	return readSNES(&PORTSNES,&PINSNES,
	CLOCK_SNES, LATCH_SNES, DATA_SNES);
}

void daisy(unsigned short data){
	transmit_short(&PORTSR, &SRCLOCK, data, DATA_SR, LATCH, SRCLK, CLEAR);
}
