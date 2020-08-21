// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 IBM Corp.

#include "ds3900.h"
#include "smbus.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

ssize_t smbus_read_byte(int fd, uint8_t reg)
{
	struct ds3900_cmd cmd;
	uint8_t val;
	int rc;

	cmd = ds3900_cmd_packet_read;
	ds3900_packet_op(&cmd, reg, sizeof(val));
	rc = ds3900_xfer(fd, cmd, &val, sizeof(val));
	if (rc < 0)
		return rc;

	return val;
}

ssize_t smbus_write_byte(int fd, uint8_t reg, uint8_t val)
{
	struct ds3900_cmd cmd;

	cmd = ds3900_cmd_packet_write;
	ds3900_packet_op(&cmd, reg, sizeof(val));
	return ds3900_xfer(fd, cmd, &val, sizeof(val));
}

ssize_t smbus_read_word(int fd, uint8_t reg)
{
	struct ds3900_cmd cmd;
	uint16_t val;
	int rc;

	cmd = ds3900_cmd_packet_read;
	ds3900_packet_op(&cmd, reg, sizeof(val));
	rc = ds3900_xfer(fd, cmd, &val, sizeof(val));
	if (rc < 0)
		return rc;

	return le32toh(val);
}

ssize_t smbus_write_word(int fd, uint8_t reg, uint16_t val)
{
	struct ds3900_cmd cmd;

	cmd = ds3900_cmd_packet_write;
	ds3900_packet_op(&cmd, reg, sizeof(val));
	return ds3900_xfer(fd, cmd, &val, sizeof(val));
}

/* Something's wrong with this, all data bytes are 0xff */
ssize_t smbus_read_block(int fd, uint8_t dev, uint8_t reg,
				uint8_t **buf, size_t len)
{
	struct ds3900_cmd cmd;
	uint8_t count;
	int rc;

	if (!buf)
		return -EINVAL;

	if (!(*buf) && len)
		return -EINVAL;

	/* Start */
	rc = ds3900_xfer(fd, ds3900_cmd_2wire_start, NULL, 0);
	if (rc < 0)
		return rc;

	/* Address with Write*/
	cmd = ds3900_cmd_2wire_write_byte;
	cmd.cmd.data = (dev << 1) | 0;
	rc = ds3900_xfer(fd, cmd, NULL, 0);
	if (rc < 0)
		goto cleanup_bus;

	/* Command Code */
	cmd = ds3900_cmd_2wire_write_byte;
	cmd.cmd.data = reg;
	rc = ds3900_xfer(fd, cmd, NULL, 0);
	if (rc < 0)
		goto cleanup_bus;

	/* Start Repeat */
	rc = ds3900_xfer(fd, ds3900_cmd_2wire_start, NULL, 0);
	if (rc < 0)
		goto cleanup_bus;

	/* Address with Read */
	cmd = ds3900_cmd_2wire_write_byte;
	cmd.cmd.data = (dev << 1) | 1;
	rc = ds3900_xfer(fd, cmd, NULL, 0);
	if (rc < 0)
		goto cleanup_bus;

	/* Count */
	cmd = ds3900_cmd_2wire_read_byte;
	cmd.cmd.data = DS3900_CMD_2WIRE_READ_BYTE_ACK;
	rc = ds3900_xfer(fd, cmd, &count, sizeof(count));
	if (rc < 0)
		goto cleanup_bus;

	/* Allocate buffer space */
	if (count > len) {
		uint8_t *new_buf = realloc(*buf, count);
		if (!new_buf) {
			rc = -ENOMEM;
			goto cleanup_bus;
		}
		*buf = new_buf;
	}

	/* Read data */
	len = count;
	while (len--) {
		cmd = ds3900_cmd_2wire_read_byte;
		cmd.cmd.data = !!len; /* ACK while there's another byte */
		rc = ds3900_xfer(fd, cmd, &(*buf)[count - (len + 1)], sizeof(**buf));
		if (rc < 0)
			goto cleanup_bus;
	}

	rc = ds3900_xfer(fd, ds3900_cmd_2wire_stop, NULL, 0);
	if (!rc)
		return count;

cleanup_bus:
	ds3900_xfer(fd, ds3900_cmd_2wire_recover, NULL, 0);

	return rc;
}
