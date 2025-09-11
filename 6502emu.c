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

#define CHIPS_IMPL
#include "6502.c"
#include "6522.h"

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

char roms_txt[20];
int serial_flow = 0;
int io_emulation = 0;
int lcd_installed = 0;
uint32_t pico_pins = 0x0000;
uint32_t pa[8];
uint32_t pb[8];
uint32_t pico_b_mask = 0;
uint32_t pico_a_mask = 0;

#define IO_EMULATION_AUTO       0
#define IO_EMULATION_NONE       1
#define IO_EMULATION_FULL_PICO  2
#define IO_EMULATION_BASIC_PICO 3
#define IO_EMULATION_FULL_EXP   4
#define IO_EMULATION_BASIC_EXP  5

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

#define PORTB 0xD000
#define PORTA 0xD001
#define DDRB  0xD002
#define DDRA  0xD003

#define PORT_A 0
#define PORT_B 1

const uint CTS_PIN = 2;
int xon_xoff = 0;

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

// {5b}{31}{7e}~ Home
// {5b}{32}{7e}~ Insert
// {5b}{33}{7e}~ Del
// {5b}{34}{7e}~ End
// {5b}{35}{7e}~ Page Up
// {5b}{36}{7e}~ Page Down

// {5b}{41} - Cursor Up
// {5b}{42} - Cursor Down
// {5b}{43} - Cursor Right
// {5b}{44} - Cursor Left

int8_t crsr = false;

uint16_t start_buffer = 0;
uint16_t end_buffer = 0;
uint16_t buffer_count = 0;
int8_t input_buffer[1024];

//#define OLD_KEY

void poll_keypress(uint32_t timeout) {
#ifndef OLD_KEY
    int16_t ch = getchar_timeout_us(timeout);
    if (ch == PICO_ERROR_TIMEOUT) {
        return;
    }
    input_buffer[end_buffer] = (uint8_t)ch;
    end_buffer++;
    buffer_count++;
    if (xon_xoff == 0 && buffer_count >= sizeof(input_buffer) - 255) gpio_put(CTS_PIN, 1);
    if (xon_xoff == 1 && buffer_count >= sizeof(input_buffer) - 255) printf("%c",0x13);//printf("%c", 0x13); // Ctrl+S
//    if (buffer_count >= sizeof(input_buffer) - 255) printf("%c",0x13);//printf("%c", 0x13); // Ctrl+S
    if (buffer_count >= sizeof(input_buffer)) printf("Overflow!!!!\n");
    if (end_buffer >= sizeof(input_buffer)) end_buffer = 0;
#endif
}

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
    if (xon_xoff == 0 && buffer_count < 255) gpio_put(CTS_PIN, 0);
    if (xon_xoff == 1 && buffer_count < 255) printf("%c",0x11); // printf("%c",0x11); // Ctrl+Q
    //if (buffer_count < 255) printf("%c",0x11); // printf("%c",0x11); // Ctrl+Q
    if (start_buffer >= sizeof(input_buffer)) start_buffer = 0;
    poll_keypress(100);
    return ch;
#endif
}

