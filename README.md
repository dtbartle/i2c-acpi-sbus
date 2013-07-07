i2c-acpi-sbus
=============

ACPI SBUS i2c device driver.

The SBUS ACPI device implements an SMBus device, slightly simplified from the
similar device implemented by the i2c-i801 driver.  I could not find any
documentation but the supported methods are straightforward to understand:

    SSXB - send byte
        Arg0: address << 1
        Arg1: data
        Return: 0 on failure, 1, on success

    SRXB - receive byte
        Arg0: (address << 1) | 0x01

    SWRB - write byte
        Arg0: address << 1
        Arg1: command
        Arg2: data
        Return: 0 on failure, 1, on success

    SRDB - read byte
        Arg0: (address << 1) | 0x01
        Arg1: command
        Return: 0xffff on failure, the read byte otherwise

    SWRW - write word
        Arg0: address << 1
        Arg1: command
        Arg2: data
        Return: 0 on failure, 1, on success

    SRDW - read word
        Arg0: (address << 1) | 0x01
        Arg1: command
        Return: 0xffffffff on failure, the read word otherwise

    SBLW - write block data
        Arg0: address << 1
        Arg1: command
        Arg2: data - data[0] is the number of bytes that follow
        Return: 0 on failure, 1, on success

    SBLR - read block data
        Arg0: (address << 1) | 0x01
        Arg1: command
        Return: 0 on failure, the read data otherwise

When reading data, the ACPI code internally or's the address with 0x01, but to
be safe this is done by the driver as well.

There seems to be no consensus on whether SWRW/SRDW are read in little-endian or
big-endian, as both variants appear in the wild.
