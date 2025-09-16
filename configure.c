#include "configure.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

char *trim(char *str)
{
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;
    
    if( str == NULL ) { return NULL; }
    if( str[0] == '\0' ) { return str; }
    
    len = strlen(str);
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

int include_a_file(FIL *out_fp, char *file);

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

uint8_t* translate_utf_8(uint32_t value, int *cnt) {
    static uint8_t bytes[8];
    for (int i=0; i<8; i++) bytes[i] = 0;
    int size = 0;
    if (value < 0x100000000) size = 4;
    if (value < 0x1000000) size = 3;
    if (value < 0x10000) size = 2;
    if (value < 0x100) size = 1;
    int b = size;
    *cnt = size + 1;
    for(int i=0; i<size; i++) {
        for (int j=0; j<6; j++) {
            bytes[b] = (bytes[b] >> 1);
            if (value & 1) bytes[b] |= 0x80;
            value = value >> 1;
        }
        bytes[b] = (bytes[b] >> 2) | 0x80;
        //printf("%i\n", bytes[b]);
        b--;

    }

    for (int j=0; j<4; j++) {
        bytes[b] = (bytes[b] >> 1);
        if (value & 1) bytes[b] |= 0x80;
        value = value >> 1;
    }
    bytes[b] = (bytes[b] >> 4) | 0b11100000;

    //printf("%i %i %i %i %i %i %i %i\n", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
    return (uint8_t *)bytes;
}

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
        // Store the mapping in the configuration
        map[num] = mp;

        // Get the next key
        data = get_attr(fil,mkey, "", ATTR_FIND_NEXT, key);
    }

}

struct define {
    char *key;
    char *value;
};

#define DEFINES_LENGTH 50

struct define defines[DEFINES_LENGTH];

