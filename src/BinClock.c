/*
 * BinClock.c
 * Jarrod Olivier
 * Modified for EEE3095S/3096S by Keegan Crankshaw
 * August 2019
 * 
 * BRDCAL003 NRGSAM001
 * August 2019
*/

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <softPwm.h>
#include <stdio.h> //For printf functions
#include <stdlib.h> // For system functions
#include <signal.h> //For the keyboard interrupt
#include "BinClock.h"
#include "CurrentTime.h"

//Global variables
int hours, mins, secs;
long lastInterruptTime = 0; //Used for button debounce
int RTC; //Holds the RTC instance

int HH,MM,SS;

void initGPIO(void){
	/* 
	 * Sets GPIO using wiringPi pins. see pinout.xyz for specific wiringPi pins
	 * You can also use "gpio readall" in the command line to get the pins
	 * Note: wiringPi does not use GPIO or board pin numbers (unless specifically set to that mode)
	 */
	printf("Setting up\n");
	wiringPiSetup(); //This is the default mode. If you want to change pinouts, be aware
	
	RTC = wiringPiI2CSetup(RTCAddr); //Set up the RTC
	
	//Set up the LEDS
	for(int i; i < sizeof(LEDS)/sizeof(LEDS[0]); i++){
	    pinMode(LEDS[i], OUTPUT);
	}
	
	//Set Up the Seconds LED for PWM
	softPwmCreate(SECS, 0, 59);
	//Write your logic here
	
	printf("LEDS done\n");
	
	//Set up the Buttons
	
	pinMode(5, INPUT);
	pullUpDnControl(5, PUD_UP);
	pinMode(30, INPUT);
	pullUpDnControl(30, PUD_UP);
	//Attach interrupts to Buttons
	wiringPiISR(5, INT_EDGE_FALLING, &hourInc);
	wiringPiISR(30, INT_EDGE_FALLING, &minInc);
	
	printf("BTNS done\n");
	printf("Setup done\n");
}
/*
*Cleans up the LED outputs, set them all to input
*/
void cleanup(int a){
	printf("\nCleaning up...\n");
	for(int i=0; i < sizeof(LEDS)/sizeof(LEDS[0]); i++){
            pinMode(LEDS[i], INPUT);
        } 
	pinMode(SECS, INPUT);
	printf("\nLEDs reset\n");
	exit(0);
}

/*
 * The main function
 * This function is called, and calls all relevant functions we've written
 */
int main(void){
	initGPIO();

	//Set test time (12:59PM)
	//You can comment this file out later
	//wiringPiI2CWriteReg8(RTC, HOUR, decCompensation(12));
	//wiringPiI2CWriteReg8(RTC, MIN, decCompensation(59));
	//wiringPiI2CWriteReg8(RTC, SEC, 0b10000000);

	delay(201);
	toggleTime(); // gets the current time and starts counting. comment to use the above test time.
	signal(SIGINT, cleanup);
	// Repeat this until we shut down
	for (;;){
		//Fetch the time from the RTC
		hours = wiringPiI2CReadReg8(RTC, HOUR);
		mins = wiringPiI2CReadReg8(RTC, MIN);
		secs = wiringPiI2CReadReg8(RTC, SEC);
		
		hours = hFormat(hexCompensation(hours));
		mins = hexCompensation(mins);
		secs = hexCompensation(secs);
		//Write your logic here
		
		//Function calls to toggle LEDs
		lightHours(hours);
		lightMins(mins);
		secPWM(secs);
		//Write your logic here
		
		// Print out the time we have stored on our RTC
		printf("The current time is: %d:%d:%d\n", hours, mins, secs);

		//using a delay to make our program "less CPU hungry"
		delay(1000); //milliseconds
	}
	return 0;
}

/*
 * Change the hour format to 12 hours
 */
int hFormat(int hours){
	/*formats to 12h*/
	if (hours >= 24){
		hours = 0;
	}
	else if (hours > 12){
		hours -= 12;
	}
	return (int)hours;
}

/*
 * Turns on corresponding LED's for hours
 * Goes through binary bit by bit to write each to the LEDs
 * When plugging in LEDs, Hours first then minutes
 * but they should go in reverse order or flip the shift
 * ie: LSB of hours plugs into pin 0
 * adapted from https://www.programmingsimplified.com/c/source-code/c-program-convert-decimal-to-binary
 */
