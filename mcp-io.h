#include <stdint.h>
#include "pico/stdlib.h"

#define MCP23017_IODIRA   0x00 //Direction of data I/O (bits set as: 1 = input, 0 = output)
#define MCP23017_IODIRB   0x01 //Direction of data I/O (bits set as: 1 = input, 0 = output)
#define MCP23017_GPINTENA 0x04 //Interrupt on change
#define MCP23017_GPINTENB 0x05 //Interrupt on change
#define MCP23017_INTCONA  0x08 //Interrupt on change control register
#define MCP23017_INTCONB  0x09 //Interrupt on change control register
#define MCP23017_GPPUA    0x0C //PullUp set internal pull up for input pins
#define MCP23017_GPPUB    0x0D //PullUp set internal pull up for input pins
#define MCP23017_GPIOA    0x12 //Port Register - write modifies latch
#define MCP23017_GPIOB    0x13 //Port Register - write modifies latch

#define MCP23017_IOCONA   0x0A //IO Configuration - BANK/MIRROR/SLEW/INTPOL
#define MCP23017_IOCONB   0x0B //IO Configuration - BANK/MIRROR/SLEW/INTPOL
#define MCP23017_INTFA    0x0E //Interrupt Flag
#define MCP23017_INTFB    0x0F //Interrupt Flag
#define MCP23017_INTCAPA  0x10 //Interrupt Capture
#define MCP23017_INTCAPB  0x11 //Interrupt Capture

#define MCP23017_IOCON_BANK_BIT   7
#define MCP23017_IOCON_MIRROR_BIT 6
#define MCP23017_IOCON_SEQOP_BIT  5
#define MCP23017_IOCON_DISSLW_BIT 4
#define MCP23017_IOCON_HAEN_BIT   3
#define MCP23017_IOCON_ODR_BIT    2
#define MCP23017_IOCON_INTPOL_BIT 1

inline void set_bit(int *value, int bit, bool set) {
	if (bit >= 0 && bit <= 15) {
		if (set) {
			*value |= (1 << bit);
		} else {
			*value &= ~(1 << bit);
		}
	}
}

inline bool is_bit_set(int value, int bit) {
	if (bit >= 0 && bit <= 15) {
		return (bool) (0x1 & (value >> bit));
	}
	return false;
}


static const int MIRROR_INTERRUPTS = 1; //save a gpio by mirroring interrupts across both banks
static const int OPEN_DRAIN_INTERRUPT_ACTIVE = 0;
static const int POLARITY_INTERRUPT_ACTIVE_LOW = 0;
static const int MCP_ALL_PINS_INPUT = 0xff;
static const int MCP_ALL_PINS_PULL_UP = 0xff;
static const int MCP_ALL_PINS_COMPARE_TO_LAST = 0x0000;
static const int MCP_ALL_PINS_INTERRUPT_ENABLED = 0xffff;

#define MCP_IRQ_GPIO_PIN 19
#define I2C_GPIO_PIN_SDA 20
#define I2C_GPIO_PIN_SLC 21

#define PORT_A 0
#define PORT_B 1

typedef struct i2c_inst i2c_inst_t;

struct i2c_handle {
	i2c_inst_t *i2c;
	uint8_t address;
};

struct i2c_handle *setup_i2c(i2c_inst_t *i2c, uint8_t address);
void free_i2c(struct i2c_handle *handle);
int write_register(struct i2c_handle *handle, uint8_t reg, uint8_t value);
int read_register(struct i2c_handle *handle, uint8_t reg, uint8_t *buffer);
int write_dual_registers(struct i2c_handle *handle, uint8_t reg, int value);
int read_dual_registers(struct i2c_handle *handle, uint8_t reg);
int setup_bank_configuration(struct i2c_handle *handle, int reg, bool mirroring, bool polarity);
int setup(struct i2c_handle *handle, bool mirroring, bool polarity);
int set_port_direction(struct i2c_handle *handle, uint8_t port, uint8_t direction);
int write_port(struct i2c_handle *handle, uint8_t port, uint8_t value);
int read_port(struct i2c_handle *handle, uint8_t port, uint8_t *value);
int set_pullup(struct i2c_handle *handle, uint8_t port, uint8_t direction);
