#include "hardware/i2c.h"
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "mcp-io.h"

struct i2c_handle *setup_i2c(i2c_inst_t *i2c, uint8_t address) {
	i2c_init(i2c0, 400000);
	gpio_set_function(I2C_GPIO_PIN_SDA, GPIO_FUNC_I2C);
	gpio_set_function(I2C_GPIO_PIN_SLC, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_GPIO_PIN_SDA);
	gpio_pull_up(I2C_GPIO_PIN_SLC);
	struct i2c_handle *handle = (struct i2c_handle *)malloc(sizeof(struct i2c_handle));
	handle->i2c = i2c;
	handle->address = address;
	return handle;
}

void free_i2c(struct i2c_handle *handle) {
	free(handle);
}

int write_register(struct i2c_handle *handle, uint8_t reg, uint8_t value){
	uint8_t command[] = { reg, value };
	int result = i2c_write_blocking(handle->i2c, handle->address, command, 2, false);
	if (result == PICO_ERROR_GENERIC) {
		return result;
	}
	return PICO_ERROR_NONE;
}

int read_register(struct i2c_handle *handle, uint8_t reg, uint8_t *buffer) {
	//uint8_t buffer = 0;
	int result;
	result = i2c_write_blocking(handle->i2c, handle->address,  &reg, 1, true);
	//mcp_debug("i2c_write_blocking: %d\n",result);
	if (result == PICO_ERROR_GENERIC) {
		return result;
	}

	result = i2c_read_blocking(handle->i2c, handle->address, buffer, 1, false);
	//mcp_debug("i2c_read_blocking: %d, read: %d\n",result, buffer);
	if (result == PICO_ERROR_GENERIC)
		return result;

	return PICO_ERROR_NONE;
}

int write_dual_registers(struct i2c_handle *handle, uint8_t reg, int value) {
	uint8_t command[] = {
			reg,
			(uint8_t)(value & 0xff),
			(uint8_t)((value>>8) & 0xff)
	};
	int result = i2c_write_blocking(handle->i2c, handle->address, command, 3, false);
	if (result == PICO_ERROR_GENERIC) {
		return result;
	}
	return PICO_ERROR_NONE;
}

int read_dual_registers(struct i2c_handle *handle, uint8_t reg) {
	uint8_t buffer[2];
	int result;
	result = i2c_write_blocking(handle->i2c, handle->address,  &reg, 1, true);
	//mcp_debug("i2c_write_blocking: %d\n",result);
	if (result == PICO_ERROR_GENERIC) {
		return result;
	}

	result = i2c_read_blocking(handle->i2c, handle->address, buffer, 2, false);
	//mcp_debug("i2c_read_blocking: %d, read: %d,%d\n",result, buffer[0], buffer[1]);
	if (result == PICO_ERROR_GENERIC)
		return result;

	return (buffer[1]<<8) + buffer[0];
}

int setup_bank_configuration(struct i2c_handle *handle, int reg, bool mirroring, bool polarity) {
	int ioConValue = 0;
	set_bit(&ioConValue, MCP23017_IOCON_BANK_BIT, false);
	set_bit(&ioConValue, MCP23017_IOCON_MIRROR_BIT, mirroring);
	set_bit(&ioConValue, MCP23017_IOCON_SEQOP_BIT, false);
	set_bit(&ioConValue, MCP23017_IOCON_DISSLW_BIT, false);
	set_bit(&ioConValue, MCP23017_IOCON_HAEN_BIT, false);
	set_bit(&ioConValue, MCP23017_IOCON_ODR_BIT, false);
	set_bit(&ioConValue, MCP23017_IOCON_INTPOL_BIT, polarity);
	return write_register(handle, reg, ioConValue);
}

int setup(struct i2c_handle *handle, bool mirroring, bool polarity) {
	int result;
	result = setup_bank_configuration(handle, MCP23017_IOCONA, mirroring, polarity);
	if (result != 0)
		return PICO_ERROR_GENERIC;

	result = setup_bank_configuration(handle, MCP23017_IOCONB, mirroring, polarity);
	return result;
}

int set_port_direction(struct i2c_handle *handle, uint8_t port, uint8_t direction) {
	direction ^= 0b11111111; 
	return write_register(handle, port == PORT_A?MCP23017_IODIRA:MCP23017_IODIRB, direction);
}

int write_port(struct i2c_handle *handle, uint8_t port, uint8_t value) {
	return write_register(handle, port == PORT_A?MCP23017_GPIOA:MCP23017_GPIOB, value);
}

int read_port(struct i2c_handle *handle, uint8_t port, uint8_t *value) {
	return read_register(handle, port == PORT_A?MCP23017_GPIOA:MCP23017_GPIOB, value); 
}

int set_pullup(struct i2c_handle *handle, uint8_t port, uint8_t direction) {
	return write_register(handle, port == PORT_A?MCP23017_GPPUA:MCP23017_GPPUB, direction);
}
