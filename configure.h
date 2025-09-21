#include <sd_card.h>
#include <stdint.h>

#define ATTR_NORMAL        0      // Normal lookup. Whole key
#define ATTR_FIND_STARTING 1      // Key starts with the specified key value
#define ATTR_FIND_NEXT     2      // Get the next key starting with the specified key value

// Structure for the configuration
struct config_t {
    char     roms_txt[20];        // File that contains the ROM information
    int      serial_flow;         // Flow control for serial
    int      io_emulation;        // Type of I/O emulation
    int      lcd_installed;       // Is the LCD installed
    uint32_t pico_pins;           // Pico pins used for I/O emulation (Only for 2 and 3)
    uint8_t *out_map[256*5];      // Mapping for output characters
    uint8_t *in_map[256];         // Mapping for input characters
    uint8_t  show_output;         // Show output when reading configuration file (For debug)
};

#define IO_EMULATION_AUTO       0 // Auto detected. Tries to initialize the I/O
#define IO_EMULATION_NONE       1 // Do not use the I/O emulation
#define IO_EMULATION_FULL_PICO  2 // Full VIA emulation with Pico pins (Future)
#define IO_EMULATION_BASIC_PICO 3 // Basic port emulation with Pico pins
#define IO_EMULATION_FULL_EXP   4 // Full VIA emulation with I/O board (Future)
#define IO_EMULATION_BASIC_EXP  5 // Basic port emulation with I/O board

#define FLOW_CONTROL_AUTO       0 // Auto detected. If GPIO3 is grounded, use XON/XOFF
#define FLOW_CONTROL_NONE       1 // Force No flow control
#define FLOW_CONTROL_RTS_CTS    2 // Force RTS/CTS flow control
#define FLOW_CONTROL_XON_XOFF   3 // Force XON/XOFF flow control

#define COMMENT   '*'             // Character used for a comment
#define PARAMETER '|'             // Character used for a parameter
#define ESCAPE    '^'             // Character user for escape

char *trim(char *s);

char *get_attr(
  FIL *fil, 
  char *key, 
  char *def, 
  uint8_t flags, 
  char *last_key
);

uint8_t* translate_utf_8(
  uint32_t value, 
  int *cnt
);

int read_config(
  unsigned char *config_file, 
  struct config_t* config
);

unsigned char *select_menu(
  struct config_t* config, 
  unsigned short  *start, 
  unsigned short *length, 
  unsigned short *via, 
  unsigned short *io
);
unsigned char *config_menu(void);

void print_seq(char *seq);

int include_a_file(FIL *out_fp, char *file);

// Defines to print a 32 bit binary value
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
