/*
 * 
 * Author : Andrew Munoz
 */ 

// -----------------------------	Includes	-----------------------------
#include <avr/io.h>
#include <avr/eeprom.h>		// Allows writing to controller EEPROM
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
unsigned char pausePic[] = {0x3c,0x42,0xa5,0x81,0xbd,0x81,0x42,0xff};	// Each represents a row
unsigned char happyFace[] = {0x3c,0x42,0xa5,0x81,0xa5,0x99,0x42,0xff};	// Each represents a row
unsigned char sadFace[] = {0x3c,0x42,0xa5,0x81,0x99,0xa5,0x42,0xff};	// Each represents a row
unsigned short controllerData = 0;	

unsigned char incomingNotes = 0;	// replaces the top row of the note area
unsigned char newNote = 0xc0;

unsigned short missCount = 0;
unsigned short hitCount = 0;
unsigned short chordCount = 0;
unsigned char isPlaying = 0;
unsigned char endGame = 0;
unsigned char resetGame = 0;
unsigned char newHS = 0;
unsigned long oldHighscore = 0;
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

unsigned long currentScore = 0;

unsigned long long leftnote = 0x2bc5a794ebe9ab30  ;
unsigned long long middlenote = 0xba483d605045ced9  ;
unsigned long long rightnote = 0x64d70f3903d06909  ;


// unsigned long leftnote = 0b10000111011000100000100100000011;
// unsigned long middlenote = 0b10100010000010001001001001000100;
// unsigned long rightnote = 0b10011000011100100100010000100000;

// unsigned long leftnote = 0b1110000111;
// unsigned long middlenote = 0b0000000111;
// unsigned long rightnote = 0b1110000111;
unsigned long long l, m, r;

	
//	----------------------------------------------------------	Start FSM definitions	----------------------------------------------------------

//---------------------- LED matrix display FSM----------------------
enum ledmStates{ledm_start, ledm_display, ledm_notplaying, ledm_endgamelose, ledm_endgamewin, ledm_endgame};
	
int ledmTick(int state){
	static unsigned char i = 0;
	// State transitions
	switch(state){
		case ledm_start:
			state = ledm_display;
			break;
		case ledm_display:
			state = (isPlaying)? ledm_display : (endGame)? ledm_endgame : ledm_notplaying ;
			break;
		case ledm_notplaying:
			state = (isPlaying)? ledm_display : (endGame)? ledm_endgame : ledm_notplaying ;
			break;
		case ledm_endgamelose:
			state = (isPlaying)? ledm_display : ledm_endgamelose ;
			//state = (isPlaying)? ledm_display : (endGame)? ledm_endgame : ledm_notplaying ;
			break;
		case ledm_endgamewin:
			state = (isPlaying)? ledm_display : ledm_endgamewin;
			break;
		case ledm_endgame:
			if (i == 5){
				i = 0;
				state = (newHS)? ledm_endgamewin : ledm_endgamelose;
			}
			else {
				i+= 1;
				state = ledm_endgame;
				}
			break;
		default: state = ledm_start; break;
	}
	// State actions
	switch(state){
		case ledm_start: break;
		case ledm_display:
			i = 0;
			setPicture(LCDnoteArea);
			break;
		case ledm_notplaying:
			setPicture(pausePic);
			break;
		case ledm_endgamelose:
			setPicture(sadFace);
			break;
		case ledm_endgamewin:
			setPicture(happyFace);
			break;
		case ledm_endgame:
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
			break;
		default: break;
	}
	return state;
}
//---------------------- Falling Note FSM----------------------
// Period should be 1/8 of the music bpm
enum fallingStates{fs_start, fs_init, fs_wait, fs_falling};

int fallingTick(int state){
	unsigned char i;
	static unsigned char timing = 0;
	
	if (resetGame || endGame){
		return fs_start;
	}
	if (!isPlaying){
		return state;	// game isnt started, just return
	}
	// State transitions
	switch(state){
		case fs_start:
			state = fs_init;
			break;
		case fs_init:
			state = fs_falling;
			break;
		case fs_falling:
			state = fs_falling;
			break;
		default:
			state = fs_start;
			break;
	}
	// State actions
	switch(state){
		case fs_start:
			break;
		case fs_init:
			timing = 0;
			l = leftnote;
			r = rightnote;
			m = middlenote;
			for (i = 0; i < 8; i += 1){
				LCDnoteArea[i] = 0;
			}
			break;
		case fs_falling:	// Objective is to move all the rows down, add the new row at the top
			for(i = 7; i > 0; i -= 1){
				LCDnoteArea[i] = LCDnoteArea[i-1];
			}
			if (timing == 3){
				if (l & 0x01){LCDnoteArea[0] = newNote;}
				if (m & 0x01){LCDnoteArea[0] |= newNote >> 3;}
				if (r & 0x01){LCDnoteArea[0] |= newNote >> 6;}
				l = l >> 1; m = m >> 1; r = r >> 1;
			}
			else{
				LCDnoteArea[0] = incomingNotes;
			}
			timing = (timing == 3)? 0 : timing + 1;
			
			break;
		default: break;
	}
	return state;
}