int read_config(unsigned char * config_file, struct config_t* config) {
    int result = 0;

    strcpy(config->roms_txt, "roms.txt");
    config->serial_flow   = 0;
    config->io_emulation  = 0;
    config->lcd_installed = 0;
    config->pico_pins     = 0x0000;
    for (int i = 0; i < 256; i++) config->out_map[i] = NULL;
    for (int i = 0; i < 256; i++) config->in_map[i] = NULL;

    FRESULT fr;
    FATFS fs;
    FIL fil;
    FILINFO fno;
    DIR dp;
    int ret;

    char *tmp_file = "tmp_file.txt";

    FIL out_fp;
    fr = f_open(&out_fp, tmp_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr == FR_OK) {
        result = include_a_file(&out_fp, config_file);
        f_close(&out_fp);
        if (result != 0) return result;
    }
    else {
        printf("Build temporary file failed\n");
        return 3;
    }
    for (int i = 0; i < DEFINES_LENGTH; i++) {
        if (defines[i].key != NULL) {
            printf("Define %i: %s=%s\n", i, defines[i].key, defines[i].value);
            free(defines[i].key);
            free(defines[i].value);
            defines[i].key = NULL;
            defines[i].value = NULL;
        }
    }

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

unsigned short convert_to_short_int(char *number) {
    if (number[0]=='$') {
        return (unsigned short)strtol(number+1, NULL, 16);
    }
    else {
        return (unsigned short)strtol(number, NULL, 10);
    }
    return 0;
}

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
        printf("ERROR: Could not open file config-list.txt\r\n");
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

char *find_define(char *p_key) {
    for (int i = 0; i < DEFINES_LENGTH; i++) {
        if (defines[i].key != NULL) {
            if (strcmp(defines[i].key, p_key) == 0) {
                return defines[i].value;
            }
        }
    }
    return NULL;
}

void strreplace(char *string, uint8_t start, uint8_t end,  const char *replaceWith) {
    char *end_str = strdup(string + end + 1);
    buf[start] = 0;
    strcat(buf, replaceWith);
    strcat(buf, end_str);
    free(end_str);
}


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

            char *replace_str;
            if ((replace_str = strstr(buf, "${")) != NULL) {
                    char *replace_end = strtok(replace_str,"}");
                    char *key = strdup(replace_str + 2);
                    uint8_t start = replace_str - buf;
                    uint8_t end = strlen(buf);
                    replace_end[strlen(replace_end)] = '}';
                    
                    char *key_value = find_define(key);
                    if (key_value) {
                        strreplace(buf, start, end, key_value);
                    }
                    else {
                        strreplace(buf, start, end, "");
                    }
                    free(key);
                    
            }

            if (buf[0] == '!') {
                if (strncmp(buf, "!INCLUDE(", 9) == 0) {
                    char *include_file = strdup(buf + 9);
                    if (include_file[strlen(include_file)-1] == ')') {
                        include_file[strlen(include_file)-1] = 0;
                    }
                    //printf("Include: %s\n", include_file);
                    include_a_file(out_fp, include_file);
                    free(include_file);
                }
                else if (strncmp(buf, "!DEFINE(", 8) == 0) {
                    char *define_value = strdup(buf + 8);

                    char *p = strtok(define_value,",");

                    // Dupicate key and trim spaces
                    char *p_key = strdup(p);
                    trim(p_key);

                    // Get value, up to a comment or end of line
                    p = strtok(NULL,")");

                    // Duplicate value and trim spaces
                    char *p_value = strdup(p);
                    trim(p_value);
                    
                    int8_t cnt = 0;
                    while (defines[cnt].key != NULL) {
                        if (cnt == DEFINES_LENGTH) {
                            printf("Too many defines. Limit is %i\n", DEFINES_LENGTH);
                            free(p_key);
                            free(p_value);
                            break;
                        }
                        cnt++;
                    }

                    if (cnt < DEFINES_LENGTH) {
                        defines[cnt].key = p_key;
                        defines[cnt].value = p_value;
                    }
                    free(define_value);
                }
                else if (strncmp(buf, "!IFDEF(", 6) == 0) {
                    if (output_code) {
                        char *value = strdup(buf + 7);
                        char *p = strtok(value,")");

                        // Dupicate key and trim spaces
                        char *p_key = strdup(p);
                        trim(p_key);

                        char *key = find_define(p_key);
                        if (key) {
                            output_code = 1;
                        }
                        else {
                            output_code = 0;
                            hide_level = if_level + 1;
                        }
                        free(value);
                        free(p_key);
                    }
                    if_level++;
                }
                else if (strncmp(buf, "!IFNDEF(", 8) == 0) {
                    if (output_code) {
                        char *value = strdup(buf + 8);
                        char *p = strtok(value,")");

                        // Dupicate key and trim spaces
                        char *p_key = strdup(p);
                        trim(p_key);

                        char *key = find_define(p_key);
                        if (key) {
                            output_code = 0;
                            hide_level = if_level + 1;
                        }
                        else {
                            output_code = 1;
                        }
                        free(value);
                        free(p_key);

                    }
                    if_level++;
                }
                else if (strncmp(buf, "!IFEQ(", 6) == 0) {
                    if (output_code) {
                        char *value = strdup(buf + 6);
                        char *p = strtok(value,",");

                        // Dupicate key and trim spaces
                        char *p_key = strdup(p);
                        trim(p_key);

                        // Get value, up to a comment or end of line
                        p = strtok(NULL,")");

                        // Duplicate value and trim spaces
                        char *p_value = strdup(p);
                        trim(p_value);


                        char *key_value = find_define(p_key);
                        if (key_value && strcmp(key_value, p_value) == 0) {
                            output_code = 1;
                        }
                        else {
                            output_code = 0;
                            hide_level = if_level + 1;
                        }
                        free(value);
                        free(p_key);
                        free(p_value);
                    }
                    if_level++;
                }
                else if (strncmp(buf, "!IFNE(", 6) == 0) {
                    if (output_code) {
                        char *value = strdup(buf + 6);
                        char *p = strtok(value,",");

                        // Dupicate key and trim spaces
                        char *p_key = strdup(p);
                        trim(p_key);

                        // Get value, up to a comment or end of line
                        p = strtok(NULL,")");

                        // Duplicate value and trim spaces
                        char *p_value = strdup(p);
                        trim(p_value);

                        char *key_value = find_define(p_key);
                        if (key_value && strcmp(key_value, p_value) != 0) {
                            output_code = 1;
                        }
                        else {
                            output_code = 0;
                            hide_level = if_level + 1;
                        }
                        free(value);
                        free(p_key);
                        free(p_value);
                    }
                    if_level++;
                }
                else if (strncmp(buf, "!ELSE", 5) == 0) {
                    if (output_code == 0 && hide_level == if_level) {
                        output_code = 1;
                    }
                    else if (output_code == 1) {
                        hide_level = if_level;
                        output_code = 0;
                    }
                }
                else if (strncmp(buf, "!ENDIF", 6) == 0) {
                    if (output_code == 0 && hide_level == if_level) {
                        output_code = 1;
                    }
                    if_level--;
                }
                else {
                    printf("Invalid directive %s\n", buf);
                }
            }
            else {
                if (output_code) {
                    //printf("%s\n",buf);
                    f_printf(out_fp, "%s\n", buf);
                }
            }
        }

        f_close(&in_fp);
    }
    else {
        printf("Could not open file: %s\n",file);
        result = 1;
    }
    if (if_level < 0) {
        printf("Too many !ENDIF directives.\n");
        result = 1;
    }
    if (if_level > 0) {
        printf("Missing !ENDIF directive.\n");
        result = 2;
    }
    return result;
}
