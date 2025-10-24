/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/i2c.h"

#include "sd_card.h"
#include "ff.h"
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "mcp-io.h"
#include "configure.h"

#define CHIPS_IMPL
#include "6502.c"
#include "6522.h"

#define VERSION "1.2.0"

//#define VIA_BASE_ADDRESS UINT16_C(0xD000)
//#define VIA UINT16_C(0xD000)

// If this is active, then an overclock will be applied
#define OVERCLOCK
// Comment this to run your own ROM
//#define TESTING

// Delay startup by so many seconds
#define START_DELAY 6
// The address at which to put your ROM file
#define ROM_START 0x8000
//Size of your ROM
#define ROM_SIZE 0x8000
// Your rom file in C array format
//#define ROM_FILE "forth.h"
// Variable in which your rom data is stored
//#define ROM_VAR taliforth_pico_bin


//#ifdef VIA_BASE_ADDRESS
m6522_t via;
uint32_t gpio_dirs;
uint32_t gpio_outs;
#define GPIO_PORTA_MASK 0xFF  // PORTB of via is translated to GPIO pins 0 to 7
#define GPIO_PORTB_MASK 0x7F8000  // PORTB of via is translated to GPIO pins 8 to 15
#define GPIO_PORTA_BASE_PIN 0
#define GPIO_PORTB_BASE_PIN 15
//#endif

// Initialize the variable for the ROM
unsigned short rom_start = 0;
unsigned short rom_length = 0;
unsigned short via_location = 0;
unsigned short io_location = 0;
unsigned char *rom = NULL;

uint32_t pa[8];
uint32_t pb[8];
uint32_t pico_b_mask = 0;
uint32_t pico_a_mask = 0;

#ifdef TESTING
#include "65C02_test.h"
#define R_VAR __65C02_extended_opcodes_test_bin
#define R_START 0
#define R_SIZE 0x10000

uint16_t old_pc = 0;
uint16_t old_pc1 = 0;
uint16_t old_pc2 = 0;
uint16_t old_pc3 = 0;
uint16_t old_pc4 = 0;

#else
//#include ROM_FILE
//#define R_VAR ROM_VAR
//#define R_START ROM_START
//#define R_SIZE ROM_SIZE
uint32_t old_ticks = 0;


#endif

struct config_t config;

uint8_t mem[0x10000];
absolute_time_t start;
bool running = true;

uint64_t via_pins = 0;

struct i2c_handle *g_handle = NULL;
int io_available = 0;
uint8_t ddra_value = 0;
uint8_t ddrb_value = 0;

uint8_t porta_value = 0;
uint8_t portb_value = 0;
uint32_t new_pins = 0;



//#ifdef VIA_BASE_ADDRESS

void via_update() {
    // uint8_t pa = M6522_GET_PA(via_pins);
    // uint8_t pb = M6522_GET_PB(via_pins);

    // //
    // // Turn on led when first bit of PB is set
    // gpio_put(PICO_DEFAULT_LED_PIN, pb & 1);

    //printf("pins  %lx\n", (uint32_t)(via_pins & 0XFFFFFFFF));
    //printf("irq   %lx\n", (uint32_t)(M6522_IRQ & 0XFFFFFFFF));
    if ((uint32_t)(via_pins & 0XFFFFFFFF) & (uint32_t)(M6522_IRQ & 0XFFFFFFFF)) {
        //printf("via irq\n");
        irq6502(); 
    }
}
//#endif

// Does not listen to data port
#define FILE_MODE_IDLE 0

// Load a filename
#define FILE_MODE_FILE_NAME 0xff
//
// byte 0 - Length of filename
// byte 1 - First byte
// ...
// byte n - nth byte

// Read the data
#define FILE_MODE_FILE_DATA 0xfe
//
// byte 0 - High byte of length of data
// byte 1 - Low byte of length of data
// byte 2 - First byte
//...
// byte n - nth byte

// Save the data, and go back to idle
#define FILE_MODE_END_DATA 0xfd

// Load the data
#define FILE_MODE_READ_DATA 0Xfc

// Load a catalog of the SD card
#define FILE_MODE_CATALOG 0xfb

// Delete file from the SD card
#define FILE_MODE_DELETE 0xfa

int file_state = FILE_MODE_IDLE;

unsigned short chrout          = 0xf001;
unsigned short file_mode       = 0xf002;
unsigned short file_data       = 0xf003;
unsigned short chrin           = 0xf004;
unsigned short file_load_data  = 0xf005;
unsigned short debug_io_enable = 0xf006;
unsigned short lcd_state       = 0xf007;
unsigned short sound           = 0xf008;

unsigned char filename_length;
unsigned char filename_pos;

unsigned short data_length;
unsigned short data_pos;
bool first_byte = false;

unsigned short load_data_length;
unsigned short load_data_pos;

unsigned char *filename = NULL;
unsigned char *data = NULL;
unsigned char *load_data = NULL;

unsigned char error_status;

//#define DEBUG_IO

int DEBUG_IO = 0;
#define DISPLAY_DEBUG if (DEBUG_IO) printf

#define PORT_A 0
#define PORT_B 1

const uint CTS_PIN = 2;
int xon_xoff = 0;
int32_t freq = 0;
int32_t freq2 = 0;
int32_t freq3 = 0;
repeating_timer_t out_timer;
repeating_timer_t out_timer2;
repeating_timer_t out_timer3;
bool has_timer = false;
bool has_timer2 = false;
bool has_timer3 = false;
uint8_t sound_toggle = 0;
uint8_t sound_toggle2 = 0;
uint8_t sound_toggle3 = 0;

bool timer_callback(repeating_timer_t *rt) {
    sound_toggle = sound_toggle?0:1;
    gpio_put(config.sound1_pin, sound_toggle);
    return true; // Return true to continue the timer
}

bool timer_callback2(repeating_timer_t *rt) {
    sound_toggle2 = sound_toggle2?0:1;
    gpio_put(config.sound2_pin, sound_toggle2);
    return true; // Return true to continue the timer
}

bool timer_callback3(repeating_timer_t *rt) {
    sound_toggle3 = sound_toggle3?0:1;
    gpio_put(config.sound3_pin, sound_toggle3);
    return true; // Return true to continue the timer
}

