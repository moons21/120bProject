/*
 * 
 * Author : Andrew Munoz
 */ 

// -----------------------------	Includes	-----------------------------
#include <avr/io.h>
#include "shiftregister.h"
#include "timer.h"
#include "io.c"
#include "taptap.h"	
#include "snes.h"


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


// -----------------------------	Row Shift Register (active low)	-----------------------------
#define PORTROW PORTD

#define ROW_DATA 4
#define ROW_LATCH	5
#define ROW_SRCLK 6
#define ROW_CLEAR 7	// LOW = clears the register. HIGH = keeps values

// -----------------------------	Column Shift Register	-----------------------------
#define PORTCOL PORTD
#define SRCOL PORTB

#define COL_DATA 0
#define COL_LATCH 1
#define COL_SRCLK 2
#define COL_CLEAR 3	// LOW = clears the register. HIGH = keeps values

// -----------------------------	Controller Defines	-----------------------------

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

//	-----------------------------	Function Prototypes	-----------------------------
void setRow(unsigned short data);
void setCol(unsigned char data);
void setPicture(unsigned char *picture);
unsigned short getSNESdata();

//	-----------------------------	Shared Variables	-----------------------------
unsigned char LCDnoteArea[] = {0,0,0,0,0,0,0,0};	// Each represents a row
	unsigned char pausePic[] = {0x3c,0x42,0xa5,0x81,0xa5,0x99,0x42,0xff};	// Each represents a row
unsigned short controllerData = 0;	
unsigned char incomingNotes = 0;	// replaces the top row of the note area
unsigned short missCount = 0;
unsigned short hitCount = 0;
unsigned short isPlaying = 0;
unsigned short endGame = 0;
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

unsigned short currentScore = 0;
	
//	----------------------------------------------------------	Start FSM definitions	----------------------------------------------------------

//---------------------- LED matrix display FSM----------------------
enum ledmStates{ledm_start, ledm_display, ledm_notplaying};
	
int ledmTick(int state){
	// State transitions
	switch(state){
		case ledm_start:
			state = ledm_display;
			break;
		case ledm_display:
			state = (isPlaying)? ledm_display : ledm_notplaying;
			break;
		case ledm_notplaying:
			state = (isPlaying)? ledm_display : ledm_notplaying;
			break;
		default: state = ledm_start; break;
	}
	// State actions
	switch(state){
		case ledm_start: break;
		case ledm_display:
			setPicture(LCDnoteArea);
			break;
		case ledm_notplaying:
			setPicture(pausePic);
			break;
		default: break;
	}
	return state;
}

//---------------------- Controller Read FSM----------------------
enum snesStates{readControllerData};

int snesReadTick(int state){
	// State transitions
	switch(state){
		case readControllerData:
		state = readControllerData;
		break;
		default:
		state = readControllerData;
		break;
	}
	// State Actions
	switch(state){
		case readControllerData:
			controllerData = getSNESdata();
			// FIXME THIS IS SUPER TEMPORARY!!!!!!!!!
			incomingNotes = 0;
			incomingNotes |=  (controllerData & SNES_Left)? 0xc0 : 0;
			incomingNotes |= (controllerData & SNES_Down)? 0x18 : 0;
			incomingNotes |= (controllerData & SNES_Right)? 0x3 : 0;
			break;
		default: break;
	}
	return state;
}
//---------------------- Falling Note FSM----------------------
// Period should be 1/8 of the music bpm
enum fallingStates{fs_wait, fs_falling};
	
int fallingTick(int state){
	unsigned char i;
	if (!isPlaying){
		return state;	// game isnt started, just return
	}
	// State transitions
	switch(state){
		case fs_wait:
			state =  fs_falling;
			break;
		case fs_falling:
			state = fs_falling;
			break;
		default:
			state = fs_wait;
			break;
	}
	// State actions
	switch(state){
		case fs_falling:	// Objective is to move all the rows down, add the new row at the top
			for(i = 7; i > 0; i -= 1){
				LCDnoteArea[i] = LCDnoteArea[i-1];
			}
			LCDnoteArea[0] = incomingNotes;
			break;
		default: break;
	}
	return state;
}

