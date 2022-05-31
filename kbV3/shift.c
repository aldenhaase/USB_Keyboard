/*
 * CFile1.c
 *
 * Created: 3/15/2022 11:18:06 AM
 *  Author: alden
 */ 
#include "shift.h"
#include "layout.h"
#include "usb.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <util/delay.h>
#include <stdlib.h>

#define r 0
#define g 1
#define b 2

#define CLOCK_HIGH() (PORTB |= (1<<PORTB4))
#define CLOCK_LOW() (PORTB &= ~(1<<PORTB4))
#define RW_HIGH() (PORTD |= (1<<PORTD7))
#define RW_LOW() (PORTD &= ~(1<<PORTD7))
#define MODE_HIGH() (PORTD |= (1<<PORTD4))
#define MODE_LOW() (PORTD &= ~(1<<PORTD4))
#define READ_PIN_A() ((PIND & (1<<PIND6))>>PIND6)
#define READ_PIN_B() ((PINB & (1<<PINB7))>>PINB7)
#define READ_PIN_C() ((PINB & (1<<PINB6))>>PINB6)
#define WRITE_LED_LOW() (PORTB &= ~(1<<PORTB5))
#define WRITE_LED_HIGH() (PORTB |= (1<<PORTB5))
#define poll_value 300

int dimming_resolution =50;
int pressed_index = 0;
int base_index = 3;
int caps_index = 6;
int numbers_index = 9;
int function_index = 12;
int dimming_index = 15;

int led_dimmer_counter = 0;

int layout_num = 0;
int num_colors = 16;
bool has_unsent_packets = false;
int pressed_color[16][3] = {
	{129, 0, 26},
	{136, 14, 255},
	{0, 30, 230},
	{0, 230, 239},
	{255, 70, 0},
	{200, 30, 0},
	{170, 100, 7},
	{120, 0, 80},
	{120, 30, 19},
	{0, 0, 100},
	{0, 30, 190},
	{190, 0, 190},
	{120, 30, 10},
	{230, 0, 190},
	{220, 0, 0},
	{198, 0, 4}
						};
