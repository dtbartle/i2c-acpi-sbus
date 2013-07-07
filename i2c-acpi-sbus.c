/*
    Copyright 2013 David Bartley <andareed@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
    Implements an i2c bus on top of the ACPI SBUS device.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/i2c.h>

static s32 acpi_sbus_access(struct i2c_adapter *adapter, u16 addr,
			    unsigned short flags, char read_write,
			    u8 command, int size, union i2c_smbus_data *data)
{
	acpi_handle handle = adapter->algo_data;
	char *cmd;
	struct acpi_object_list input;
	union acpi_object in_params[4];
	acpi_status status;
	struct acpi_buffer out_buf = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *out_obj;
	unsigned long long out_int;

	if (size == I2C_SMBUS_BYTE) {
		if (read_write == I2C_SMBUS_READ) {
			cmd = "SRXB";
			input.count = 1;
		} else {
			cmd = "SSXB";
			input.count = 2;
			in_params[1].type = ACPI_TYPE_INTEGER;
			in_params[1].integer.value = command;
		}
	} else {
		switch (size) {
		case I2C_SMBUS_BYTE_DATA:
			if (read_write == I2C_SMBUS_READ) {
				cmd = "SRDB";
				input.count = 2;
			} else {
				cmd = "SWRB";
				input.count = 3;
				in_params[2].type = ACPI_TYPE_INTEGER;
				in_params[2].integer.value = data->byte;
			}
			break;
		case I2C_SMBUS_WORD_DATA:
			if (read_write == I2C_SMBUS_READ) {
				cmd = "SRDW";
				input.count = 2;
			} else {
				cmd = "SWRW";
				input.count = 3;
				in_params[2].type = ACPI_TYPE_INTEGER;
				in_params[2].integer.value = data->word;
			}
			break;
		case I2C_SMBUS_BLOCK_DATA:
			if (read_write == I2C_SMBUS_READ) {
				cmd = "SBLR";
				input.count = 2;
			} else {
				cmd = "SBLW";
				input.count = 3;
				in_params[2].buffer.length = data->block[0];
				in_params[2].buffer.pointer = data->block + 1;
			}
			break;
		default:
			dev_warn(&adapter->dev, "Unsupported size %d\n", size);
			return -EOPNOTSUPP;

		}

		in_params[1].type = ACPI_TYPE_INTEGER;
		in_params[1].integer.value = command;
	}

	in_params[0].type = ACPI_TYPE_INTEGER;
	in_params[0].integer.value = ((addr & 0x7f) << 1) | (read_write & 0x01);
	input.pointer = in_params;


	if (read_write == I2C_SMBUS_WRITE || size != I2C_SMBUS_BLOCK_DATA) {
		status = acpi_evaluate_integer(handle, cmd, &input, &out_int);
	} else {
		status = acpi_evaluate_object(handle, cmd, &input, &out_buf);
	}
	if (ACPI_FAILURE(status))
		return -EIO;
	if (read_write == I2C_SMBUS_WRITE)
		return (out_int == 0) ? -EIO : 0;

	switch (size) {
	case I2C_SMBUS_BYTE:
	case I2C_SMBUS_BYTE_DATA:
		if (out_int > 0xff)
			return -EIO;
		data->byte = out_int;
		break;
	case I2C_SMBUS_WORD_DATA:
		if (out_int > 0xffff)
			return -EIO;
		data->word = out_int;
		break;
	case I2C_SMBUS_BLOCK_DATA:
		if (out_buf.length < 1 || out_buf.length > I2C_SMBUS_BLOCK_MAX)
			return -EPROTO;
		out_obj = out_buf.pointer;
		if (out_obj->type != ACPI_TYPE_BUFFER)
			return -EIO;
		memcpy(data->block, out_obj->buffer.pointer,
		    out_obj->buffer.length);
		break;
	}

	return 0;
}

static u32 acpi_sbus_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA |
	    I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_BLOCK_DATA;
}

static const struct i2c_algorithm smbus_algorithm = {
	.smbus_xfer	= acpi_sbus_access,
	.functionality	= acpi_sbus_func,
};

struct i2c_adapter acpi_sbus = {
	.owner		= THIS_MODULE,
	.class		= I2C_CLASS_HWMON | I2C_CLASS_SPD,
	.algo		= &smbus_algorithm,
	.name		= "ACPI SBUS i2c",
};

static bool added_acpi_sbus;

static acpi_status find_acpi_sbus_devices(acpi_handle handle, u32 level,
				          void *context, void **return_value)
{
	acpi_status status;
	char node_name[5];
	struct acpi_buffer buffer = { sizeof(node_name), node_name };
	struct acpi_device *device = NULL;

	status = acpi_get_name(handle, ACPI_SINGLE_NAME, &buffer);
	if (ACPI_FAILURE(status) || strcmp("SBUS", node_name) != 0)
		return AE_OK;

	if (added_acpi_sbus) {
		pr_warn("ACPI: only one SBUS device supported!\n");
		return AE_ALREADY_EXISTS;
	}

	acpi_bus_get_device(handle, &device);
	if (!device) {
		pr_err("ACPI: failed to get device for SBUS!\n");
		return AE_NOT_FOUND;
	}

	acpi_sbus.dev.parent = &device->dev;
	acpi_sbus.algo_data = handle;

	if (i2c_add_adapter(&acpi_sbus)) {
		pr_err("ACPI: failed to add i2c adapter for SBUS device!\n");
		return AE_ERROR;
	}

	pr_info("ACPI: found SBUS i2c device.\n");
	added_acpi_sbus = true;
	return AE_OK;
}

static int __init i2c_acpi_sbus_init(void)
{
	acpi_status status;

	if (acpi_disabled)
		return 0;

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX, find_acpi_sbus_devices,
				     NULL, NULL, NULL);
	if (status == AE_OK) {
		return 0;
	} else if (status == AE_ALREADY_EXISTS) {
		return -EEXIST;
	} else if (status == AE_NOT_FOUND) {
		return -ENXIO;
	} else {
		return -EIO;
	}

	return 0;
}

static void __exit i2c_acpi_sbus_exit(void)
{
	if (added_acpi_sbus) {
		i2c_del_adapter(&acpi_sbus);
	}
}

MODULE_AUTHOR("David Bartley <andareed@gmail.com>");
MODULE_DESCRIPTION("ACPI SBUS i2c device driver");
MODULE_LICENSE("GPL");

module_init(i2c_acpi_sbus_init);
module_exit(i2c_acpi_sbus_exit);
