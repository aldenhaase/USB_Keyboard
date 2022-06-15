/*
 * kbV3.c
 *
 * Created: 3/15/2022 11:14:32 AM
 * Author : alden
 */ 

#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/io.h>
#include <string.h>
#include "shift.h"
#include "usb.h"



int main(int argc, char** argv) {
	usb_init();

	while (!get_usb_config_status()) {
	}

	shift_init();
	while(1) shift(); // Scan the matrix
}