//***************************************************
// Initialize the I/O board.
//
// gpio_irq - Pico gpio pin for interupt
// 
// Returns 1 if I/O board is initialized, 0 of it is
// not
//***************************************************
int init_io(uint gpio_irq) {
    g_handle = setup_i2c(i2c0, 0x27);

	int result = 0;

	result = setup(g_handle, MIRROR_INTERRUPTS, POLARITY_INTERRUPT_ACTIVE_LOW);
    if (result != 0) result = set_port_direction(g_handle, PORT_A, 0b11111111); // Set to output
    if (result != 0) result = set_port_direction(g_handle, PORT_B, 0b11111111); // Set to output
    if (result != 0) result = set_pullup(g_handle, PORT_A, MCP_ALL_PINS_PULL_UP);
    if (result != 0) result = set_pullup(g_handle, PORT_B, MCP_ALL_PINS_PULL_UP);
    return result == 0?1:0;
}

int8_t crsr = false;

uint16_t start_buffer = 0;
uint16_t end_buffer = 0;
uint16_t buffer_count = 0;
int8_t input_buffer[4096];

//#define OLD_KEY

//***************************************************
// Check if key is proessed. If it is, store it in
// queue. Handle the handshaking if it is turned on
//
// timeout - How long to wait for the keypress
//***************************************************
void poll_keypress(uint32_t timeout) {
#ifndef OLD_KEY
    // Get the key from the keyboard
    int16_t ch = getchar_timeout_us(timeout);

    // Return if it timed out
    if (ch == PICO_ERROR_TIMEOUT) {
        return;
    }

    // Store the key at the end of the queue
    input_buffer[end_buffer] = (uint8_t)ch;
    end_buffer++;
    buffer_count++;

    // Process RTS/CTS, buffer is full
    if (xon_xoff == 0 && buffer_count >= sizeof(input_buffer) - 255) gpio_put(CTS_PIN, 1);

    // Process XON/XOFF, buffer is full
    if (xon_xoff == 1 && buffer_count >= sizeof(input_buffer) - 255) printf("%c",0x13);

    // Buffer overflow. Only get here if flow control is turned off, on terminal
    if (buffer_count >= sizeof(input_buffer)) printf("Overflow!!!!\n");

    if (end_buffer >= sizeof(input_buffer)) end_buffer = 0;
#endif
}

//***************************************************
// Get the next key in the buffer.
//
// Return the key or 0 if there is none
//***************************************************
uint8_t get_key(void) {
#ifdef OLD_KEY
    int16_t ch = getchar_timeout_us(100);
    if (ch == PICO_ERROR_TIMEOUT) {
        return 0;
    }
    return ch;
#else
    if (buffer_count == 0) return 0;
    uint8_t ch = input_buffer[start_buffer];
    start_buffer++;
    buffer_count--;

    // RTS/CTS
    if (xon_xoff == 0 && buffer_count < 255) gpio_put(CTS_PIN, 0);

    // XON/XOFF
    if (xon_xoff == 1 && buffer_count < 255) printf("%c",0x11);

    if (start_buffer >= sizeof(input_buffer)) start_buffer = 0;
    poll_keypress(100);
    return ch;
#endif
}

uint8_t seq[10];

struct config_t *g_config = NULL;

//***************************************************
// Process ANSI escape sequence from input
// 
// Returns a the mapped character, or 0 if not found
//***************************************************
uint8_t process_esc(void) {
    uint8_t cnt = 0;
    uint8_t ch = 0x1b;
    // Process the key input until there is not keypress
    while (ch != 0) {
        seq[cnt++]=ch;
        ch = (uint8_t)get_key();
    }
    seq[cnt++] = 0;

    // Find the seqence in the in map
    for (int i=0; i < 256; i++) {
        // If the is a mapped character
        if (g_config->in_map[i] != 0) {
            // Check if it matches, return the character
            if (strcmp(g_config->in_map[i],seq) == 0) {
                return i;
            }
        }
    }
    // If we didn't find a mapping, return 0
    return 0;
}