uint8_t read6502(uint16_t address) {
#ifndef TESTING
    // Get a character from input
    if (address == chrin) {
        int16_t ch = get_key(); // getchar_timeout_us(100);
        if (ch == 0) {
           return 0;
        }
        if (ch == 0x1b) { // Escape
            // Check if key follows
            ch = get_key(); // getchar_timeout_us(100);
            if (ch == 0) {
                return 0;
            }
            if (ch == 0x5b) { // Special key
                ch = get_key(); // getchar_timeout_us(100);
                if (ch == 0) {
                    return 0;
                }

                switch (ch) {
                    case 0x31: // Home
                        ch = 0x13;
                        crsr = false;
                        break;
                    case 0x32: // Insert
                        ch = 0x94;
                        crsr = false;
                        break;
                    case 0x33: // Del
                        ch = 0x7f;
                        crsr = false;
                        break;
                    case 0x34: // End
                        ch = 0x93;
                        crsr = false;
                        break;
                    case 0x35: // Page Up
                        ch = 0x00;
                        crsr = false;
                        break;
                    case 0x36: // Page Down
                        ch = 0x00;
                        crsr = false;
                        break;

                    case 0x41: // Cursor Up
                        ch = 0; //0x91;
                        crsr = true;
                        break;
                    case 0x42: // Cursor Down
                        ch = 0; //0x9d;
                        crsr = true;
                        break;
                    case 0x43: // Cursor Right
                        ch = 0x1d;
                        crsr = true;
                        break;
                    case 0x44: // Cursor Left
                        ch = 0x9d;
                        crsr = true;
                        break;
                };

                if (!crsr) {
                    uint16_t ch2 = get_key(); // getchar_timeout_us(100);
                    if (ch2 == 0) {
                        return 0;
                    }

                    if (ch = 0x31 && ch2 == 0x31) { // F1
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x31) { // F1
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x31) { // F1
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x32) { // F2
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x33) { // F3
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x34) { // F4
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x35) { // F5
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x36) { // F6
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x37) { // F7
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x38) { // F8
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x31 && ch2 == 0x39) { // F9
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x33 && ch2 == 0x31) { // F10
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }

                    if (ch = 0x32 && ch2 == 0x33) { // F11
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x00;
                    }
                    if (ch = 0x32 && ch2 == 0x34) { // F12
                        uint16_t ch3 = get_key(); // getchar_timeout_us(100);
                        ch == 0x94;
                    }
                }
            }
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

    } else if (address == file_data) {
        DISPLAY_DEBUG("Error status: %i\n", error_status);
        return error_status;

    } else if (address == lcd_state) {
        //printf("lcd state: %i\n", lcd_installed);
        return lcd_installed;
    } else if ((address & 0xFFF0) == via_location) {
        if (io_emulation == IO_EMULATION_BASIC_EXP) {
            if (io_available) {
                if (via_location != 0 && address == PORTB) {
                    uint8_t value;
                    if (io_available) {
                        read_port(g_handle, PORT_B, &value);
                    }
                    else {
                        value = 0;    
                    }
                    return value;
                } else if (via_location != 0 && address == PORTA) {
                    uint8_t value;
                    read_port(g_handle, PORT_A, &value);
                    return value;
                } else if (via_location != 0 && address == DDRB) {
                    return ddrb_value;
                } else if (via_location != 0 && address == DDRA) {
                    return ddrb_value;
                }
            }
            else {
                return 0;
            }

        } else if (io_emulation == IO_EMULATION_BASIC_PICO) {
            if (io_available) {
                if (via_location != 0 && address == PORTB) {
                    uint8_t value;
                    if (io_available) {
                        //read_port(g_handle, PORT_B, &value);
                    }
                    else {
                        value = 0;    
                    }
                    return value;
                } else if (via_location != 0 && address == PORTA) {
                    uint8_t value;
                    //read_port(g_handle, PORT_A, &value);
                    return value;
                } else if (via_location != 0 && address == DDRB) {
                    return ddrb_value;
                } else if (via_location != 0 && address == DDRA) {
                    return ddrb_value;
                }
            }
            else {
                return 0;
            }


        } else if (io_emulation == IO_EMULATION_FULL_EXP) {
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

        } else if (io_emulation == IO_EMULATION_FULL_PICO) {
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
    return mem[address];
}

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

            // Format item
            //sprintf(tmp,"%-20s  %10u\n", fno.fname, size);
            sprintf(tmp,"%-6u %s\n", size, fno.fname);

            // Append it to the end of the buffer
            strcat(data+2, tmp);
            //printf("%-20s  %10u\n", fno.fname, fno.fsize);
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
unsigned char *char_map[256];

char *get_mapped(unsigned char value) {
    return char_map[value];
}

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
        if (mapped = get_mapped(value)) {
            printf("%s", mapped);
        }
        else {
            printf("%c", value);
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
                unsigned char *x = scan_files("/data");
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
        if (io_emulation == IO_EMULATION_BASIC_EXP) {
            if (io_emulation == 5) { // Expansion Board
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

        } else if (io_emulation == IO_EMULATION_BASIC_PICO) {
            if (io_emulation == 3) { // Pico GPIO
                if (via_location != 0 && address == via_location) {
                    gpio_outs &= ~((uint32_t)pico_b_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_outs |= pb[i];
                            value = value >> 1;
                        }
                    }
                    DISPLAY_DEBUG("Port B: %08x %08x\n", gpio_dirs, gpio_outs);
                    gpio_put_masked(gpio_dirs, gpio_outs);

                } else if (via_location != 0 && address == (via_location + 1)) {
                    gpio_outs &= ~((uint32_t)pico_a_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_outs |= pa[i];
                            value = value >> 1;
                        }
                    }
                    DISPLAY_DEBUG("Port A: %08x %08x\n", gpio_dirs, gpio_outs);
                    gpio_put_masked(gpio_dirs, gpio_outs);

                } else if (via_location != 0 && address == (via_location + 2)) {
                    gpio_dirs &= ~((uint32_t)pico_b_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_dirs |= pb[i];
                            value = value >> 1;
                        }
                    }
                    gpio_set_dir_all_bits(gpio_dirs);

                } else if (via_location != 0 && address == (via_location + 3)) {
                    gpio_dirs &= ~((uint32_t)pico_a_mask);
                    for (int i = 0; i < 8; i++) {
                        if (value & 1) {
                            gpio_dirs |= pa[i];
                            value = value >> 1;
                        }
                    }
                    gpio_set_dir_all_bits(gpio_dirs);
                }
            }
            else {
            }
        } else if (io_emulation == IO_EMULATION_FULL_EXP) {
    
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


        } else if (io_emulation = IO_EMULATION_FULL_PICO) {
    
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

char *trim(char *s) {
    char *ptr;
    if (!s)
        return NULL;   // handle NULL string
    if (!*s)
        return s;      // handle empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

unsigned short convert_to_short_int(char *number) {
    if (number[0]=='$') {
        return (unsigned short)strtol(number+1, NULL, 16);
    }
    else {
        return (unsigned short)strtol(number, NULL, 10);
    }
    return 0;
}


unsigned char *select_menu(unsigned short  *start, unsigned short *length, unsigned short *via, unsigned short *io) {
    FRESULT fr;
    FATFS fs;
    FIL fil;
    FILINFO fno;
    DIR dp;
    int ret;
    char buf[255];

    unsigned char * data = NULL;

    char *menu_items = "123456789ABCDEFGHIJKLMNOPQRSTUVWYZ";

#define ROM_COUNT 36

    // Open file for reading
    fr = f_open(&fil, roms_txt, FA_READ);
    if (fr != FR_OK) {
        printf("ERROR: Could not open file %s\r\n", roms_txt);
    }
    else {
        char *roms[36];
        roms[0] = NULL;

        // Read the roms.txt file.
        int count = 0;
        while (f_gets(buf, sizeof(buf), &fil)) {
            
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') {
                buf[strlen(buf)-1] = 0;
            }
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\r') {
                buf[strlen(buf)-1] = 0;
            }

            trim(buf);

            if (buf[0] != '#' && buf[0] != 0) {
                roms[count] = strdup(buf);
                count++;
                if (count == strlen(menu_items)) break;
            }
        }
        roms[count] = NULL;

        // Close file
        fr = f_close(&fil);

        // Output menu
        count = 0;
        while (roms[count] != NULL) {
            char *tmp = strdup(roms[count]);
            char *p = strtok(tmp,",");
            printf("%c - %s\n", menu_items[count], p);
            p = strtok(NULL, ",");
            p = strtok(NULL, ",");
            free(tmp);

            count++;
        }

        // Select menu option
        printf("Select from the menu: \n");
        int i = 0;
        while (true) {
            buf[0] = getchar();
            for (i=0; i < count; i++) {
                //printf("%c - %c\n", buf[0], menu_items[i]);
                if (toupper(buf[0]) == menu_items[i]) {
                    break;
                }
            }
            if (i < count) break;
        }

        // Get data from the selected menu item
        char *p = strtok(roms[i],",");
        p = strtok(NULL, ",");
        char *rom_file = strdup(p);
        p = strtok(NULL, ",");
        char *start_str = strdup(p);
        p = strtok(NULL, ",");
        char *via_str = strdup(p);
        p = strtok(NULL, ",");
        char *io_str = strdup(p);


        // Set the start and length of the rom, and free the start
        // string

        *start = convert_to_short_int(start_str);
        if (*start == 0) *start = 0x8000;
        free(start_str);

        *via = convert_to_short_int(via_str);
        free(via_str);

        *io = convert_to_short_int(io_str);
        if (*io == 0) *io = 0xf000;
        free(io_str);


        // Load the binary data from the file.
        printf("\nLoading ROM %s ...\n", rom_file);
 
        fr = f_open(&fil, rom_file, FA_READ);
        if (fr != FR_OK) {
            printf("ERROR: Could not open file %s\r\n", rom_file);
        }
        else {

            // Get the size of the file
            *length = f_size(&fil);

            // Allocate memory to hold the rom
            unsigned char *rom_data = malloc(*length);

            // Read the binary file
            UINT bytes_read = 0;
            fr = f_read(&fil, rom_data, *length, &bytes_read);
            if (fr != FR_OK) {
                printf("ERROR: Could not read file %s\r\n", rom_file);
            }

            // Set the pointer for the rom
            data = rom_data;

            // Close file
            fr = f_close(&fil);

            // Free the rom file name
            free(rom_file);
        }

        // free menu
        count = 0;
        while (roms[count] != NULL) {
            free(roms[count]);
            count++;
        }
    }

    // Return a pointer to the ROM data
    return data;
}
char *get_next_attr(FIL *fil, char *key, char *def) {

}
#define ATTR_NORMAL        0
#define ATTR_FIND_STARTING 1
#define ATTR_FIND_NEXT     2

char *get_attr(FIL *fil, char *key, char *def, uint8_t flags, char *last_key) {
    static char buf[255];

    // Go to start of the file
    if (!(flags & ATTR_FIND_NEXT)) {
        f_rewind(fil);
    }
    else {
        flags |= ATTR_FIND_NEXT;
    }

    int found = 0;

    // Get each line
    while (f_gets(buf, sizeof(buf), fil)) {
        
        // Trim off the LF
        if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') {
            buf[strlen(buf)-1] = 0;
        }

        // Trim off the CR
        if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\r') {
            buf[strlen(buf)-1] = 0;
        }

        // Trim off the leading and trailing spaces
        trim(buf);

        // Ignore comments and empty lines
        if (buf[0] != '#' && buf[0] != 0) {
            char *line = strdup(buf);
            //printf("Line: %s\n", line);
            char *p = strtok(line,"=");
            char *p_key = strdup(p);
            trim(p_key);
            //printf("p_key: %s\n", p_key);
            p = strtok(NULL,"#");
            char *p_value = strdup(p);
            trim(p_value);
            if (last_key != NULL) {
                strcpy(last_key, p_key);
            }
            //printf("p_key: %s key: %s\n", p_key, key);
            if (flags & ATTR_FIND_STARTING) {
                if (strncmp(p_key,key,strlen(key)) == 0) {
                    strcpy(buf, p_value);
                    found = 1;
                    break;
                }
            }
            else {
                if (strcmp(p_key,key) == 0) {
                    strcpy(buf, p_value);
                    found = 1;
                    break;
                }
            }
            free(line);
            free(p_key);
            free(p_value);
            if (found) break;
        }
    }
    if (!found) {
        strcpy(buf, def);
    }
    return buf;
}

