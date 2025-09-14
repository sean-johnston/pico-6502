#include "configure.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

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
        if (buf[0] != '*' && buf[0] != 0) {
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

int read_config(struct config_t* config) {
    strcpy(config->roms_txt, "roms.txt");
    config->serial_flow   = 0;
    config->io_emulation  = 0;
    config->lcd_installed = 0;
    config->pico_pins     = 0x0000;
    for (int i = 0; i < 256; i++) config->char_map[i] = NULL;

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
        strcpy(config->roms_txt, data);
        printf("ROM File: %s\n", data);

        data = get_attr(&fil,"SERIAL-FLOW","0",ATTR_NORMAL,0);
        config->serial_flow = atoi(data);
        printf("SERIAL-FLOW: %s\n", data);

        data = get_attr(&fil,"LCD-INSTALLED","0",ATTR_NORMAL,0);
        config->lcd_installed = atoi(data);
        printf("LCD-INSTALLED: %s\n", data);

        data = get_attr(&fil,"IO-EMULATION","0",ATTR_NORMAL,0);
        printf("IO-EMULATION: %s\n", data);
        config->io_emulation = atoi(data);
        if (config->io_emulation == 2 || config->io_emulation == 3) {
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
                    config->pico_pins |= (int)pow((double)2,(double)i);
                    count++;
                }
                //printf("%s: %s\n", gpio, data);
            }
            //printf("pico_pins "DWORD_TO_BINARY_PATTERN"\n", DWORD_TO_BINARY(pico_pins));
        }

        // Read character mappings
        printf("Loading Key Map\n");
        char map_key[10];
        unsigned char mapping[50];
        unsigned char ch;

        char key[20];

        // Load key mappings
        data = get_attr(&fil,"OUT_MAP_","", ATTR_FIND_STARTING, key);   
        //printf("key: %s data: %s\n", key, data);
        while (*data != 0) {
            char *endptr;
            int cnt = 0;
            char *x = data;
            if (data[0] == 'U' || data[0] == 'u') {
                uint16_t num = strtol(data+1, &endptr, 16);
                memcpy(mapping,translate_utf_8(num, &cnt),8);
            }
            else if (data[0] == '^') {
                cnt = 0;
                *data = 0x1b;
                while (data[cnt] != 0) {
                    if (data[cnt] == '^') {
                        mapping[cnt]=0x1b;
                    }
                    else {
                        mapping[cnt]=data[cnt];
                    }
                    cnt++;
                }
                mapping[cnt] = 0;
            }
            else {
                char *p = strtok(x,",");
                while (p != NULL) {
                    strcpy(map_key, p);
                    trim(map_key);
                    if (*map_key == '$') {
                        ch = (int)strtol(map_key+1, &endptr, 16);
                    }
                    else {
                        ch = atoi(map_key);
                    }
                    mapping[cnt] = ch;
                    cnt++;
                    p = strtok(NULL,",");
                }
            }
            unsigned char *mp = (unsigned char *)malloc(cnt+1);
            memcpy(mp, mapping, cnt);
            mp[cnt] = 0;
            uint8_t num;
            if (*(key+8) == '$') {
                num = strtol(key+9, &endptr, 16);
            }
            else {
                num = strtol(key+8, &endptr, 10);
            }
            config->char_map[num] = mp;

            data = get_attr(&fil,"OUT_MAP_","", ATTR_FIND_NEXT | ATTR_FIND_STARTING, key);
        }
        fr = f_close(&fil);
    }
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
        while (f_gets(buf, sizeof(buf), &fil)) {
            
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') {
                buf[strlen(buf)-1] = 0;
            }
            if (strlen(buf) > 0 && buf[strlen(buf)-1] == '\r') {
                buf[strlen(buf)-1] = 0;
            }

            trim(buf);

            if (buf[0] != '*' && buf[0] != 0) {
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
