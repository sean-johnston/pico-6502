#include "configure.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

//***************************************
//
// Trims leading and trailing spaces a buffer.
//
// str - buffer to trim
//
// Returns a pointer to the buffer
//
//***************************************
char *trim(char *str)
{
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;
    
    // If the NULL string return NULL
    if( str == NULL ) { return NULL; }

    // If empty string return empty string
    if( str[0] == '\0' ) { return str; }
    
    // Get the length of the string.
    len = strlen(str);

    // Get pointer to the end of the string
    endp = str + len;
    
    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while( isspace((unsigned char) *frontp) ) { ++frontp; }
    if( endp != frontp )
    {
        while( isspace((unsigned char) *(--endp)) && endp != frontp ) {}
    }
    
    if(frontp != str && endp == frontp )
    {
        // Empty string
        *(isspace((unsigned char) *endp) ? str : (endp + 1)) = '\0';
    }
    else if( str + len - 1 != endp )
            *(endp + 1) = '\0';
    
    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
    endp = str;
    if( frontp != str )
    {
            while( *frontp ) { *endp++ = *frontp++; }
            *endp = '\0';
    }
    
    return str;
}

//***************************************
//
// Get an attribute from the file.
//
// fil - File pointer
// key - Key to the attribute to get
// def - Default value if a key is not found
// flags - Flags for the call. Valid values are:
//     * ATTR_NORMAL - Searches for the exact key
//     * ATTR_FIND_STARTING - Find the first key that 
//                            the start of the key
//     * ATTR_FIND_NEXT - Gets the next key that match
//                        the start of the key
// last_key - Last key found
//
// Returns a pointer to the buffer
//
//***************************************
char *get_attr(FIL *fil, char *key, char *def, uint8_t flags, char *last_key) {
    static char buf[256];

    // If not find next
    if (!(flags & ATTR_FIND_NEXT)) {
        // Go to start of the file
        f_rewind(fil);
    }
    else {
        //flags |= ATTR_FIND_NEXT;
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
        if (buf[0] != COMMENT && buf[0] != 0) {
            // Dupicate line
            char *line = strdup(buf);

            // Get the key
            char *p = strtok(line,"=");

            // Dupicate key and trim spaces
            char *p_key = strdup(p);
            trim(p_key);

            // Get value, up to a comment or end of line
            p = strtok(NULL,"*");

            // Duplicate value and trim spaces
            char *p_value = strdup(p);
            trim(p_value);
            
            // Copy the key to the last key, so it can be returned
            if (last_key != NULL) {
                strcpy(last_key, p_key);
            }

            // If we are starting with the first key
            if ((flags & ATTR_FIND_STARTING) || (flags & ATTR_FIND_NEXT)) {
                // If the key is found
                if (strncmp(p_key,key,strlen(key)) == 0) {
                    // Store it in the buffer, set found flag
                    // break out of the loop
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
            
            // Free the line, key and value
            free(line);
            free(p_key);
            free(p_value);
            if (found) break;
        }
    }

    // If not found copy the default
    if (!found) {
        strcpy(buf, def);
    }
    return buf;
}

//***************************************
//
// Translates a unicode value to a stream of bytes in UTF-8.
//
// value - Unicode integer
// cnt - How many bytes in the output stream
// 
// Return an array of bytes representing the UTF-8.
//
// Example: E04A (2 bytes)
// 
// Unicode: 11100000 01001010
// UTF-8  : [1110]1110 [10]000001 [10]001010

//***************************************
uint8_t* translate_utf_8(uint32_t value, int *cnt) {
    static uint8_t bytes[8];
    // Zero out the bytes
    for (int i=0; i<8; i++) bytes[i] = 0;

    // Get size of the value, based on the value
    int size = 0;
    if (value < 0x100000000) size = 4;
    if (value < 0x1000000) size = 3;
    if (value < 0x10000) size = 2;
    if (value < 0x100) size = 1;
    int b = size;

    // Add one byte to the count
    *cnt = size + 1;

    // Iterate through the bytes
    for(int i=0; i<size; i++) {
        // New bytes contain 6 bits
        for (int j=0; j<6; j++) {
            // Prepend the bit
            bytes[b] = (bytes[b] >> 1);
            if (value & 1) bytes[b] |= 0x80;
            // Get next bit
            value = value >> 1;
        }
        // Prepend '10' to the byte
        bytes[b] = (bytes[b] >> 2) | 0x80;
        // Go to the next byte
        b--;

    }

    // Last byte, only 4 bits
    for (int j=0; j<4; j++) {
        // Prepend the bit
        bytes[b] = (bytes[b] >> 1);
        if (value & 1) bytes[b] |= 0x80;
        // Get next bit
        value = value >> 1;
    }
    // Prepend '1110' to the byte
    bytes[b] = (bytes[b] >> 4) | 0b11100000;

    // return the bytes
    return (uint8_t *)bytes;
}

//***************************************
//
// Load the character mappings from the configuration file
//
// fil - File pointer
// mkey - Key for the map we are reading
// map - Map to be populated
// show_output - Show the output as the map is populated
// 
//***************************************
void load_map(FIL *fil, char *mkey, uint8_t **map, uint8_t show_output) {
    char map_key[10];
    unsigned char mapping[50];
    unsigned char ch;

    char key[20];
    char *data;

    // Load key mappings
    data = get_attr(fil,mkey,"", ATTR_FIND_STARTING, key);   

    // If we have data
    while (*data != 0) {
        // Show output if the flag is set
        if (show_output == 1) {
          printf("Key: %s - Data: %s\n", key, data);
        }
        char *endptr;
        int cnt = 0;
        char *x = data;

        // If it is unicode
        if (data[0] == 'U' || data[0] == 'u') {
            // Convert number from a hex number
            uint16_t num = strtol(data+1, &endptr, 16);
            // Translate the UTF-8
            memcpy(mapping,translate_utf_8(num, &cnt),8);
        }
        // If it is an ANSI escape sequence
        else if (data[0] == ESCAPE) {
            cnt = 0;
            *data = 0x1b;
            while (data[cnt] != 0) {
                // If we found an escape
                if (data[cnt] == ESCAPE) {
                    // Make it escape
                    mapping[cnt]=0x1b;
                }
                else {
                    // Any other character
                    mapping[cnt]=data[cnt];
                }
                // Increment counter
                cnt++;
            }
            // Terminate mapping
            mapping[cnt] = 0;
        }
        else {
            // If it is a comma delimited list
            // Get first token
            char *p = strtok(x,",");

            // While we have tokens
            while (p != NULL) {
                // Copy the key and trim
                strcpy(map_key, p);
                trim(map_key);

                // If token ends in dollar sign
                if (*map_key == '$') {
                    // Process as a hex
                    ch = (int)strtol(map_key+1, &endptr, 16);
                }
                else {
                    // Process as decimal
                    ch = atoi(map_key);
                }
                // Store the value in the mapping
                mapping[cnt] = ch;

                // Increment counter
                cnt++;

                // Get next token
                p = strtok(NULL,",");
            }
        }

        // Allocate space for mapping
        unsigned char *mp = (unsigned char *)malloc(cnt+1);
        // Copy the data to the mapping, and terminate sequence
        memcpy(mp, mapping, cnt);
        mp[cnt] = 0;

        // Get the mapping set
        uint8_t map_set = 0;

        // Check if there is a colon
        char *ms = strstr(key,":");

        // If we have one, use the map set
        if (ms) {
            // Terminate the key and increment the pointer
            // past the colon
            *ms = 0;
            ms++;

            // Get set value as hex
            if (*ms == '$') {
                map_set = strtol(ms+1, &endptr, 16);
            }
            // Get set value as decimal
            else {
                map_set = strtol(ms, &endptr, 10);
            }
        }

        // If key ends in dollar sign
        uint8_t num;
        // Process key as hex
        if (*(key+strlen(mkey)) == '$') {
            num = strtol(key+strlen(mkey)+1, &endptr, 16);
        }
        // Process key as decimal
        else {
            num = strtol(key+strlen(mkey), &endptr, 10);
        }
        if (map_set < MAP_SETS) {
            // Store the mapping in the configuration
            map[num+(map_set*256)] = mp;
        }

        // Get the next key
        data = get_attr(fil,mkey, "", ATTR_FIND_NEXT, key);
    }

}

//***************************************
//
// Structure for the configuration defines
//
//***************************************
struct define {
    char *key;
    char *value;
    struct define *next;
};

//***************************************
//
// Pointer to the defines
//
//***************************************
struct define *defines = NULL; // [DEFINES_LENGTH];

//***************************************
//
// Find the key in the linked list, and
// returns it.
//
// p_key = Key to find
//
// Returns the key, or NULL if it
// is not found.
//
//***************************************
struct define *find_key(char *p_key) {
    struct define *next = defines;

    while (next != NULL) {
        // If the key matches, return it
        if (strcmp(p_key, next->key) == 0) {
            return next;
        }
        // Go to the next item
        next = next->next;
    }
    return NULL;
}

//***************************************
//
// Add a define to the link list
//
// key - Key for the define
// value - Value for the define
// 
//***************************************
void add_define(char *key, char *value) {

    // Find the key
    struct define *next = find_key(key);

    // If found
    if (next) {
        // Free the old value
        free(next->value);

        // assign a new value
        next->value = strdup(value);
    }

    // Store pointer to the defines
    next = defines;

    // Allocate a new define structure
    defines = (struct define *)malloc(sizeof(struct define));

    // Assign key and value
    defines->key = key;
    defines->value = value;

    // If there are no defines
    if (next == NULL) {
        defines->next = NULL;
    }
    // Add to the defines
    else {
        defines->next = next;
    }
}

//***************************************
//
// Find the define in the linked list, and
// returns the value of the key.
//
// p_key = Key to find
// 
// Returns value of the key, or NULL if it
// is not found.
//
//***************************************
char *find_define(char *p_key) {
    // Find the key
    struct define *key = find_key(p_key);

    // If we found the key return the value
    if (key) return key->value;

    // Return NULL if we don't find one
    return NULL;
}

//***************************************
//
// Free the linked list of the defines
//
//***************************************
void free_defines(void) {
    struct define *next = NULL;
    // While we have items
    while (defines != NULL) {
        // Get the next item
        next = defines->next;
        //printf("Define: %s=%s\n", defines->key, defines->value);
        // Free the key and value
        free(defines->key);
        free(defines->value);
        // Free the define structure
        free(defines);
        // Assign next to the pointer
        defines = next;
    }
}

//***************************************
//
// Read the configuration file, and 
// return it configuration structure.
//
// config_file = File to read
// config - Pointer to the structure to
//          read the configuration into.
// 
// Returns 0 for success, and non-zero
// for an error.
//
//***************************************
int read_config(unsigned char * config_file, struct config_t* config) {
    int result = 0;

    // Set defaults
    strcpy(config->roms_txt, "roms.txt");
    config->serial_flow   = 0;
    config->io_emulation  = 0;
    config->lcd_installed = 0;
    config->pico_pins     = 0x0000;

    // Initialize the maps
    for (int i = 0; i < 256*5; i++) config->out_map[i] = NULL;
    for (int i = 0; i < 256; i++) config->in_map[i] = NULL;

    FRESULT fr;
    FATFS fs;
    FIL fil;
    FILINFO fno;
    DIR dp;
    int ret;

    char *tmp_file = "tmp_file.txt";

    FIL out_fp;
    // Open the temporary output file
    fr = f_open(&out_fp, tmp_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr == FR_OK) {
        // Include the main file
        result = include_a_file(&out_fp, config_file);
        f_close(&out_fp);
        // If error return the result
        if (result != 0) return result;
    }
    else {
        printf("Build temporary file failed\n");
        return 3;
    }

    // Free the defines
    free_defines();

    // Open file for reading
    fr = f_open(&fil, tmp_file, FA_READ);
    if (fr != FR_OK) {
        printf("Could not open configuration file. Using defaults.\r\n");
    }
    else {
        char *data = NULL;
        char gpio[10];
        char via[10];

        // Get the flag to show output from the configuration
        data = get_attr(&fil,"SHOW_OUTPUT","0",ATTR_NORMAL,0);
        config->show_output = atoi(data);

        // Get the ROMs file name
        data = get_attr(&fil,"ROM-FILE","roms.txt",ATTR_NORMAL,0);
        strcpy(config->roms_txt, data);
        printf("ROM File: %s\n", data);

        // Get flow control mode
        data = get_attr(&fil,"SERIAL-FLOW","0",ATTR_NORMAL,0);
        config->serial_flow = atoi(data);
        printf("SERIAL-FLOW: %s\n", data);

        // Get if LCD is installed
        data = get_attr(&fil,"LCD-INSTALLED","0",ATTR_NORMAL,0);
        config->lcd_installed = atoi(data);
        printf("LCD-INSTALLED: %s\n", data);

        // Get the I/O emulation mode
        data = get_attr(&fil,"IO-EMULATION","0",ATTR_NORMAL,0);
        printf("IO-EMULATION: %s\n", data);
        config->io_emulation = atoi(data);

        // If emulation uses Pico pins
        if (config->io_emulation == IO_EMULATION_FULL_PICO || config->io_emulation == IO_EMULATION_BASIC_PICO) {
            int count = 0;

            // Iterate through the GPIO pins
            for (int i = 0; i < 29; i++) {

                // Get GPIO pin
                sprintf(gpio, "GPIO-%i", i);
                data = get_attr(&fil,gpio,"RESERVED",ATTR_NORMAL,0);

                // If it is VIA
                if (strcmp(data, "VIA") == 0) {
                    // Setup tht pin and assign it to a port and number
                    gpio_init(i);
                    gpio_pull_up(i);

                    if (count < 8) {
                        // Port A
                        sprintf(via, "PA%i", count);
                    }
                    else {
                        // Port B
                        sprintf(via, "PB%i", count-8);
                    }
                    printf("%s: %s\n", gpio, via);

                    // OR the pin to the pico pins
                    config->pico_pins |= (int)pow((double)2,(double)i);

                    // Increment the count
                    count++;
                }
            }
        }

        // Read character mappings
        printf("Loading Key Map\n");

        load_map(&fil, "OUT_MAP_", config->out_map, config->show_output);

        load_map(&fil, "IN_MAP_", config->in_map, config->show_output);

        // Close the configuration file
        fr = f_close(&fil);
    }
    return 0;
}

//***************************************
//
// Convert a string to short integer. If
// the string is preceeded by a '$',
// convert from a hex number.
//
// number - String that contains the number
//
// Returns a short integer represented by
// the string.
//
//***************************************
unsigned short convert_to_short_int(char *number) {
    if (number[0]=='$') {
        return (unsigned short)strtol(number+1, NULL, 16);
    }
    else {
        return (unsigned short)strtol(number, NULL, 10);
    }
    return 0;
}

//***************************************
//
// Load the menu of the roms. Allows the 
// user to select from the menu, and 
// returns the rom and other information.
//
// config - Pointer to the configuration
//          structure
// start  - Pointer to an unsigned short
//          integer of the start address
//          of the ROM.
// length - Pointer to an unsigned short
//          integer of the length of the
//          ROM.
//  
// via    - Pointer to an unsigned short
//          integer of the via address.
//
// io     - Pointer to an unsigned short
//          integer of the I/O address.
//
// Returns the binary of the ROM, or 
// NULL if a rom was not able to be
// loaded.
//
//***************************************
unsigned char *select_menu(struct config_t* config, unsigned short  *start, unsigned short *length, unsigned short *via, unsigned short *io) {
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
    fr = f_open(&fil, config->roms_txt, FA_READ);
    if (fr != FR_OK) {
        printf("ERROR: Could not open file %s\r\n", config->roms_txt);
    }
    else {
        char *roms[36];
        roms[0] = NULL;

        // Read the roms.txt file.
        int count = 0;

        // Get file line by line
        while (f_gets(buf, sizeof(buf), &fil)) {
            
            // Remove the LF
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') {
                buf[strlen(buf)-1] = 0;
            }

            // Remove the CR
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\r') {
                buf[strlen(buf)-1] = 0;
            }

            // Remove extra spaces
            trim(buf);

            // If not comment or empty line
            if (buf[0] != COMMENT && buf[0] != 0) {
                // Add ROM to the list
                roms[count] = strdup(buf);
                count++;
                if (count == strlen(menu_items)) break;
            }
        }

        // Terminate the list
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

//***************************************
//
// Load the menu of the config-list.txt. 
// Allows the user to select from the menu, 
// and returns the configuration file.
// 
// If the config-list.txt is not found,
// config.txt is returned.
//
// Returns the name of the configuration
// file selected, or config.txt.
//
//***************************************
unsigned char *config_menu(void) {
    FRESULT fr;
    FATFS fs;
    FIL fil;
    FILINFO fno;
    DIR dp;
    int ret;
    char buf[255];

    unsigned char * data = NULL;

    char *menu_items = "123456789ABCDEFGHIJKLMNOPQRSTUVWYZ";

#define CONFIG_COUNT 36

    char *config_file;
    // Open file for reading
    fr = f_open(&fil, "config-list.txt", FA_READ);
    if (fr != FR_OK) {
        printf("Could not open file config-list.txt. User config.txt.\n");
        config_file = strdup("config.txt");
    }
    else {
        char *configs[36];
        configs[0] = NULL;

        // Read the roms.txt file.
        int count = 0;

        // Get file line by line
        while (f_gets(buf, sizeof(buf), &fil)) {
            
            // Remove the LF
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') {
                buf[strlen(buf)-1] = 0;
            }

            // Remove the CR
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\r') {
                buf[strlen(buf)-1] = 0;
            }

            // Remove extra spaces
            trim(buf);

            // If not comment or empty line
            if (buf[0] != COMMENT && buf[0] != 0) {
                // Add ROM to the list
                configs[count] = strdup(buf);
                count++;
                if (count == strlen(menu_items)) break;
            }
        }

        // Terminate the list
        configs[count] = NULL;

        // Close file
        fr = f_close(&fil);

        // Output menu
        count = 0;
        while (configs[count] != NULL) {
            char *tmp = strdup(configs[count]);
            char *p = strtok(tmp,",");
            printf("%c - %s\n", menu_items[count], p);
            free(tmp);
            count++;
        }

        // Select menu option
        printf("Select from the menu: \n");
        int i = 0;
        while (true) {
            buf[0] = getchar();
            for (i=0; i < count; i++) {
                if (toupper(buf[0]) == menu_items[i]) {
                    break;
                }
            }
            if (i < count) break;
        }

        // Get data from the selected menu item
        char *p = strtok(configs[i],",");
        p = strtok(NULL, ",");
        config_file = strdup(p);

        // free menu
        count = 0;
        while (configs[count] != NULL) {
            free(configs[count]);
            count++;
        }
    }

    // Return a pointer to the ROM data
    return config_file;
}

