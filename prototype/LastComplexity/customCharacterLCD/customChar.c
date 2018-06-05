/*
 * LastComplexity.c
 *
 * Created: 6/3/2018 10:42:25 PM
 * Author : moons XPS
 */ 

#include <avr/io.h>
#include <avr/eeprom.h>		// Allows writing to controller EEPROM
#include "timer.h"
#include "shiftregister.h"
#include "snes.h"
#include "io.c"

// -----------------------------	Defines Shift register 0	-----------------------------
#define PORTSR PORTC
#define SRCLKPORT PORTC
#define C 3	// LOW = clears the register. HIGH = keeps values
#define D 0
#define L 1
#define S 2

// -----------------------------	Controller Defines	-----------------------------
//Bits: B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R,
//		0, 1,      2,  3,     4,  5,   6,      7,   9, 10,11,12
// Controller Ports
#define PORTSNES PORTA
#define PINSNES PINA
// Pins the snes controller is connected to
#define CLOCK_SNES 2
#define LATCH_SNES 1
#define DATA_SNES 0

// Controller Buttons
#define SNES_B 0x01<<0 
#define SNES_Y 0x01<<1 
#define SNES_Select 0x01<<2 
#define SNES_Start 0x01<<3 
#define SNES_Up 0x01<< 4 
#define SNES_Down 0x01<<5 
#define SNES_Left 0x01<<6 
#define SNES_Right 0x01<<7 
#define SNES_A 0x01<<8 
#define SNES_X 0x01<<9 
#define SNES_L 0x01<<10 
#define SNES_R 0x01<<11


// -----------------------------	Prototypes	-----------------------------
unsigned short _readSNES();	// Helper function for snes controller header
void daisy(unsigned short data);	// controller output via register daisy chain
unsigned long int findGCD(unsigned long int a, unsigned long int b);
void displayHighscore(unsigned long score, unsigned char column, unsigned char length);

void createChar(uint8_t location, uint8_t charmap[]) {
	location &= 0x7; // we only have 8 locations 0-7
	
	LCD_WriteCommand(0x40 + location*8);
	for (unsigned char i=0; i<8; i+=1) {
		LCD_WriteData(charmap[i]);
	}
}

// -----------------------------	Shared Variables	-----------------------------
unsigned char customChar_musicNote[8] = {
	0b00001,
	0b00011,
	0b00101,
	0b01001,
	0b01001,
	0b01011,
	0b11011,
	0b11000
};

//---------------------------------------Task scheduler data structure---------------------------------------
// Struct for Tasks represent a running process in our simple real-time operating system.
typedef struct _task {
	/*Tasks should have members that include: state, period,
		a measurement of elapsed time, and a function pointer.*/
	signed char state; //Task's current state
	unsigned long int period; //Task period
	unsigned long int elapsedTime; //Time elapsed since last task tick
	int (*TickFct)(int); //Task tick function
} task;


int main(void)
{
	DDRA = 0xfe;	PORTA = 0x01;	// clock&latch is output, but the data pin is input
    DDRC = 0xff; PORTC = 0x00;	// Port c is the output register
	DDRD = 0xff; PORTD = 0xff;
	
	//LCD Stuff
	LCD_init();
	
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 50;
	unsigned long int SMTick2_calc = SMTick1_calc;
	
	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SMTick1_calc, SMTick2_calc);
	
	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;
	
	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;
	unsigned long int SMTick2_period = SMTick2_calc/GCD;

	//Declare an array of tasks
	static task task1, task2;
	task *tasks[] = { &task1, &task2};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	
	// CUSTROM CHARACTER
	createChar(1,customChar_musicNote);
	
	// Set the timer and turn it on
	//TimerSet(GCD);
	TimerSet(500);
	TimerOn();
	LCD_Cursor(1);
	LCD_WriteData(1);
	unsigned short i = 121; // Scheduler for-loop iterator
    while(1) {
		LCD_Cursor(1);
		LCD_WriteData(0);
		i = (i == 255)? 0 : i + 1;
	   /* // Scheduler code
	    for ( i = 0; i < numTasks; i++ ) {
		    // Task is ready to tick
		    if ( tasks[i]->elapsedTime == tasks[i]->period ) {
			    // Setting next state for task
			    tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
			    // Reset the elapsed time for next tick.
			    tasks[i]->elapsedTime = 0;
		    }
		    tasks[i]->elapsedTime += 1;
	    }
	    while(!TimerFlag);
	    TimerFlag = 0;
    }
	*/
	while(!TimerFlag);
	TimerFlag = 0;
	}
}

unsigned short _readSNES(){
	return readSNES(&PORTSNES,&PINSNES,
	CLOCK_SNES, LATCH_SNES, DATA_SNES);
}
void daisy(unsigned short data){
	transmit_short(&PORTSR, data, D, L, S, C);
}

//--------Find GCD function --------------------------------------------------
unsigned long int findGCD(unsigned long int a, unsigned long int b)
{
	unsigned long int c;
	while(1){
		c = a%b;
		if(c==0){return b;}
		a = b;
		b = c;
	}
	return 0;
}
//--------End find GCD function ----------------------------------------------

//--------HighScore function --------------------------------------------------
void displayHighscore(unsigned long score, unsigned char column, unsigned char length)	// Length is how many rows past the initial column to write the data
{
	 unsigned char c = column;
	 unsigned char sigfig = 0;	
	 unsigned char display[10];		// max value of long can be 10 digits long
	 
	 length = (length > 10)? 10 : length;	// sets limit to how long the length can be
	 
	 for (signed char i = 0; i < 10; i += 1){	// Initialize values
		 display[i] = 0;
	 }
	 
	 while(score){	// puts the score data into the array
		 display[sigfig] = score % 10;
		 score /= 10;
		 sigfig += 1;
	 }
  	 for (signed char i = length - 1; i >= 0; i-=1){
 		   LCD_Cursor(c);
		   LCD_WriteData(display[i] + 48);
 		   c += 1;
  	 }
}
//--------End function ----------------------------------------------