//***************************************************
// Read byte from the memory address
//
// address - 16 bit unsigned number for the address
// 
// Returns the byte at that location or a register
//***************************************************
uint8_t read6502(uint16_t address) {
#ifndef TESTING
    // TODO: Make this configurable

    // Get a character from input
    if (address == chrin) {
        int16_t ch = get_key(); // getchar_timeout_us(100);

        if (ch == 0x1b) { // Escape
            ch = process_esc();
        }
        return (uint8_t) ch & 0xFF;

    // Load the next byte of data
    } else if (address == file_load_data) {

        // If there is some data
        if (load_data != NULL) {

            // Get new byte
            unsigned char load_ch = load_data[load_data_pos];

            // Increment the position
            load_data_pos++;

            // If we run out of data, free the data buffer, and set an error.
            if (load_data_pos >= load_data_length + 2) {
                // free buffer
                free(load_data);
                load_data = NULL;
                load_data_pos = 0;

                // Return 0
                load_ch = 0;

                // Set error
                error_status = 3;
            }
            if (DEBUG_IO) {
                printf("[%02x ]", load_ch);
            }

            // Return the byte
            return load_ch; 
        }
        else {
            DISPLAY_DEBUG("++++\n");
            // If we don't have any more data, return 0
            return 0;
        }

    // Return the status of data
    } else if (address == file_data) {
        DISPLAY_DEBUG("Error status: %i\n", error_status);
        return error_status;

    // Return if the LCD is installed or not
    } else if (address == lcd_state) {
        return config.lcd_installed;

    // With the VIA location
    } else if ((address & 0xFFF0) == via_location) {
        if (config.serial_flow == IO_EMULATION_BASIC_EXP) {
            if (io_available) {
                if (via_location != 0 && address == via_location) {
                    uint8_t value;
                    if (io_available) {
                        read_port(g_handle, PORT_B, &value);
                    }
                    else {
                        value = 0;    
                    }
                    return value;
                } else if (via_location != 0 && address == via_location + 1) {
                    uint8_t value;
                    read_port(g_handle, PORT_A, &value);
                    return value;
                } else if (via_location != 0 && address == via_location + 2) {
                    return ddrb_value;
                } else if (via_location != 0 && address == via_location + 3) {
                    return ddrb_value;
                }
            }
            else {
                return 0;
            }

        } else if (config.io_emulation == IO_EMULATION_BASIC_PICO) {
            if (io_available) {
                if (via_location != 0 && address == via_location) {
                    uint8_t value;
                    if (io_available) {
                        read_port(g_handle, PORT_B, &value);
                    }
                    else {
                        value = 0;
                    }
                    return value;
                } else if (via_location != 0 && address == via_location + 1) {
                    uint8_t value;
                    read_port(g_handle, PORT_A, &value);
                    return value;
                } else if (via_location != 0 && address == via_location + 2) {
                    return ddrb_value;
                } else if (via_location != 0 && address == via_location + 3) {
                    return ddrb_value;
                }
            }
            else {
                return 0;
            }


        } else if (config.io_emulation == IO_EMULATION_FULL_EXP) {
            if (io_available) {
                via_pins &= ~(M6522_RS_PINS | M6522_CS2); // clear RS pins - set CS2 low
                // Set via   RW high   set selected  set RS pins
                via_pins |= (M6522_RW | M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));

                via_pins = m6522_tick(&via, via_pins);

                uint8_t value;
                read_port(g_handle, PORT_B, &value);
                if (value != portb_value) {
                    via_pins = via_pins & 0b11111111100000000111111111111111;
                    via_pins |= (value << 15);
                    portb_value = value;
                }

                read_port(g_handle, PORT_A, &value);
                if (value != portb_value) {
                    via_pins = via_pins & 0b111111111111111111111111100000000;
                    via_pins |= value;
                    portb_value = value;
                }

                // Via trigrred IRQ
                uint8_t vdata = M6522_GET_DATA(via_pins);
                //printf("reading from VIA: %04X %02X \n", address, vdata);
                via_update();
                //old_ticks > 0 ? old_ticks-- : 0;
                return vdata;
            }
            else {
                return 0;
            }

        } else if (config.io_emulation == IO_EMULATION_FULL_PICO) {
            if (io_available) {
                via_pins &= ~(M6522_RS_PINS | M6522_CS2); // clear RS pins - set CS2 low
                // Set via   RW high   set selected  set RS pins
                via_pins |= (M6522_RW | M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));

                via_pins = m6522_tick(&via, via_pins);

                uint8_t value;
                //read_port(g_handle, PORT_B, &value);
                if (value != portb_value) {
                    via_pins = via_pins & 0b11111111100000000111111111111111;
                    via_pins |= (value << 15);
                    portb_value = value;
                }

                //read_port(g_handle, PORT_A, &value);
                if (value != portb_value) {
                    via_pins = via_pins & 0b111111111111111111111111100000000;
                    via_pins |= value;
                    portb_value = value;
                }

                // Via trigrred IRQ
                uint8_t vdata = M6522_GET_DATA(via_pins);
                //printf("reading from VIA: %04X %02X \n", address, vdata);
                via_update();
                //old_ticks > 0 ? old_ticks-- : 0;
                return vdata;
            }
            else {
                return 0;
            }
        }

//#endif
    }
#endif
    // Return the memory byte
    return mem[address];
}

//***************************************************
// Scan the files in the specified directory of the
// SD Card.
//
// path - Path to the directory on the SD Card.
// 
// Returns a memory buffer containing the directory
// output. 10k buffer. 
// Todo: Need to make this more dynamic
//***************************************************
unsigned char *scan_files (
    char* path        /* Start node to be scanned (***also used as work area***) */
)
{
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;
    char tmp[255];

    unsigned char *data = NULL;

    // Open the directory
    res = f_opendir(&dir, path);                   /* Open the directory */
    if (res == FR_OK) {
        // Allocate buffer for directory output
        data = malloc(10*1024);

        // Initialize the buffer
        data[0] = 0;
        data[1] = 0;
        data[2] = 0;

        // Read until done
        for (;;) {

            // Read directory item
            res = f_readdir(&dir, &fno);

            // If we have no more items, exit loop
            if (fno.fname[0] == 0) break;

            // If name is . or .., ignore
            if (fno.fname[0] == '.') continue;

            // Get the size of the item
            unsigned int size = fno.fsize;

            // Format the item
            if (fno.fattrib & AM_DIR) {            /* It is a directory */
                sprintf(tmp,"<DIR>  %s\n", fno.fname);
            }
            else {
                sprintf(tmp,"%-6u %s\n", size, fno.fname);
            }

            // Append it to the end of the buffer
            strcat(data+2, tmp);
        }

        // Close the directory
        f_closedir(&dir);

        // Store file size at the start of the buffer, with high byte first
        data[1] = strlen(data+2) & 0xff;
        data[0] = strlen(data+2) >> 8;
    }

    // Return the data
    return data;
}

char new_filename[255];

//***************************************************
// Free the filename, data and load data buffer, and
// set all values to initial values.
//***************************************************
void cleanup_buffers(void) {
    // Clean up the buffers
    if (filename != NULL) {
        free(filename);
        filename = NULL;
    }
    if (data != NULL) {
        free(data);
        data = NULL;
    }
    if (load_data != NULL) {
        free(load_data);
        load_data = NULL;
    }
    load_data_pos = 0;
    first_byte = false;
}

char *mapped = NULL;
int param_cnt = 0;
int params_read = 0;
uint8_t params[10];
char sequence_buffer[30];

uint8_t map_set         = 0;   // Current map set
uint8_t map_set_command = 0;   // Map set command flag
uint8_t *map_set_param  = "|"; // Map set command need a parameter

//***************************************************
// Get number of character in sequence
// 
// seq - Sequence
//
// Returns the number of parameters in a sequence
//***************************************************
int get_params(char *seq) {
    // Start at zero
    int param_count = 0;

    // While we have character
    while(*seq != 0) {
        // If pipe character increment count
        if (*seq == PARAMETER) param_count++;
        seq++;
    }
    return param_count;
}

//***************************************************
// Get the mapped sequence for the value
//
// value - Character to get the map for
// 
// Returns the sequence for the mapped character or
// zero.
//***************************************************
char *get_mapped(unsigned char value) {
    // If character is 0
    if (value == 0) {
        // Set the map set command flag
        map_set_command = 1;

        // Return mapping for getting a parameter
        return map_set_param;
    }

    // Start with 0 mapping
    uint8_t* ch = 0;

    // If other set if greater than 0
    if (map_set > 0) {
        ch = config.out_map[value+(map_set*256)];
    }

    // If map set does not have a map, use the main map.
    if (ch == 0) {
        ch = config.out_map[value];
    }

    // Return the mapping.
    return ch;
}