void lightHours(int units){
	for(int shift = 3; shift >= 0; shift--){
		int shifted = units>>shift;
		if (shifted & 1){
			digitalWrite(LEDS[shift], 1);
		}
		else{
			digitalWrite(LEDS[shift], 0);
		}
	}	
}

/*
 * Turn on the Minute LEDs
 */
void lightMins(int units){
	for(int shift = 5; shift >= 0; shift--){
                int shifted = units>>shift;
                if (shifted & 1){
                        digitalWrite(LEDS[shift+4], HIGH);
                }
                else{
                        digitalWrite(LEDS[shift+4], LOW);
                }
        }
}

/*
 * PWM on the Seconds LED
 * The LED should have 60 brightness levels
 * The LED should be "off" at 0 seconds, and fully bright at 59 seconds
 */
void secPWM(int units){
	// Write your logic here
	softPwmWrite(SECS, units);
}

/*
 * hexCompensation
 * This function may not be necessary if you use bit-shifting rather than decimal checking for writing out time values
 */
int hexCompensation(int units){
	/*Convert HEX or BCD value to DEC where 0x45 == 0d45 
	  This was created as the lighXXX functions which determine what GPIO pin to set HIGH/LOW
	  perform operations which work in base10 and not base16 (incorrect logic) 
	*/
	int unitsU = units%0x10;
	units = units & 0b01111111; //set the 7th bit to zero. doesnt work without doing this

	if (units >= 0x50){
		units = 50 + unitsU;
	}
	else if (units >= 0x40){
		units = 40 + unitsU;
	}
	else if (units >= 0x30){
		units = 30 + unitsU;
	}
	else if (units >= 0x20){
		units = 20 + unitsU;
	}
	else if (units >= 0x10){
		units = 10 + unitsU;
	}
	return units;
}


/*
 * decCompensation
 * This function "undoes" hexCompensation in order to write the correct base 16 value through I2C
 */
int decCompensation(int units){
	int unitsU = units%10;	

	if (units >= 50){
		units = 0x50 + unitsU;
	}
	else if (units >= 40){
		units = 0x40 + unitsU;
	}
	else if (units >= 30){
		units = 0x30 + unitsU;
	}
	else if (units >= 20){
		units = 0x20 + unitsU;
	}
	else if (units >= 10){
		units = 0x10 + unitsU;
	}
	return units;
}


/*
 * hourInc
 * Fetch the hour value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 23 hours in a day
 * Software Debouncing should be used
 */
void hourInc(void){
	//Debounce
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 1 triggered, %x\n", hours);
		//Fetch RTC Time
		int h = hexCompensation(wiringPiI2CReadReg8(RTC, HOUR));
		//Increase hours by 1, ensuring not to overflow
		h = hFormat(h+1);
		//Write hours back to the RTC
		wiringPiI2CWriteReg8(RTC, HOUR, decCompensation(h));
	}
	lastInterruptTime = interruptTime;
}

/* 
 * minInc
 * Fetch the minute value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 60 minutes in an hour
 * Software Debouncing should be used
 */
void minInc(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 2 triggered, %x\n", mins);
		//Fetch RTC Time
		int m = hexCompensation(wiringPiI2CReadReg8(RTC, MIN));
		//Increase minutes by 1, ensuring not to overflow
		m = m + 1;
		if (m >= 60){
			m = 0;
			hourInc();
		}
		//Write minutes back to the RTC
		wiringPiI2CWriteReg8(RTC, MIN, decCompensation(m));
	}
	lastInterruptTime = interruptTime;
}

//This interrupt will fetch current time from another script and write it to the clock registers
//This functions will toggle a flag that is checked in main
void toggleTime(void){
	long interruptTime = millis();
	printf("time: %ld", interruptTime);

	if (interruptTime - lastInterruptTime>200){ //interruptTime - lastInterruptTime>200
		HH = getHours();
		MM = getMins();
		SS = getSecs();

		HH = hFormat(HH);
		HH = decCompensation(HH);
		wiringPiI2CWriteReg8(RTC, HOUR, HH);

		MM = decCompensation(MM);
		wiringPiI2CWriteReg8(RTC, MIN, MM);

		SS = decCompensation(SS);
		wiringPiI2CWriteReg8(RTC, SEC, 0b10000000+SS);

	}
	lastInterruptTime = interruptTime;
}