#define DWORD_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define DWORD_TO_BINARY(dword)  \
  ((dword) & 0x80000000 ? '1' : '0'), \
  ((dword) & 0x40000000 ? '1' : '0'), \
  ((dword) & 0x20000000 ? '1' : '0'), \
  ((dword) & 0x10000000 ? '1' : '0'), \
  ((dword) & 0x08000000 ? '1' : '0'), \
  ((dword) & 0x04000000 ? '1' : '0'), \
  ((dword) & 0x02000000 ? '1' : '0'), \
  ((dword) & 0x01000000 ? '1' : '0'), \
  ((dword) & 0x00800000 ? '1' : '0'), \
  ((dword) & 0x00400000 ? '1' : '0'), \
  ((dword) & 0x00200000 ? '1' : '0'), \
  ((dword) & 0x00100000 ? '1' : '0'), \
  ((dword) & 0x00080000 ? '1' : '0'), \
  ((dword) & 0x00040000 ? '1' : '0'), \
  ((dword) & 0x00020000 ? '1' : '0'), \
  ((dword) & 0x00010000 ? '1' : '0'), \
  ((dword) & 0x00008000 ? '1' : '0'), \
  ((dword) & 0x00004000 ? '1' : '0'), \
  ((dword) & 0x00002000 ? '1' : '0'), \
  ((dword) & 0x00001000 ? '1' : '0'), \
  ((dword) & 0x00000800 ? '1' : '0'), \
  ((dword) & 0x00000400 ? '1' : '0'), \
  ((dword) & 0x00000200 ? '1' : '0'), \
  ((dword) & 0x00000100 ? '1' : '0'), \
  ((dword) & 0x00000080 ? '1' : '0'), \
  ((dword) & 0x00000040 ? '1' : '0'), \
  ((dword) & 0x00000020 ? '1' : '0'), \
  ((dword) & 0x00000010 ? '1' : '0'), \
  ((dword) & 0x00000008 ? '1' : '0'), \
  ((dword) & 0x00000004 ? '1' : '0'), \
  ((dword) & 0x00000002 ? '1' : '0'), \
  ((dword) & 0x00000001 ? '1' : '0') 