//***************************************************
// Write a byte to the memory or register
// 
// Address - Address in memory
// value - Value to put in the memory location or
//         register.
//***************************************************
void write6502(uint16_t address, uint8_t value) {
#ifdef TESTING
    mem[address] = value;
    if (address == 0x202) {
        printf("next test is %d\n", value);
    }
#else

    error_status = 0;

    // Output a character
    if (address == chrout) {
        // If we have a sequence with parameters
        if (param_cnt > 0) {
            // Store parameter for later use
            params[params_read] = value;
            params_read++;

            // If we have all the parameters
            if (params_read == param_cnt) {

                // If this is a map set command
                if (map_set_command) {
                    // Set the parameter to the map set
                    map_set = params[0];

                    // Reset the map set command flag.
                    map_set_command = 0;
                }
                else {
                    // Process sequence
                    char buffer[50]; // Buffer for final string
                    char format[50]; // Buffer for format
                    *format = 0;
                    int format_cnt = 0; // Number of parameters
                    for (int i=0; i<strlen(mapped); i++) {
                        // If we have a pipe character (parameter)
                        if (mapped[i] == PARAMETER) {
                            // Added a parameter to the format
                            format[format_cnt++] = '%';
                            format[format_cnt++] = 'i';
                        }
                        else {
                            // Added a character from the mapped sequence
                            format[format_cnt++] = mapped[i];
                        } 
                    }
                    format[format_cnt] = 0;
                    // Produce the final sequence
                    sprintf(buffer, format, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]);
                    // Print the sequence
                    printf("%s", buffer);
                }

                // Reset the parameter count and mapped variables
                param_cnt = 0;
                mapped = NULL;
            }
        }
        else {
            // If we have a mapped parameter
            if (mapped = get_mapped(value)) {
                // Get number of parameters
                param_cnt = get_params(mapped);
                // If there are parameter
                if (param_cnt > 0) {
                    // Set the read count and clear the parameters
                    params_read = 0;
                    for (int i=0; i<sizeof(params); i++) params[i] = 0;
                }
                else {
                    // Just output the mapped character
                    printf("%s", mapped);
                }
            }
            else {
                // Not mapped character
                printf("%c", value);
            }
        }

    } else if (address == sound) {

        if (config.sound1_pin != 0) {
            freq = (freq & 0xff00) | value;
            if (has_timer != 0) {
                cancel_repeating_timer(&out_timer);
                gpio_put(config.sound1_pin, 0);
                has_timer = false;
            }
            if (freq != 0) {
                add_repeating_timer_us((uint32_t)(65536-freq)/2, timer_callback, 0, &out_timer);
                has_timer = true;
            }
        }


    } else if (address == sound+1) {
        if (config.sound1_pin != 0) {
            freq = (freq & 0x00ff) | (value << 8);
            if (has_timer != 0) {
                cancel_repeating_timer(&out_timer);
                gpio_put(config.sound1_pin, 0);
                has_timer = false;
            }
            if (freq != 0) {
                add_repeating_timer_us((uint32_t)(65536-freq)/2, timer_callback, 0, &out_timer);
                has_timer = true;
            }
        }

    } else if (address == sound+2) {

        if (config.sound2_pin != 0) {
            freq2 = (freq2 & 0xff00) | value;
            if (has_timer2 != 0) {
                cancel_repeating_timer(&out_timer2);
                gpio_put(config.sound2_pin, 0);
                has_timer2 = false;
            }
            if (freq2 != 0) {
                add_repeating_timer_us((uint32_t)(65536-freq2)/2, timer_callback2, 0, &out_timer2);
                has_timer2 = true;
            }
        }


    } else if (address == sound+3) {
        if (config.sound2_pin != 0) {
            freq2 = (freq2 & 0x00ff) | (value << 8);
            if (has_timer2 != 0) {
                cancel_repeating_timer(&out_timer2);
                gpio_put(config.sound2_pin, 0);
                has_timer2 = false;
            }
            if (freq2 != 0) {
                add_repeating_timer_us((uint32_t)(65536-freq2)/2, timer_callback2, 0, &out_timer2);
                has_timer2 = true;
            }
        }

    } else if (address == sound+4) {

        if (config.sound3_pin != 0) {
            freq3 = (freq3 & 0xff00) | value;
            if (has_timer3 != 0) {
                cancel_repeating_timer(&out_timer3);
                gpio_put(config.sound3_pin, 0);
                has_timer3 = false;
            }
            if (freq3 != 0) {
                add_repeating_timer_us((uint32_t)(65536-freq3)/2, timer_callback3, 0, &out_timer3);
                has_timer3 = true;
            }
        }

    } else if (address == sound+5) {
        if (config.sound3_pin != 0) {
            freq3 = (freq3 & 0x00ff) | (value << 8);
            if (has_timer3 != 0) {
                cancel_repeating_timer(&out_timer3);
                gpio_put(config.sound3_pin, 0);
                has_timer3 = false;
            }
            if (freq3 != 0) {
                add_repeating_timer_us((uint32_t)(65536-freq3)/2, timer_callback3, 0, &out_timer3);
                has_timer3 = true;
            }
        }


    // Setting the file mode
    } else if (address == file_mode) {
        DISPLAY_DEBUG("File mode: %i\n", value);
        file_state = value;

        // We are done, Save the file and reset the buffers.
        if (file_state == FILE_MODE_END_DATA) {

            // If we don't have a file name
            if (filename == NULL) {
                DISPLAY_DEBUG("No file name\n");
            }

            // Send the file to the sd card
            else {
                filename[filename_pos] = 0;
                DISPLAY_DEBUG("Filename: %s\n", filename);
            }

            // No data
            if (data == NULL) {
                DISPLAY_DEBUG("No data\n");
            }
            else {
                // Zero terminate the buffer
                data[data_pos] = 0;
                DISPLAY_DEBUG("Data: %s\n", data);
            }

            FRESULT fr;
            FATFS fs;
            FIL fil;

            // Mount the file system. Error if can't
            fr = f_mount(&fs, "0:", 1);
            if (fr != FR_OK) {
                DISPLAY_DEBUG("ERROR: Could not mount filesystem\r\n");
                error_status = 5;
                return;
            }

            // If there is a file name
            if (filename != NULL) {
                // Prepend the data directory on the file
                strcpy(new_filename, "data/");
                int replace = 0;
                if (filename[0]=='@') {
                    replace = 1;
                    strcat(new_filename, filename+1);
                }
                else {
                    strcat(new_filename, filename);
                }
                char *dir_path = strdup(new_filename);
                char *last = strrchr(dir_path,'/');
                if (last) *last = 0;
                f_mkdir(dir_path);
                free(dir_path);

                FILINFO fno;

                if (!replace) {
                    fr = f_stat(new_filename, &fno);
                    if (fr == FR_OK) {
                        error_status = 6;
                        return;
                    }
                }


                // Open file for writing ()
                fr = f_open(&fil, new_filename, FA_WRITE | FA_CREATE_ALWAYS);
                if (fr != FR_OK) {
                    DISPLAY_DEBUG("ERROR: Could not open file (%d)\r\n", fr);
                    error_status = 1;
                    return;
                }
                else {

                    // Write the data buffer to the file
                    UINT bw = 0;
                    fr = f_write (&fil, data, data_pos, &bw);

                    // Close file
                    fr = f_close(&fil);
                    if (fr != FR_OK) {
                        DISPLAY_DEBUG("ERROR: Could not close file (%d)\r\n", fr);
                    }
                }

                // Unmount drive
                f_unmount("0:");
            }

            cleanup_buffers();
            load_data_pos = 0;
            file_state = 0;
            first_byte = false;
        }
        // If 0, force to idle mode
        if (file_state == FILE_MODE_IDLE) {
            cleanup_buffers();
            error_status = 0;
        }

        // Delete a file
        if (file_state == FILE_MODE_DELETE) {
            FRESULT fr;
            FATFS fs;
            FIL fil;

            // If no file
            if (filename == NULL) {
                DISPLAY_DEBUG("No file name\n");
            }
            // Terminate the file name buffer with 0
            else {
                filename[filename_pos] = 0;
                DISPLAY_DEBUG("Filename: %s\n", filename);
            }

            // Mount the file system, error if can not
            fr = f_mount(&fs, "0:", 1);
            if (fr != FR_OK) {
                DISPLAY_DEBUG("ERROR: Could not mount filesystem\r\n");
                error_status = 5;
                return;
            }

            // If we have a file name
            if (filename != NULL) {

                // Prepend data directory to file name
                strcpy(new_filename, "data/");
                strcat(new_filename, filename);
                DISPLAY_DEBUG("Filename: %s\n",new_filename);

                // Unlink the file. Error if can't
                fr = f_unlink(new_filename);
                if (fr == FR_OK) {
                    error_status = 0;
                } else {
                    error_status = 6;
                }

                // Unmount drive
                f_unmount("0:");
            }
            else {
                error_status = 4;
            }

            // Clean up
            cleanup_buffers();
        }

        if (file_state == FILE_MODE_READ_DATA) {

            FRESULT fr;
            FATFS fs;
            FIL fil;

            // No file name
            if (filename == NULL) {
                DISPLAY_DEBUG("No file name\n");
            }
            else {
                // Terminate filename buffer with zero
                filename[filename_pos] = 0;
                DISPLAY_DEBUG("Filename: %s\n", filename);
            }

            // Mount file system. Error if can't
            fr = f_mount(&fs, "0:", 1);
            if (fr != FR_OK) {
                DISPLAY_DEBUG("ERROR: Could not mount filesystem\r\n");
                error_status = 5;
                return;
            }

            // If we have a filename
            if (filename != NULL) {
                // Prepend data directory to filename
                strcpy(new_filename, "data/");
                strcat(new_filename, filename);

                DISPLAY_DEBUG("Filename: %s\n",new_filename);

                // Open file for Reading ()
                fr = f_open(&fil, new_filename, FA_READ);
                if (fr != FR_OK) {
                    DISPLAY_DEBUG("ERROR: Could not open file (%d)\r\n", fr);
                    error_status = 1;
                }
                else {

                    // Get the size of the file
                    load_data_length = f_size(&fil);
                    DISPLAY_DEBUG("File Length: %i\n",load_data_length);

                    // Allocate memory for file data
                    load_data = malloc(load_data_length + 2);

                    // Store the length at the beginning of the buffer, hight byte first
                    load_data[1] = load_data_length & 0xff;
                    load_data[0] = load_data_length >> 8;

                    // Read the file from the SD card
                    UINT br = 0;
                    fr = f_read (&fil, load_data+2, load_data_length, &br);
                    DISPLAY_DEBUG("Bytes read: %i\n",br);

                    // Close file
                    fr = f_close(&fil);
                    if (fr != FR_OK) {
                        DISPLAY_DEBUG("ERROR: Could not close file (%d)\r\n", fr);
                        error_status = 1;
                    }
                }

                // Unmount drive
                f_unmount("0:");

                unsigned char ld_chr; 
                for (int i; i < load_data_length + 2; i++) {
                    ld_chr = load_data[i];
                    DISPLAY_DEBUG("%02x ", ld_chr);
                }
                DISPLAY_DEBUG("\n");
            }
            else {
                error_status = 4;
            }
        }

        if (file_state == FILE_MODE_CATALOG) {

            FRESULT fr;
            FATFS fs;
            FIL fil;

            // Mount file system. Error if can't
            fr = f_mount(&fs, "0:", 1);
            if (fr != FR_OK) {
                DISPLAY_DEBUG("ERROR: Could not mount filesystem\r\n");
                error_status = 5;
            }
            else {
                // Read the files on SD card, and return a buffer, with
                // the list

                strcpy(new_filename, "data/");

                if (filename) {
                    strcat(new_filename,filename);
                }
                DISPLAY_DEBUG("\nFilename: %s\n", filename);
                DISPLAY_DEBUG("\nFile Path: %s\n", new_filename);
                unsigned char *x = scan_files(new_filename);
                DISPLAY_DEBUG(x+2);
                if (load_data != NULL) {
                    free(load_data);
                    load_data = NULL;
                    load_data_pos = 0;
                }

                load_data = x;
                load_data_length = strlen(load_data+2);
            }

            // Unmount drive
            f_unmount("0:");
        }
 
    // Sending the data
    } else if (address == file_data) {
        DISPLAY_DEBUG("File data: %i - File mode: %i\n", value, file_state);

        // If sending the filename
        if (file_state == FILE_MODE_FILE_NAME) {
            if (filename == NULL) {
                // If filename does not exist, allocate it
                filename_length = value ;
                filename = malloc(filename_length + 1);
                filename_pos = 0;
                filename[filename_length] = 0;
            }
            else {
                // Fill in the filename until we have reached the size.
                if (filename_pos != filename_length) {
                    filename[filename_pos] = value;
                    filename_pos++;
                }
            }
        }

        // If sending the data
        if (file_state == FILE_MODE_FILE_DATA) {
            if (data == NULL) {
                // If we do not has a data buffer, get the first byte
                // of the length if it is not set, if it is get the second 
                // byte of the length.
                if (!first_byte) {
                    data_length = value << 8;
                    first_byte = true;
                }
                else {
                    // If we have the first byte, get the second byte and
                    // allocate the data buffer.
                    data_length = data_length | value;
                    data = malloc(data_length + 1);
                    data_pos = 0;
                }
            }
            else {
                // Fill in the data bufer until we have reached the size.
                if (data_pos != data_length) {
                    data[data_pos] = value;
                    data_pos++;
                }
            }
        }
    // Enable debug. Any non-zero value
    } else if (address == debug_io_enable) {
        DEBUG_IO = value;

    } else if ((address & 0xFFF0) == via_location) {
        if (config.io_emulation == IO_EMULATION_BASIC_EXP) {
            if (config.io_emulation == 5) { // Expansion Board
                if (via_location != 0 && address == via_location) {
                    write_port(g_handle, PORT_B, value);
                } else if (via_location != 0 && address == (via_location + 1)) {
                    write_port(g_handle, PORT_A, value);
                } else if (via_location != 0 && address == (via_location + 2)) {
                    ddrb_value = value;
                    set_port_direction(g_handle, PORT_B, value);
                } else if (via_location != 0 && address == (via_location + 3)) {
                    ddra_value = value;
                    set_port_direction(g_handle, PORT_A, value);
                }
            }
            else {
            }

        } else if (config.io_emulation == IO_EMULATION_BASIC_PICO) {
            if (config.io_emulation == 3) { // Pico GPIO
                if (via_location != 0 && address == via_location) {
                    gpio_outs &= ~((uint32_t)pico_b_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_outs |= pb[i];
                        }
                        value = value >> 1;
                    }
                    DISPLAY_DEBUG("Port B: %08x %08x\n", gpio_dirs, gpio_outs);
                    gpio_put_masked(gpio_dirs, gpio_outs);

                } else if (via_location != 0 && address == (via_location + 1)) {
                    gpio_outs &= ~((uint32_t)pico_a_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_outs |= pa[i];
                        }
                        value = value >> 1;
                    }
                    DISPLAY_DEBUG("Port A: %08x %08x\n", gpio_dirs, gpio_outs);
                    gpio_put_masked(gpio_dirs, gpio_outs);

                } else if (via_location != 0 && address == (via_location + 2)) {
                    gpio_dirs &= ~((uint32_t)pico_b_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_dirs |= pb[i];
                        }
                        value = value >> 1;
                    }
                    gpio_set_dir_all_bits(gpio_dirs);

                } else if (via_location != 0 && address == (via_location + 3)) {
                    gpio_dirs &= ~((uint32_t)pico_a_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_dirs |= pa[i];
                        }
                        value = value >> 1;
                    }
                    gpio_set_dir_all_bits(gpio_dirs);
                }
            }
            else {
            }
        } else if (config.io_emulation == IO_EMULATION_FULL_EXP) {
    
            if (io_available) {
                //printf("writing to VIA %04X val: %02X\n", address, value);
                via_pins &= ~(M6522_RW | M6522_RS_PINS | M6522_CS2); // SET RW pin low to write - clear data pins - clear RS pins
                // Set via selected      set RS pins                 set data pins
                via_pins |= (M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));
                M6522_SET_DATA(via_pins, value);

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRB) {
                    // Setting DDRB / Set pins to in/output
                    gpio_dirs &= ~((uint32_t)GPIO_PORTB_MASK);
                    gpio_dirs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
                    //gpio_set_dir_all_bits(gpio_dirs);
                    uint8_t db = (uint8_t)((gpio_dirs >> 15) & 0xff);
                    set_port_direction(g_handle, PORT_B, db);
                }


                // 0x 7F 80 00
                // 0000|0000 0111|1111 1000|0000 0000|0000

                // 0x0000FF
                // 0000|0000 0000|0000 0000|0000 1111|1111

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RB) {
                    // Setting DDRB / Set pins to in/output
                    gpio_outs &= ~((uint32_t)GPIO_PORTB_MASK);
                    gpio_outs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
                    //gpio_put_masked(gpio_dirs, gpio_outs);
                    uint8_t db = (uint8_t)((gpio_dirs >> 15) & 0xff);
                    uint8_t pb = (uint8_t)((gpio_outs >> 15) & 0xff);
                    //printf("Port B: D %02x - P %02x\n",db, pb);
                    write_port(g_handle, PORT_B, pb);

                }

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRA) {
                    // Setting DDRB / Set pins to in/output
                    gpio_dirs &= ~((uint32_t)GPIO_PORTA_MASK);
                    gpio_dirs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
                    //gpio_set_dir_all_bits(gpio_dirs);

                    uint8_t da = (uint8_t)(gpio_dirs & 0xff);
                    //printf("DDR A: %02x\n",da);
                    set_port_direction(g_handle, PORT_A, da);
                }

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RA) {
                    // Setting DDRB / Set pins to in/output
                    gpio_outs &= ~((uint32_t)GPIO_PORTA_MASK);
                    gpio_outs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
                    //gpio_put_masked(gpio_dirs, gpio_outs);
                    uint8_t da = (uint8_t)(gpio_dirs & 0xff);
                    uint8_t pa = (uint8_t)(gpio_outs & 0xff);
                    //printf("Port A: D %02x - P %02x\n", da, pa);
                    write_port(g_handle, PORT_A, pa);
                }

                via_pins = m6522_tick(&via, via_pins);

                via_update();
                //old_ticks > 0 ? old_ticks-- : 0;
            }
            else {
                // Do nothing
            }


        } else if (config.io_emulation = IO_EMULATION_FULL_PICO) {
    
            if (io_available) {
                //printf("writing to VIA %04X val: %02X\n", address, value);
                via_pins &= ~(M6522_RW | M6522_RS_PINS | M6522_CS2); // SET RW pin low to write - clear data pins - clear RS pins
                // Set via selected      set RS pins                 set data pins
                via_pins |= (M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));
                M6522_SET_DATA(via_pins, value);

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRB) {
                    // Setting DDRB / Set pins to in/output
                    gpio_dirs &= ~((uint32_t)GPIO_PORTB_MASK);
                    gpio_dirs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
                    //gpio_set_dir_all_bits(gpio_dirs);
                    uint8_t db = (uint8_t)((gpio_dirs >> 15) & 0xff);
                    //printf("DDR B: %02x\n",db);
                    set_port_direction(g_handle, PORT_B, db);
                }


                // 0x 7F 80 00
                // 0000|0000 0111|1111 1000|0000 0000|0000

                // 0x0000FF
                // 0000|0000 0000|0000 0000|0000 1111|1111

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RB) {
                    // Setting DDRB / Set pins to in/output
                    gpio_outs &= ~((uint32_t)GPIO_PORTB_MASK);
                    gpio_outs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
                    //gpio_put_masked(gpio_dirs, gpio_outs);
                    uint8_t db = (uint8_t)((gpio_dirs >> 15) & 0xff);
                    uint8_t pb = (uint8_t)((gpio_outs >> 15) & 0xff);
                    //printf("Port B: D %02x - P %02x\n",db, pb);
                    write_port(g_handle, PORT_B, pb);

                }

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRA) {
                    // Setting DDRB / Set pins to in/output
                    gpio_dirs &= ~((uint32_t)GPIO_PORTA_MASK);
                    gpio_dirs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
                    //gpio_set_dir_all_bits(gpio_dirs);

                    uint8_t da = (uint8_t)(gpio_dirs & 0xff);
                    //printf("DDR A: %02x\n",da);
                    set_port_direction(g_handle, PORT_A, da);
                }

                if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RA) {
                    // Setting DDRB / Set pins to in/output
                    gpio_outs &= ~((uint32_t)GPIO_PORTA_MASK);
                    gpio_outs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
                    //gpio_put_masked(gpio_dirs, gpio_outs);
                    uint8_t da = (uint8_t)(gpio_dirs & 0xff);
                    uint8_t pa = (uint8_t)(gpio_outs & 0xff);
                    //printf("Port A: D %02x - P %02x\n", da, pa);
                    write_port(g_handle, PORT_A, pa);
                }

                via_pins = m6522_tick(&via, via_pins);

                via_update();
                //old_ticks > 0 ? old_ticks-- : 0;
            }
            else {
                // Do nothing
            }
        }
    } else {
        // If address is ROM, don't update the memory
        if (address < rom_start) {
            mem[address] = value;
        }
    }