//---------------------- Note Hit FSM----------------------
enum notehitStates{notehit_wait, notehit_detect, notehit_hit, notehit_missed, notehit_error, notehit_hold};
unsigned char nhs_currentnote = 0;
unsigned char nhs_prevnote = 0;
int notehitTick(int state){
	static unsigned char i = 0;
	static unsigned char c = 0;
	nhs_prevnote = nhs_currentnote;
	nhs_currentnote = LCDnoteArea[7];
	if (!isPlaying){
		return state;	// game isnt started, just return
	}
	// State transitions
	switch(state){
		case notehit_wait:
			state = (nhs_prevnote == nhs_currentnote)? notehit_wait : notehit_detect;
			break;
		case notehit_detect:
			if (nhs_currentnote){
				// missed the note
				if( ((nhs_currentnote & 0xc0) && !(controllerData & SNES_Y)) ||
					((nhs_currentnote & 0x18) && !(controllerData & SNES_B)) ||
					((nhs_currentnote & 0x3) && !(controllerData & SNES_A))
				){
					c += 1;
					state = (c == 3)? notehit_missed: notehit_detect;}
				// Hit the note
				else{ state = notehit_hit;}
			}
			else{
				// Check if pressed the buttons when there was no note present
				if( ((controllerData & SNES_Y)) ||
				((controllerData & SNES_B)) ||
				((controllerData & SNES_A))
				){
					state = notehit_missed;
				}
				else{
					state = notehit_detect;
				}
			}
			break;
		case notehit_hit:
			c=0;
			state = notehit_hold;
			break;
		case notehit_missed:
			c=0;
			state = notehit_hold;
			break;
		case notehit_error:
			break;
		case notehit_hold:
			c=0;
			if ( nhs_prevnote != nhs_currentnote && nhs_currentnote){
				state = notehit_missed;		// Cant just hold notes to hit them
			}
			// Still on the same note
			else{
				state = (controllerData & (SNES_A | SNES_B | SNES_Y))? notehit_hold : notehit_detect;
			}
			break;
		default:
			state = notehit_detect;
			break;
	}
	// State actions
	switch(state){
		case notehit_wait:
			break;
		case notehit_detect: 
			break;
		case notehit_hit:
			hitCount += 1;
			break;
		case notehit_missed:
			missCount+=1;
			break;
		case notehit_error:
			missCount+=1;
			break;
		case notehit_hold: break;
		default:
		break;
	}
	if (i > 10){
		
		displayHighscore(missCount, 7, 4);
		
		displayHighscore(hitCount, 22,4);
		i = 0;
	}
	i += 1;
	return state;
}

// --------------------------	Start/Pause FSM	------------------------------
enum speStates {spe_start, spe_begin, spe_bhold, spe_init,spe_playing, spe_pause, spe_pausehold, spe_playhold, spe_endgame, spe_reset, spe_resethold};

int speTick(int state){
	static unsigned char endtimer = 0;	// tracks how long endgame message is held
	// Transition Logic
	switch (state){
		case spe_start:
			state = spe_init;
			break;
		case spe_init:
			state = spe_begin;
			break;
		case spe_begin:
			if (controllerData & SNES_Start){
				state = spe_playhold;
				LCD_ClearScreen();	// Clear screen of Message
			}
			else{
				state = spe_begin;
			}
			break;
			case spe_bhold:
			state = (controllerData & SNES_Start)? spe_bhold : spe_playing;
			break;
		case spe_playing:
			if (endGame){
				LCD_ClearScreen();
				LCD_DisplayString(1, "You Lose :(");
				state = spe_endgame;
			}
			else{
				if (controllerData & SNES_Select){
					state = spe_resethold;
				}
				else{
					state = (controllerData & SNES_Start)? spe_pausehold : spe_playing;	// checks if game is paused or not
					if (state == spe_pausehold){	// Display message when transitioning to pause screen
						LCD_ClearScreen();
						LCD_DisplayString(1, "GAME PAUSED");
					}
				}
			}
			break;
		case spe_pause:
			state = (controllerData & SNES_Start)? spe_playhold : spe_pause;	// checks if game is paused or not
			break;
		case spe_pausehold:
			state = (controllerData & SNES_Start)? spe_pausehold : spe_pause;	// checks if game is paused or not
			break;
		case spe_playhold:
			state = (controllerData & SNES_Start)? spe_playhold : spe_playing;	// checks if game is paused or not
			break;
		case spe_endgame:
			state = (endtimer > 100)? spe_init : spe_endgame;	// restarts game
			break;
		case spe_reset:
			state = spe_start;
			break;
		case spe_resethold:
			state = (controllerData & SNES_Select)? spe_resethold : spe_reset;
			break;
		default:	state = spe_start; break;
	}
	// Action Logic
	switch (state){
		case spe_start:
			break;
		case spe_init:
			LCD_ClearScreen();	// Clear screen of Message
			LCD_Cursor(1);
			LCD_WriteData(1);
			LCD_Cursor(16);
			LCD_WriteData(1);
			LCD_DisplayString(4, "Welcome!");
			LCD_DisplayString( 19,"Press Start!");
			endtimer = 0;	// reset timer
			isPlaying = 0;	// Not playing yet
			endGame = 0;	// Game isnt over
			break;
		case spe_begin:
			break;
		case spe_bhold:
			break;
		case spe_playing:
			isPlaying = 1;
			break;
		case spe_pause:
			break;
		case spe_pausehold:
			isPlaying = 0;	// Pause the game
			break;
		case spe_playhold:
			break;
		case spe_endgame:
			isPlaying = 0;	// Game has ended, not playing anymore
			endtimer += 1;	// increment endgame timer
			break;
		case spe_reset:
			isPlaying = 0;
			break;
		case spe_resethold:
			break;
		default:	break;
	}
	return state;
}

// --------------------------	Highscore FSM	------------------------------
enum highscoreStates{hs_start, hs_init, hs_getscore, hs_update};
	
