/*
 * IncFile1.h
 *
 * Created: 3/15/2022 11:22:30 AM
 *  Author: alden
 */ 


#ifndef SHIFT_H_
#define SHIFT_H_
  #include <stdbool.h>

  #define NUM_ROWS 4
  #define NUM_COLS 12
  #define B_OFFSET 4  // PORTB does not start at 0, 0-3 is used for ICSP
  #define Clk_Bit_Low 0xBF
  #define Clk_Bit_High 0x40
  #define Shift_Bit_High 0x80
  #define Shift_Bit_Low 0x7F
  void shift_init();
  void add_key_to_HID();
  void remove_key_from_HID();
  void shift();
#endif /* SHIFT_H_ */