#endif
}

void callback() {
    #ifdef TESTING
        if ((pc == old_pc) && (old_pc == old_pc1)) {

            if (clockticks6502%1000000 == 0) {
                absolute_time_t now = get_absolute_time();
                int64_t elapsed = absolute_time_diff_us(start, now);

                float khz = (double)clockticks6502 / (double)(elapsed);

                printf("Average emulated speed was %.3f MHz\n", khz);
            }

            if (old_pc == 0x24F1) {
                printf("65C02 test suite passed sucessfully!\n\n");

                absolute_time_t now = get_absolute_time();
                int64_t elapsed = absolute_time_diff_us(start, now);

                float khz = (double)clockticks6502 / (double)(elapsed);

                printf("Average emulated speed was %.3f MHz\n", khz);
                printf("Average emulated speed was %.3f MHz\n", khz);

            } else {
                printf("65C02 test suite failed\n");
                printf("pc %04X opcode: %02X test: %d status: %02X \n", old_pc, opcode, mem[0x202], status);
                printf("a %02X x: %02X y: %02X value: %02X \n\n", a, x, y, value);
            }

            running= false;

        }
        old_pc4 = old_pc3;
        old_pc3 = old_pc2;
        old_pc2 = old_pc1;
        old_pc1 = old_pc;
        old_pc = pc;
    #endif
    //if (clockticks6502 % 100 == 0) {
        // absolute_time_t now = get_absolute_time();
        // int64_t elapsed = absolute_time_diff_us(start, now);

        // float khz = (float)clockticks6502 / (float)(elapsed/1000.0);

        // printf("kHz %.2f\n", khz);
    //}

#ifdef VIA_BASE_ADDRESS
    // one tick for each clock to keep accurate time
    for (uint16_t i = 0; i < clockticks6502-old_ticks; i++) {
        via_pins = m6522_tick(&via, via_pins);
    }

    via_update();

    old_ticks = clockticks6502;
#endif

}