int read_config() {
    FRESULT fr;
    FATFS fs;
    FIL fil;
    FILINFO fno;
    DIR dp;
    int ret;
    char filename[] = "config.txt";

    // Open file for reading
    fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        printf("Could not open configuration file. Using defaults.\r\n");
    }
    else {
        char *data = NULL;
        char gpio[10];
        char via[10];
        data = get_attr(&fil,"ROM-FILE","roms.txt",ATTR_NORMAL,0);
        strcpy(roms_txt, data);
        printf("ROM File: %s\n", data);

        data = get_attr(&fil,"SERIAL-FLOW","0",ATTR_NORMAL,0);
        serial_flow = atoi(data);
        printf("SERIAL-FLOW: %s\n", data);

        data = get_attr(&fil,"LCD-INSTALLED","0",ATTR_NORMAL,0);
        lcd_installed = atoi(data);
        printf("LCD-INSTALLED: %s\n", data);

        data = get_attr(&fil,"IO-EMULATION","0",ATTR_NORMAL,0);
        printf("IO-EMULATION: %s\n", data);
        io_emulation = atoi(data);
        if (io_emulation == 2 || io_emulation == 3) {
            int count = 0;
            for (int i = 0; i < 29; i++) {
                sprintf(gpio, "GPIO-%i", i);
                data = get_attr(&fil,gpio,"RESERVED",ATTR_NORMAL,0);
                if (strcmp(data, "VIA") == 0) {
                    gpio_init(i);
                    gpio_pull_up(i);

                    if (count < 8) {
                        sprintf(via, "PA%i", count);
                    }
                    else {
                        sprintf(via, "PB%i", count-8);
                    }
                    printf("%s: %s\n", gpio, via);
                    pico_pins |= (int)pow((double)2,(double)i);
                    count++;
                }
                //printf("%s: %s\n", gpio, data);
            }
            //printf("pico_pins "DWORD_TO_BINARY_PATTERN"\n", DWORD_TO_BINARY(pico_pins));
        }

        // Read character mappings
        printf("Loading Key Map\n");
        char map_key[10];
        unsigned char mapping[10];
        unsigned char ch;

        char key[20];

        // Clear the character map
        for (int i=0;i<256;i++) char_map[i] = NULL;

        // Load key mappings
        data = get_attr(&fil,"MAP_","", ATTR_FIND_STARTING, key);            
        //printf("key: %s data: %s\n", key, data);
        while (*data != 0) {
            int cnt = 0;
            char *x = data;
            char *p = strtok(x,",");
            while (p != NULL) {
                strcpy(map_key, p);
                trim(map_key);
                ch = atoi(map_key);
                //printf("-%i\n",(int)ch);
                mapping[cnt] = ch;
                cnt++;
                p = strtok(NULL,",");
            }
            unsigned char *mp = (unsigned char *)malloc(cnt+1);
            memcpy(mp, mapping, cnt);
            mp[cnt] = 0;
            char *endptr;
            uint8_t num = strtol(key+4, &endptr, 16);
            //printf("num: %02X str: %s\n",num,key+4);
            char_map[num] = mp;



            data = get_attr(&fil,"MAP_","", ATTR_FIND_NEXT | ATTR_FIND_STARTING, key);
            //if (*data != 0)         
            //    printf("key: %s data: %s\n", key, data);
        }

/*

        for (int i = 0; i < 256; i++) {
            sprintf(map_key, "%02X", i);
            if (*data == 0) {
                char_map[i] = NULL;
                //printf("%s: Not found\n", map_key); 
            }
            else {
                printf("%s: %s\n", map_key, data); 

                int cnt = 0;
                char *x = data;
                char *p = strtok(x,",");
                while (p != NULL) {
                    strcpy(map_key, p);
                    trim(map_key);
                    ch = atoi(map_key);
                    printf("-%i\n",(int)ch);
                    mapping[cnt] = ch;
                    cnt++;
                    p = strtok(NULL,",");
                }
                unsigned char *mp = (unsigned char *)malloc(cnt+1);
                memcpy(mp, mapping, cnt);
                mp[cnt] = 0;
                char_map[i] = mp;

            }
        }
            */

        fr = f_close(&fil);
    }
}