int base_color[3] = {2, 2, 2};
int caps_color[3] = {100, 0, 0};
int numbers_color[3] = {22,8,4};
int function_color[3] = {4, 4, 10};
int colors[18] = {};
int off[3] = {0,0,0}; 
bool pressed_led_still_lit = false;
bool led_data[1153] = {0}; //This is one bigger than need be because the send led function currently overshoots
int active_led[48] = {};
int led_color[48] =  {};
int previous_state[16] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
int debounce_arr[16] = {0};
int debounce_value = 16;
bool caps_toggle = false;



	//void do_layer_led() { PORTC = layout_num << 6; }
		

	void reset_led(){
				WRITE_LED_LOW();
				for(int i = 0; i < 617; i++){ //send reset signal ***this has not be scoped yet
					asm volatile ("nop\n");
				}
	}
	

	void send_led_data() {
		cli();
		bool set = led_data[0];
		reset_led();
		int i = 0;
		while(1){
			if(!set){
				WRITE_LED_HIGH();
				++i;
				set = led_data[i];
				if(i > 1151){
					break;
				}
				WRITE_LED_LOW();
			}
			else{
				if(i > 1151){
					break;
				}
				++i;
				set = led_data[i];
				WRITE_LED_HIGH();
				WRITE_LED_LOW();
			}
		}
		WRITE_LED_HIGH(); // So that it doesn't reset the array
		sei();
	}
	

	
	void add_led_data(int index, int color_index){
		int j = 0;
		for(int i = index; i < index + 8; i ++){
			led_data[i] = !(colors[color_index] & (1 << (7 - j)));
			led_data[i+8] = !(colors[color_index + 1] & (1 << (7-j)));
			led_data[i+16] = !(colors[color_index + 2] & (1 << (7-j)));
			j++;
		}	
	}

	
	void fill_leds(int fill_color_index){
		for(int i = 0; i <48; i++){
			if(caps_toggle && i == 37){
					continue;
			}
			active_led[i] = 0;
			add_led_data(i*24, fill_color_index);	
			}
		send_led_data();
	}
	
	void swap_index(int* first, int* second){
		int temp = *first;
		*first = *second;
		*second = temp;
	}
		
	void add_key_to_HID(uint16_t key){
		if (key & KEY_MOD_MACRO) { // Is this a macro key, in which case we have to set the appropriate value in the modifier bit
			key &= ~(KEY_MOD_MACRO);
			keyboard_modifier |= key;
		} else if (key & KEY_LS_MACRO) { // Is this a layer shift macro, in which case we have to change the layer number
				key &= ~(KEY_LS_MACRO);
				layout_num = key;
				if(layout_num == 1){
					swap_index(&base_index, &numbers_index);
					fill_leds(base_index);
				}else{
					swap_index(&base_index, &function_index);
					fill_leds(base_index);
				}
				for(int i = 0; i < 6; i++){
					keyboard_pressed_keys[i] = 0;
				} // We have to clear the report after shifting layers because of conflicts from key-ups and key-downs on different layers
		} else{
			for (int k = 0; k < 6; k++) {
				if (keyboard_pressed_keys[k] == KEY_NONE) {
				keyboard_pressed_keys[k] = key;
				break;
				}
			}
		}
				}

	
	void remove_key_from_HID(uint16_t key){
		if (key & KEY_MOD_MACRO) { // Is this a macro key, we have to remove the macro bit from the modifier byte to signify a key up
			key &= ~(KEY_MOD_MACRO);
			keyboard_modifier &= ~(key);
		} else if (key & KEY_LS_MACRO) { // Is this a layer shift key, we have to remove the bit from layout_num to switch back to the previous layer
							if(layout_num == 1){
								swap_index(&base_index, &numbers_index);
								fill_leds(base_index);
								}else{
								swap_index(&base_index, &function_index);
								fill_leds(base_index);
							}
			layout_num = 0;
			for(int i = 0; i < 6; i++){
				keyboard_pressed_keys[i] = 0;
			}
		} else {
			for (int k = 0; k < 6; k++) { // Normal key up event, find the key in the HID report and remove it
				if (keyboard_pressed_keys[k] == key) {
					keyboard_pressed_keys[k] = 0;
					break;
				}
			}
		}
	}
	
	
	void process_led_toggle(){
	if(!caps_toggle){
		add_led_data(37*24, caps_index);
	}else{
		add_led_data(37*24, base_index);
	}
		send_led_data();
	}
	
	void randomize(int index){
			led_color[index] = rand() % 16;
	}
	
	void process_key_down(int index, int sect, uint16_t key){
		int bit_index = (index * 24) + (sect * 384);
		add_key_to_HID(key);
		if(key != KEY_CAPSLOCK){
			active_led[(16*sect)+index] = 0;
			int led_index = (16*sect)+index;
			randomize(led_index);
			for(int i = 0; i < 3;  i ++){
				colors[pressed_index + i] = pressed_color[led_color[led_index]][i];
			}
			add_led_data(bit_index,	pressed_index);
			send_led_data();
		}else{
			process_led_toggle();
		}
	}
	
	void process_key_up(int index, int sect, uint16_t key){
		if(key != KEY_CAPSLOCK){
		active_led[(16*sect)+index] = dimming_resolution;
		}else{
			caps_toggle = !caps_toggle;
		}
		remove_key_from_HID(key);
	}
	
	void check_for_change(int index, int shift_reg_value, int sect){
			uint16_t key = key_array[layout_num][sect][index];
			if(shift_reg_value){
				process_key_up(index, sect, key);
			}else{
				process_key_down(index, sect ,key);
			}
	}
	
	
		
	int calc_dimmed(int remaining, int index, int rgb){
			float ratio = 1.0/dimming_resolution;
			float diff;
				if(pressed_color[led_color[index]][rgb] > colors[base_index + rgb]){
					diff = ((pressed_color[led_color[index]][rgb] - colors[base_index + rgb])*ratio);
				int ret_val = pressed_color[led_color[index]][rgb] - (diff * remaining);
				if(ret_val < 0){
					return 0;
				}else{
					return ret_val;
				}
			}else{
					diff = ((colors[base_index + rgb] - pressed_color[led_color[index]][rgb])*ratio);
					int ret_val = pressed_color[led_color[index]][rgb] + (diff * remaining);
					if(ret_val >255){
						return 255;
						}else{
						return ret_val;
					}

			}
		}

	
	
	void dim_leds(){
		if(led_dimmer_counter == poll_value){
			for(int i = 0; i<48; i++){
				if(active_led[i]){
					if(active_led[i] > 1){
						int remaining = dimming_resolution - active_led[i] + 1;
						for(int j = 0; j < 3; j ++){
							colors[dimming_index + j] = calc_dimmed(remaining, i, j);
						}
						add_led_data(i*24,dimming_index);
						for(int j = 0; j < 3; j ++){
							colors[dimming_index + j] = colors[base_index + j];
						}
						}else{
						add_led_data(i*24, base_index);
					}
					active_led[i]--;
				}
				
			}
			send_led_data();
			led_dimmer_counter = 0;
			}else{
			led_dimmer_counter++;
		}
	}
	
	
	ISR(TIMER0_COMPA_vect) {
			if (has_unsent_packets) {
				usb_send();
				has_unsent_packets = false;
			}
		}
		
	void shift() {
		cli();
		MODE_HIGH();//put shift registers into parallel load mode
		CLOCK_HIGH();
		CLOCK_LOW(); 
		MODE_LOW(); //put shift registers into serial output mode
		sei();
		int key_values[3];
		int current_state;
		int changed_values;
		for(int i = 0; i < 16; i++){
			key_values[0] = READ_PIN_A();
			key_values[1] = READ_PIN_B();
			key_values[2] = READ_PIN_C();		
			current_state = ((key_values[2] << 2) | (key_values[1] << 1) | (key_values[0] << 0)); //if these were right next to each other in the same port we could just mask off the three bits.
			if(current_state != previous_state[i]){ //99% of the time they are not going to change
				if(debounce_arr[i] >  debounce_value){
					changed_values = current_state ^ previous_state[i];
					for(int j = 0; j < 3; j ++){
						int bit_mask = 1 << j;
						if(changed_values & bit_mask){
							check_for_change(i, key_values[j], j);
						}
					}
					previous_state[i] = current_state;
					has_unsent_packets = true;
					debounce_arr[i] = 0;
				}else{
					debounce_arr[i]++;
				}
			}else{
					if(debounce_arr[i] > 0){
						debounce_arr[i]--;
					}
			}
			cli();
			CLOCK_HIGH();
			CLOCK_LOW();
			sei();
		}
			dim_leds();
	}
	


	
		void init_color_array(){
			
			for(int i = 0; i < 3; i ++)	{
				colors[base_index + i] = base_color[i];
				colors[caps_index + i] = caps_color[i];
				colors[numbers_index + i] = numbers_color[i];
				colors[function_index + i] = function_color[i];
			}
		}
	
		void shift_init() {
			DDRD |= (1<<DDD4); // set pd4 as output for MODE
			DDRD |= (1<<DDD7); // set pd7 as output for R/W
			DDRB |= (1<<DDB4);// set pb4 as output for CLK
			DDRB |= (1<<DDB5);// set pb5 as output for LED
			DDRB &= ~(1<<DDB6);// set pb6 as input for section C
			DDRB &= ~(1<<DDB7);// set pb7 as input for section B
			DDRD &= ~(1<<DDD6);// set pd6 as input for section A

			CLOCK_HIGH();
			RW_HIGH();

			TCCR0B |= (1 << CS00) | (1 << CS02); // Set up the 1024 prescaler on timer 0
			TCCR0A = (1 << WGM01); // Set the timer to compare mode
			OCR0A = 255; // Set the compare value to max for a lower frequency
			TIMSK0 = (1 << OCIE0A); // Enable timer compare interrupts
			init_color_array();
			fill_leds(base_index);
		}