void setup_config(void) {
    printf("\n");
    gpio_init(CTS_PIN);
    gpio_set_dir(CTS_PIN   , GPIO_IN);
    gpio_pull_up(CTS_PIN);

    if (config.sound1_pin != 0) {
        gpio_init(config.sound1_pin);
        gpio_set_dir(config.sound1_pin   , GPIO_OUT);
        gpio_pull_up(config.sound1_pin);
    }

    if (config.sound2_pin != 0) {
        gpio_init(config.sound2_pin);
        gpio_set_dir(config.sound2_pin   , GPIO_OUT);
        gpio_pull_up(config.sound2_pin);
    }

    if (config.sound3_pin != 0) {
        gpio_init(config.sound3_pin);
        gpio_set_dir(config.sound3_pin   , GPIO_OUT);
        gpio_pull_up(config.sound3_pin);
    }

    if (config.serial_flow == FLOW_CONTROL_AUTO) {
        printf("Flow Control\n");
        xon_xoff = !gpio_get(CTS_PIN);
        if (xon_xoff) {
            // If pin is grounded, we use XON/XOFF
            printf("Using XON/XOFF Flow Control\n");
        }
        else {
            // If pin is not ground, we use RTS/CTS
            printf("Using RTS/CTS Flow Control\n");
            gpio_pull_down(CTS_PIN);
            gpio_set_dir(CTS_PIN, GPIO_OUT);
        }
    }
    else if (config.serial_flow == FLOW_CONTROL_NONE) {
            printf("Configured No Serial Flow Control\n");
    }
    else if (config.serial_flow == FLOW_CONTROL_RTS_CTS) {
            printf("Configured RTS/CTS Flow Control\n");
    }
    else if (config.serial_flow == FLOW_CONTROL_XON_XOFF) {
            printf("Configured XON/XOFF Flow Control\n");
    }

    if (config.io_emulation == IO_EMULATION_AUTO || 
        config.io_emulation == IO_EMULATION_FULL_EXP || 
        config.io_emulation == IO_EMULATION_BASIC_EXP) { // Auto
        printf("Initializing IO\n");
        io_available = init_io(MCP_IRQ_GPIO_PIN);
        if (io_available) {
            printf("I/O Available (Expansion Board)\n");
            config.io_emulation = IO_EMULATION_BASIC_EXP;
        }
        else {
            // If we get an error initializing, not available
            printf("No I/O Available (Expansion Board)\n");
        }
    }
    else if (config.io_emulation == IO_EMULATION_FULL_PICO || 
            config.io_emulation == IO_EMULATION_BASIC_PICO) {
        printf("Configured I/O Available (Pico GPIO)\n");
        io_available = 1;
    }
    else if (config.io_emulation == IO_EMULATION_NONE) {
        printf("Configured No I/O Available\n");
    }
    printf("\n");

    if (config.io_emulation == IO_EMULATION_FULL_PICO || 
        config.io_emulation == IO_EMULATION_BASIC_PICO) {
        uint32_t pins = config.pico_pins;
        int cnt = 0;
        for (int i = 0; i < 8; i++) {
            while (cnt < 32 && (pins & 1) == 0) {
                cnt++;
                pins = pins >> 1;
            }
            if (cnt >= 32) break;
            pa[i] = (int)pow((double)2,(double)cnt);
            //printf("pa[%i]=%08x\n", i, pa[i]);
            cnt++;
            pins = pins >> 1;
        }

        for (int i = 0; i < 8; i++) {
            while (cnt < 32 && (pins & 1) == 0) {
                cnt++;
                pins = pins >> 1;
            }
            if (cnt >= 32) break;
            pb[i] = (int)pow((double)2,(double)cnt);
            //printf("pb[%i]=%08x\n", i, pb[i]);
            cnt++;
            pins = pins >> 1;
        }

        pico_a_mask = pa[0] | pa[1] | pa[2] | pa[3] | pa[4] | pa[5] | pa[6] | pa[7];
        pico_b_mask = pb[0] | pb[1] | pb[2] | pb[3] | pb[4] | pb[5] | pb[6] | pb[7];    
    }
}