//---------------------- Note Hit FSM----------------------
enum notehitStates{notehit_start, notehit_init, notehit_wait, notehit_detect, notehit_hit, notehit_missed, notehit_error, notehit_hold};
	
unsigned char nhs_currentnote = 0;
unsigned char nhs_prevnote = 0;

int notehitTick(int state){
	static unsigned char i = 0;
	static unsigned char forgiveness = 0;
	nhs_prevnote = nhs_currentnote;
	nhs_currentnote = LCDnoteArea[7];
	
	if (endGame || resetGame){
		return notehit_start;
	}
	if (!isPlaying){
		return state;	// game is paused, just return
	}
	
	// State transitions
	switch(state){
		case notehit_start:
			state = notehit_init;
			break;
		case notehit_init:
			state = notehit_detect;
			break;
		case notehit_detect:
			// detect a hit
			if(nhs_currentnote){
				if( (((nhs_currentnote & 0xc0) && (controllerData & SNES_Y)) || (!(nhs_currentnote & 0xc0) && !(controllerData & SNES_Y)))
				&& (((nhs_currentnote & 0x18) && (controllerData & SNES_B)) || (!(nhs_currentnote & 0x18) && !(controllerData & SNES_B)))
				&& (((nhs_currentnote & 0x3) && (controllerData & SNES_A)) || (!(nhs_currentnote & 0x3) && !(controllerData & SNES_A)))
				){
					state = notehit_hit;
				}
			}
			else{
				if ((controllerData & SNES_Y) ||(controllerData & SNES_B) || (controllerData & SNES_A) ){	// You cant just press and hold buttons
					if (forgiveness == 5){
						forgiveness = 0;
						state = notehit_missed;
					}
					else{
						forgiveness += 1;
					}
				}
				state = notehit_detect;
			}
			break;
		case notehit_hit:
			state = notehit_hold;
			break;
		case notehit_missed:
			state = notehit_hold;
			break;
		case notehit_error:
			break;
		case notehit_hold:
			if (nhs_prevnote == nhs_currentnote){	// we stayed on the same note
				state = (controllerData & (SNES_A | SNES_B | SNES_Y))? notehit_hold : notehit_detect;	// transition to detect
			}
			else{	// we moved to a new note
				state = notehit_missed;	// and we mustve missed it
			}
			break;
		default:
			state = notehit_start;
			break;
	}
	// State actions
	switch(state){
		case notehit_start:
			break;
		case notehit_init:
			hitCount = 0;
			missCount = 0;
			chordCount = 0;
			break;
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
	return state;
}

// --------------------------	Start/Pause FSM	------------------------------
enum speStates {spe_start, spe_begin, spe_bhold, spe_init,spe_playing, spe_pause, spe_pausehold, spe_playhold, spe_endgame, spe_reset, spe_resethold, spe_resettimer};

int speTick(int state){
	static unsigned char endtimer = 0;	// tracks how long endgame message is held
	static unsigned char timer = 0;	// tracks when note stuff is empty
	static unsigned char rtimer = 0;	// for reset 
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
			if (l == 0 && m == 0 && r == 0){	// game has ended
				if (timer == 30){
					
					state = spe_endgame;
				}
				else{
					timer += 1;
					state = (controllerData & SNES_Start)? spe_pausehold : spe_playing;	// checks if game is paused or not
					if (state == spe_pausehold){	// Display message when transitioning to pause screen
						LCD_ClearScreen();
						LCD_DisplayString(1, "GAME PAUSED");
					}
				}
			}
			else{
				if (controllerData & SNES_Select){		// Reset game
					resetGame = 1;
					state = spe_reset;
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
			state = spe_resethold;
			break;
		case spe_resethold:
			state = (controllerData & SNES_Select)? spe_resethold : spe_start;
			break;
		case spe_resettimer:
			if (rtimer > 5){
				rtimer	= 0;
				state = spe_start;
			}
			else{
				rtimer += 1;
				state = spe_resettimer;
			}
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
			resetGame = 0;
			timer = 0;
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
			endGame = 1;
			break;
		case spe_reset:
			resetGame = 1;
			isPlaying = 0;
			break;
		case spe_resethold:
			break;
		case spe_resettimer:
			break;
		default:	break;
	}
	return state;
}

// --------------------------	Highscore FSM	------------------------------
enum highscoreStates{hs_start, hs_init, hs_getscore, hs_update, hs_paused, hs_holdupdate};
	
int hsTick(int state){
	
	static unsigned long cS;
	static unsigned long oldScore;
	static unsigned char i = 0;
	
	if(resetGame){
		return hs_start;	// dont let a reset break the game
	}
	// State transitions
	switch(state){
		case hs_start:
			state = hs_init;
			break;
		case hs_init:
			state = hs_getscore;
			break;
		case hs_getscore:
			state = (endGame || resetGame)? hs_update : hs_getscore;	// if the game is over, check to update the score
			break;
		case hs_paused:
			state = isPlaying? hs_getscore : hs_paused;
			if (state = hs_getscore){LCD_ClearScreen();}	// Goes back to displaying score
			break;
		case hs_update:
			state = hs_holdupdate;
			break;
		case hs_holdupdate:
			state = (endGame)? hs_holdupdate : hs_start;
			break;
		default: state = hs_start; break;
	}
	// state actions
	switch(state){
		case hs_start:
			break;
		case hs_init:
			i = 0;
			cS = 0;
			oldScore = 0;
			currentScore = 0;
			hitCount = 0;
			newHS = 0;
			//oldHighscore = eeprom_read_dword((uint32_t*)0);			// FIXMEFIXMEFIXEME
			oldHighscore = 420;
			break;
		case hs_getscore:
			if (isPlaying){
				currentScore = hitCount * 25 + chordCount * 1000;
				oldScore = cS;
				cS = currentScore;
				if ((((controllerData && SNES_A) || (controllerData && SNES_B) ||(controllerData && SNES_Y) )) && (oldScore != currentScore)){
					i = 1;
					LCD_DisplayString(1, "Score: ");
					displayHighscore(currentScore, 8, 8);
					newHS = (currentScore > oldHighscore)? 1 : 0;
				}
			}
			break;
		case hs_paused:
			i = 0;
			break;
		case hs_update:
			if ((currentScore > oldHighscore)){
				newHS = 1;
				LCD_ClearScreen();
				LCD_DisplayString(1, "New HighScore!");
				displayHighscore(currentScore, 24, 8);
				// eeprom_update_dword (( uint32_t *) 0 , currentScore ) ;	// FIXMEFIXMEFIXME
			}
			else{
				LCD_ClearScreen();
				LCD_DisplayString(1, "GAME OVER");
				LCD_DisplayString(17, "Old HS: ");
				displayHighscore(oldHighscore, 24, 8);
				newHS = 0;
			}
// 			oldHighscore ;
// 			if (currentScore > oldHighscore){
// 				eeprom_update_dword (( uint32_t *) 0 , currentScore ) ;
// 			}
			break;
		case hs_holdupdate:
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
	//eeprom_update_dword (( uint32_t *) 0 , 420 ) ;
	creatCustomChar(1,customChar_musicNote);	// Creates the custom music note
	
	
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 20;		// LED matrix needs at least 20 ms period as to not have noticable flickering
	unsigned long int SMTick2_calc = 50;		// ms for snes controller
	unsigned long int SMTick3_calc = 100;		// Falling notes
	unsigned long int SMTick4_calc = SMTick2_calc;		// Hit detection
	unsigned long int SMTick5_calc = 50;		// main pause/play screen
	unsigned long int SMTick6_calc = 250;		// Highscore stuff
	
	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SMTick1_calc, SMTick2_calc);
	tmpGCD = findGCD(tmpGCD, SMTick3_calc);
	tmpGCD = findGCD(tmpGCD, SMTick4_calc);
	tmpGCD = findGCD(tmpGCD, SMTick5_calc);
	tmpGCD = findGCD(tmpGCD, SMTick6_calc);
	
	
	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;
	
	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;
	unsigned long int SMTick2_period = SMTick2_calc/GCD;
	unsigned long int SMTick3_period = SMTick3_calc/GCD;
	unsigned long int SMTick4_period = SMTick4_calc/GCD;
	unsigned long int SMTick5_period = SMTick5_calc/GCD;
	unsigned long int SMTick6_period = SMTick6_calc/GCD;

	//Declare an array of tasks
	static task task1, task2, task3, task4,task5, task6;
	task *tasks[] = { &task5, &task3, &task1, &task2, &task4, &task6};
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
	
	// Task 5 (Highscore)
	task6.state = -1;//Task initial state.
	task6.period = SMTick6_period;//Task Period.
	task6.elapsedTime = SMTick6_period;//Task current elapsed time.
	task6.TickFct = &hsTick;//Function pointer for the tick.
	
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