int hsTick(int state){
	// State transitions
	switch(state){
		case hs_start:
			state = hs_init;
			break;
		case hs_init:
			state = hs_getscore;
			break;
		case hs_getscore:
			break;
		case hs_update:
			break;
		default: break;
	}
	// state actions
	switch(state){
		case hs_start:
			break;
		case hs_init:
			break;
		case hs_getscore:
			break;
		case hs_update:
			break;
		default: break;
	}
	return state;
}


//	----------------------------------------------------------	End FSM definitions	----------------------------------------------------------
	
//	-----------------------------	Main	-----------------------------
int main(void)
{
	DDRA = 0xFE; PORTA = 0x01;	// Pin0 is input, rest is output
    DDRD = 0xFF; PORTD = 0x00;	// PORTD used for row and column shift registers
	DDRC = 0xff; PORTC = 0x00;	// PORTC is lcd
	
	LCD_init();
	creatCustomChar(1,customChar_musicNote);	// Creates the custom music note
	
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 20;		// LED matrix needs at least 20 ms period as to not have noticable flickering
	unsigned long int SMTick2_calc = 50;		// ms for snes controller
	unsigned long int SMTick3_calc = 200;		// Falling notes
	unsigned long int SMTick4_calc = SMTick2_calc;		// Hit detection
	unsigned long int SMTick5_calc = SMTick2_calc;		// main pause/play screen
	
	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SMTick1_calc, SMTick2_calc);
	tmpGCD = findGCD(tmpGCD, SMTick3_calc);
	tmpGCD = findGCD(tmpGCD, SMTick4_calc);
	tmpGCD = findGCD(tmpGCD, SMTick5_calc);
	
	
	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;
	
	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;
	unsigned long int SMTick2_period = SMTick2_calc/GCD;
	unsigned long int SMTick3_period = SMTick3_calc/GCD;
	unsigned long int SMTick4_period = SMTick4_calc/GCD;
	unsigned long int SMTick5_period = SMTick5_calc/GCD;

	//Declare an array of tasks
	static task task1, task2, task3, task4,task5;
	task *tasks[] = { &task5, &task3, &task1, &task2, &task4};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	// Task 1	(LED matrix)
	task1.state = -1;//Task initial state.
	task1.period = SMTick1_period;//Task Period.
	task1.elapsedTime = SMTick1_period;//Task current elapsed time.
	task1.TickFct = &ledmTick;//Function pointer for the tick.
	
	// Task 2 (SNES controller)
	task2.state = -1;//Task initial state.
	task2.period = SMTick2_period;//Task Period.
	task2.elapsedTime = SMTick2_period;//Task current elapsed time.
	task2.TickFct = &snesReadTick;//Function pointer for the tick.
	
	// Task 3 (Falling Notes)
	task3.state = -1;//Task initial state.
	task3.period = SMTick3_period;//Task Period.
	task3.elapsedTime = SMTick3_period;//Task current elapsed time.
	task3.TickFct = &fallingTick;//Function pointer for the tick.
	
	// Task 4 (Hit detection)
	task4.state = -1;//Task initial state.
	task4.period = SMTick4_period;//Task Period.
	task4.elapsedTime = SMTick4_period;//Task current elapsed time.
	task4.TickFct = &notehitTick;//Function pointer for the tick.
	
	// Task 5 (Play/pause/end)
	task5.state = -1;//Task initial state.
	task5.period = SMTick5_period;//Task Period.
	task5.elapsedTime = SMTick5_period;//Task current elapsed time.
	task5.TickFct = &speTick;//Function pointer for the tick.
	
	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();

	unsigned short i; // Scheduler for-loop iterator
    while (1) 
    {	
		 // Scheduler code
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
}


//	-----------------------------	LED Matrix Functions	-----------------------------
void setRow(unsigned short data){
	signed char i = 0;
	unsigned char reverse = 0;	// since char inputs least sig bit first, we have to reverse it before it gets reversed...
	for (i = 7; i >= 0; i -= 1){
		reverse |= ((data >> i) & 0x01)? 0x80 >> i : 0;
	}
	
	transmit_data(&PORTROW, reverse,
	ROW_DATA, ROW_LATCH, ROW_SRCLK, ROW_CLEAR);
}
void setCol(unsigned char data){
	transmit_data(&PORTCOL,data,
	COL_DATA, COL_LATCH, COL_SRCLK, COL_CLEAR);
}

void setPicture(unsigned char *picture){
	// find what to set the rows to
	for (unsigned char i = 0; i < 8; i += 1){
		//setRow((0x01 << i) & 0x08);
		setCol(0x00);
		setRow(0xff);
		setRow(~(0x01 << i));
		setCol(picture[i]);
	}
}

//	----------------------------- End	LED Matrix Functions	-----------------------------

//	----------------------------- SNES controller Helper Function	-----------------------------
unsigned short getSNESdata(){
	return readSNES(&PORTSNES,&PINSNES,
	CLOCK_SNES, LATCH_SNES, DATA_SNES);
}