void set_sd_card_pins(uint8_t spi, uint8_t miso, uint8_t sck, uint8_t mosi);

int main() {
#ifdef OVERCLOCK
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(280000, true);
#endif

    stdio_init_all();

    sleep_ms(2000);
    printf("Version: %s\n",VERSION);
#ifdef OVERCLOCK
    printf("Overclocked\n");
#endif

    FATFS fs;
    FRESULT fr;
    char buf[100];

//    uint16_t freq = 53402; //63512; //63264;

    //add_repeating_timer_us	((uint32_t)(65536-freq)/2, timer_callback, 0, &out_timer);
    // Wait for user to press 'ENTER' to continue
    printf("\r\nPress 'ENTER' to start.\r\n");
    while (true) {
        buf[0] = getchar();
        if ((buf[0] == '\r') || (buf[0] == '\n')) {
            break;
        }
    }

    printf("\n");

    printf("Initializing SD card ...\n");
    set_sd_card_pins(0, 4, 6, 7);
    sd_init_driver();

    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) {
        printf("ERROR: Could not mount filesystem\r\n");
        while(true) {}
    }
    else {
        unsigned char *config_file = config_menu();

        printf("\n");
        g_config = &config;
        printf("Reading Configuration ...\n");
        if (read_config(config_file, &config) != 0) {
            printf("Error in reading configuration file.\n");
        }

        free(config_file);

        setup_config();

        printf("\n");
        printf("Reading ROMs ...\n");

        // Select the ROM
        rom = select_menu(&config, &rom_start, &rom_length, &via_location, &io_location);

        // Unmount drive
        f_unmount("0:");
    }

    if (rom == NULL) {
        printf("Loading ROM failed!\n");
        while(true) {}
    }