int main() {
#ifdef OVERCLOCK
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(280000, true);
#endif
    stdio_init_all();

    sleep_ms(2000);

    strcpy(roms_txt, "roms.txt");

    FATFS fs;
    FRESULT fr;
    char buf[100];

    // Wait for user to press 'enter' to continue
    printf("\r\nPress 'ENTER' to start.\r\n");
    while (true) {
        buf[0] = getchar();
        if ((buf[0] == '\r') || (buf[0] == '\n')) {
            break;
        }
    }

    // Initialize SD card
    if (!sd_init_driver()) {
        printf("ERROR: Could not initialize SD card\r\n");
        while(true) {}
    }

    // Mount drive
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) {
        printf("ERROR: Could not mount filesystem\r\n");
        while(true) {}
    }
    else {

        printf("\n");
        printf("Reading Configuration ...\n");
        read_config();

        printf("\n");
        printf("Reading ROMs ...\n");

        // Select the ROM
        rom = select_menu(&rom_start, &rom_length, &via_location, &io_location);

        // Unmount drive
        f_unmount("0:");
    }

    if (rom == NULL) {
        printf("Loading ROM failed!\n");
        while(true) {}
    }

    printf("\n");
    gpio_init(CTS_PIN);
    gpio_set_dir(CTS_PIN   , GPIO_IN);
    gpio_pull_up(CTS_PIN);
    
    if (serial_flow == 0) { // Auto
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
    else if (serial_flow == 1) { // None
            printf("Configured No Serial Flow Control\n");
    }
    else if (serial_flow == 2) { // RTS/CTS
            printf("Configured RTS/CTS Flow Control\n");
    }
    else if (serial_flow == 3) { // XON/XOFF
            printf("Configured XON/XOFF Flow Control\n");
    }

    if (io_emulation == 0 || io_emulation == 4 || io_emulation == 5) { // Auto
        printf("Initializing IO\n");
        io_available = init_io(MCP_IRQ_GPIO_PIN);
        if (io_available) {
            printf("I/O Available (Expansion Board)\n");
            io_emulation = 5;
        }
        else {
            // If we get an error initializing, not available
            printf("No I/O Available (Expansion Board)\n");
        }
    }
    else if (io_emulation == 2 || io_emulation == 3) {
        printf("Configured I/O Available (Pico GPIO)\n");
        io_available = 1;
    }
    else if (io_emulation == 1) {
        printf("Configured No I/O Available\n");
    }
    printf("\n");

    if (io_emulation == 2 || io_emulation == 3) {
        uint32_t pins = pico_pins;
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

    if (io_emulation == 2) {
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
    else if (io_emulation == 3) {
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
