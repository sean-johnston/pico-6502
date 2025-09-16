#include <sd_card.h>
#include <stdint.h>

#define ATTR_NORMAL        0
#define ATTR_FIND_STARTING 1
#define ATTR_FIND_NEXT     2

struct config_t {
    char roms_txt[20];
    int serial_flow;
    int io_emulation;
    int lcd_installed;
    uint32_t pico_pins;
    uint8_t *out_map[256];
    uint8_t *in_map[256];
    uint8_t show_output;
};

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

#define IO_EMULATION_AUTO       0
#define IO_EMULATION_NONE       1
#define IO_EMULATION_FULL_PICO  2
#define IO_EMULATION_BASIC_PICO 3
#define IO_EMULATION_FULL_EXP   4
#define IO_EMULATION_BASIC_EXP  5

#define FLOW_CONTROL_AUTO       0
#define FLOW_CONTROL_NONE       1
#define FLOW_CONTROL_RTS_CTS    2
#define FLOW_CONTROL_XON_XOFF   3

#define COMMENT   '*'
#define PARAMETER '|'
#define ESCAPE    '^'

char *trim(char *s);
char *get_attr(FIL *fil, char *key, char *def, uint8_t flags, char *last_key);
uint8_t* translate_utf_8(uint32_t value, int *cnt);
int read_config(unsigned char *config_file, struct config_t* config);
unsigned char *select_menu(struct config_t* config, unsigned short  *start, unsigned short *length, unsigned short *via, unsigned short *io);
unsigned char *config_menu(void);
void print_seq(char *seq);