//    for(uint8_t i = START_DELAY; i > 0; i--) {
//        printf("Starting in %d \n", i);
//        sleep_ms(1000);
//    }

//    printf("Starting\n");

//   if (R_START + R_SIZE > 0x10000) {
//        printf("Your rom will not fit. Either adjust ROM_START or ROM_SIZE\n");
//        while(1) {}
//    }
//    for (int i = R_START; i < R_SIZE + R_START; i++) {
//        mem[i] = R_VAR[i-R_START];
//    }

    chrout          = io_location + 1; // 0xf001;
    file_mode       = io_location + 2; // 0xf002;
    file_data       = io_location + 3; // 0xf003;
    chrin           = io_location + 4; // 0xf004;
    file_load_data  = io_location + 5; // 0xf005;
    debug_io_enable = io_location + 6; // 0xf006;

    printf("ROM Start   : %04x (%i)\n",rom_start, rom_start);
    printf("ROM Length  : %04x (%i)\n",rom_length, rom_length);
    printf("VIA Location: %04x (%i)\n",via_location, via_location);
    printf("I/O Location: %04x (%i)\n",io_location, io_location);

    printf("Executing ROM\n");

    // Check if ROM will fit in memory
    if (rom_length + rom_start > 0x10000) {
        printf("Your rom will not fit. Either adjust ROM start or ROM size\n");
        while(1) {}
    }

    // Store the ROM in memory
    for (int i = rom_start; i < rom_length + rom_start; i++) {
        mem[i] = rom[i-rom_start];
    }

    // Free the ROM. 
    free(rom);

    hookexternal(callback);
    reset6502();
//#ifdef VIA_BASE_ADDRESS

    if (config.io_emulation == IO_EMULATION_FULL_PICO) {
        // setup VIA
        m6522_init(&via);
        m6522_reset(&via);
        gpio_dirs = 0; //GPIO_PORTB_MASK | GPIO_PORTA_MASK;
        gpio_outs = 0;
        // Init GPIO
        // Set pins 0 to 7 as output as well as the LED, the others as input
        gpio_init_mask(gpio_dirs);
        gpio_set_dir_all_bits(gpio_dirs);
    }
    else if (config.io_emulation == IO_EMULATION_FULL_PICO) {
        gpio_dirs = pico_a_mask|pico_b_mask;
        gpio_set_dir_all_bits(gpio_dirs);
    }

//#endif

#ifdef TESTING
    pc = 0X400;
#endif
    start = get_absolute_time();

    while (running) {
        step6502();
        poll_keypress(0);
    }

    free_i2c(g_handle);
    g_handle = NULL;
    return 0;
}