// Output the sequence in hex
void print_seq(char *seq) {
    for (int i=0; i<strlen(seq); i++) {
        printf("%02x ", seq[i]);
    }
}

char buf[255];

int if_level = 0;
int output_code = 1;
int hide_level = 0;
int else_count = 0;

//***************************************
//
// Replace a string with another
//
// string - String to do the replacing to
// start  - Start position to replace.
// end    - End position to replace.
// replaceWith - String to replace with
//
//***************************************
void strreplace(char *string, uint8_t start, uint8_t end,  const char *replaceWith) {
    char *end_str = strdup(string + end + 1);
    buf[start] = 0;
    strcat(buf, replaceWith);
    strcat(buf, end_str);
    free(end_str);
}


//***************************************
//
// Include a file in the out file
//
// out_fp - File pointer for output file
// file   - Name of the file to include.
//
// Return 0 for succes, and non-zero for
// error.
//
//***************************************
int include_a_file(FIL *out_fp, char *file) {
    FRESULT fr;
    int result = 0;
    FIL in_fp;
    printf("Opening configuration file: %s\n", file);
    fr = f_open(&in_fp,file, FA_READ);
    if (fr == FR_OK) {
        while(f_gets(buf, sizeof(buf), &in_fp)) {
            // Remove the LF
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') {
                buf[strlen(buf)-1] = 0;
            }

            // Remove the CR
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\r') {
                buf[strlen(buf)-1] = 0;
            }

            // Remove extra spaces
            trim(buf);

            //***********************************************************
            // Process substitution
            //***********************************************************
            char *replace_str;
            if ((replace_str = strstr(buf, "${")) != NULL) {
                    // Get teh replace string
                    char *replace_end = strtok(replace_str,"}");
                    // Get the lookup key for the define
                    char *key = strdup(replace_str + 2);
                    // Get the start and end position of the 
                    // part of the string to replace
                    uint8_t start = replace_str - buf;
                    uint8_t end = strlen(buf);

                    // Put the curly bracket back
                    replace_end[strlen(replace_end)] = '}';
                    
                    // Find the define to replace
                    char *key_value = find_define(key);

                    // If we find a define, replace it
                    if (key_value) {
                        strreplace(buf, start, end, key_value);
                    }
                    // If not, replace it with an empty string
                    else {
                        strreplace(buf, start, end, "");
                    }
                    // Free the key
                    free(key);
                    
            }

            //***********************************************************
            // Process directive
            //***********************************************************
            if (buf[0] == '!') {

                // Process include
                if (strncmp(buf, "!INCLUDE(", 9) == 0) {
                    // Get the name of the file to include
                    char *include_file = strdup(buf + 9);
                    // Remove trailing parentheses
                    if (include_file[strlen(include_file)-1] == ')') {
                        include_file[strlen(include_file)-1] = 0;
                    }
                    //printf("Include: %s\n", include_file);

                    // Include the file
                    include_a_file(out_fp, include_file);

                    // Free the file name
                    free(include_file);
                }
                else if (strncmp(buf, "!DEFINE(", 8) == 0) {
                    // Get the define key and value
                    char *define_value = strdup(buf + 8);

                    // Get the key
                    char *p = strtok(define_value,",");
                    char *p_key = strdup(p);
                    trim(p_key);


                    // Get the value
                    p = strtok(NULL,")");
                    char *p_value = strdup(p);
                    trim(p_value);
                    
                    // Add the define
                    add_define(p_key, p_value);

                    // Free the define value
                    free(define_value);
                }
                else if (strncmp(buf, "!IFDEF(", 6) == 0) {\
                    // If we are outputting code
                    if (output_code) {
                        // Get the key
                        char *value = strdup(buf + 7);
                        char *p = strtok(value,")");
                        char *p_key = strdup(p);
                        trim(p_key);

                        // Find the key
                        char *key = find_define(p_key);

                        // If key is found, set flag to output code
                        if (key) {
                            output_code = 1;
                        }
                        // If key is not found, set flag to not output code
                        else {
                            output_code = 0;
                            hide_level = if_level + 1;
                        }

                        // Free the value and key
                        free(value);
                        free(p_key);
                    }
                    // Increment the if level
                    if_level++;
                }
                else if (strncmp(buf, "!IFNDEF(", 8) == 0) {
                    // If we are outputting code
                    if (output_code) {
                        // Get the key
                        char *value = strdup(buf + 8);
                        char *p = strtok(value,")");
                        char *p_key = strdup(p);
                        trim(p_key);

                        // Find the key
                        char *key = find_define(p_key);
                        // If key is found, set flag to not output code
                        if (key) {
                            output_code = 0;
                            hide_level = if_level + 1;
                        }
                        // If key is not found, set flag to output code
                        else {
                            output_code = 1;
                        }
                        // Free the value and key
                        free(value);
                        free(p_key);

                    }
                    // Increment the if level
                    if_level++;
                }
                else if (strncmp(buf, "!IFEQ(", 6) == 0) {
                    // If we are outputting code
                    if (output_code) {
                        // Get the if value
                        char *value = strdup(buf + 6);

                        // Get the key
                        char *p = strtok(value,",");
                        char *p_key = strdup(p);
                        trim(p_key);

                        // Get the value
                        p = strtok(NULL,")");
                        char *p_value = strdup(p);
                        trim(p_value);

                        // Find the key
                        char *key_value = find_define(p_key);
                        // If the key is found and matches the value
                        if (key_value && strcmp(key_value, p_value) == 0) {
                            // Turn on output flag
                            output_code = 1;
                        }
                        // If the key is not found or doesn't matche the value
                        else {
                            // Turn off output flag
                            output_code = 0;
                            hide_level = if_level + 1;
                        }

                        // Free if value, key, and value
                        free(value);
                        free(p_key);
                        free(p_value);
                    }
                    // Increment the if level
                    if_level++;
                }
                else if (strncmp(buf, "!IFNE(", 6) == 0) {
                    // If we are outputting code
                    if (output_code) {
                        // Get the if value
                        char *value = strdup(buf + 6);

                        // Get the key
                        char *p = strtok(value,",");
                        char *p_key = strdup(p);
                        trim(p_key);

                        // Get the value
                        p = strtok(NULL,")");
                        char *p_value = strdup(p);
                        trim(p_value);

                        // Find the key
                        char *key_value = find_define(p_key);

                        // If the key is found and does not matche the value
                        if (key_value && strcmp(key_value, p_value) != 0) {
                            // Turn on the output flag
                            output_code = 1;
                        }
                        else {
                            // Turn off the output flag
                            output_code = 0;
                            hide_level = if_level + 1;
                        }
                        
                        // Free the memory
                        free(value);
                        free(p_key);
                        free(p_value);
                    }
                    // Increment the if level
                    if_level++;
                }
                else if (strncmp(buf, "!ELSE", 5) == 0) {
                    // If the output flag is off and we are as the 
                    // if level that it was turn on, turn it on
                    if (output_code == 0 && hide_level == if_level) {
                        output_code = 1;
                    }
                    // If the output flag is on and we are as the 
                    // if level that it was turn off, turn it on
                    else if (output_code == 1 && hide_level == if_level) {
                        hide_level = if_level;
                        output_code = 0;
                    }
                }
                else if (strncmp(buf, "!ENDIF", 6) == 0) {
                    // If the output flag is off and we are as the 
                    // if level that it was turn on, turn it on
                    if (output_code == 0 && hide_level == if_level) {
                        output_code = 1;
                    }
                    // Decrement the if level
                    if_level--;
                }
                else {
                    // We found an invalid directive
                    printf("Invalid directive %s\n", buf);
                }
            }
            else {
                if (output_code) {
                    // If normal text, output the line
                    //printf("%s\n",buf);
                    f_printf(out_fp, "%s\n", buf);
                }
            }
        }

        // Close the input file
        f_close(&in_fp);
    }
    else {
        // We could not open the input file
        printf("Could not open file: %s\n",file);
        result = 1;
    }
    if (if_level < 0) {
        // Found to many endifs
        printf("Too many !ENDIF directives.\n");
        result = 1;
    }
    if (if_level > 0) {
        // Did find enough endifs
        printf("Missing !ENDIF directive.\n");
        result = 2;
    }
    // Return the result
    return result;
}
