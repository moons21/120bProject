#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "io.h"


/*
	This header file basically includes any helpful code to the project but dont require their own seperate header files
*/

//--------Creates a custom character at the lcd location --------------------------------------------------
void creatCustomChar(uint8_t location, uint8_t charmap[]) {
	location &= 0x7; // we only have 8 locations 0-7
	
	LCD_WriteCommand(0x40 + location*8);
	for (unsigned char i=0; i<8; i+=1) {
		LCD_WriteData(charmap[i]);
	}
}

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