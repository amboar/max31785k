// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 IBM Corp.

#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DS3900_RSP_BAD	0xfa

struct ds3900_cmd {
	struct {
		uint8_t cmd;
		uint8_t data;
	} cmd;
	struct {
		uint8_t rsp;
		uint8_t len;
	} rsp;
};

struct ds3900_hid_out_report {
	uint8_t nr;
	uint8_t cmd;
	uint8_t data;
	uint8_t tail[];
};

static void ds3900_packet_op(struct ds3900_cmd *cmd, uint8_t reg, uint8_t len)
{
	cmd->cmd.cmd |= (len - 1) & 0x0f;
	cmd->rsp.rsp |= (len - 1) & 0x0f;
	cmd->cmd.data = reg;
	if ((cmd->cmd.cmd & 0xf0) == 0x90)
		cmd->rsp.len = len + 1;
}

const struct ds3900_cmd ds3900_cmd_packet_write = {
	.cmd = { .cmd = 0x80, .data = 0x00, },
	.rsp = { .rsp = 0x80, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_packet_read = {
	.cmd = { .cmd = 0x90, .data = 0x00, },
	.rsp = { .rsp = 0x90, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_2wire_start = {
	.cmd = { .cmd = 0xa0, .data = 0x00, },
	.rsp = { .rsp = 0xb0, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_2wire_start_repeat = {
	.cmd = { .cmd = 0xa0, .data = 0x00, },
	.rsp = { .rsp = 0xb0, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_2wire_write_byte = {
	.cmd = { .cmd = 0xa1, .data = 0x00, },
	.rsp = { .rsp = 0xb1, .len = 1 },
};

#define DS3900_CMD_2WIRE_READ_BYTE_NACK		0x00
#define DS3900_CMD_2WIRE_READ_BYTE_ACK		0x01

const struct ds3900_cmd ds3900_cmd_2wire_read_byte = {
	.cmd = { .cmd = 0xa2, .data = 0x00, },
	.rsp = { .rsp = 0xb2, .len = 2 },
};

const struct ds3900_cmd ds3900_cmd_2wire_stop = {
	.cmd = { .cmd = 0xa3, .data = 0x00, },
	.rsp = { .rsp = 0xb3, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_2wire_recover = {
	.cmd = { .cmd = 0xa4, .data = 0x00, },
	.rsp = { .rsp = 0xb4, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_packet_device_address = {
	.cmd = { .cmd = 0xa5, .data = 0x00, },
	.rsp = { .rsp = 0xb5, .len = 1 },
};

const struct ds3900_cmd ds3900_cmd_read_revision = {
	.cmd = { .cmd = 0xc2, .data = 0x00, },
	.rsp = { .rsp = 0xd2, .len = 3 },
};

static void help(const char *name)
{
	fprintf(stderr, "USAGE: %s SUBCOMMAND HIDRAW\n", name);
}

static int ds3900_xfer(int fd, const struct ds3900_cmd cmd, void *buf,
		       size_t len)
{
	struct ds3900_hid_out_report *tx, _tx;
	ssize_t egress, ingress;
	uint8_t rx_buf[16 + 1];
	bool is_packet_write;
	size_t tx_len;

	if (cmd.rsp.len > (sizeof(rx_buf) - 1))
		return -EINVAL;

	if (len == SIZE_MAX)
		return -EINVAL;

	if (cmd.rsp.len > (len + 1))
		return -EINVAL;

	if (!buf && len > 0)
		return -EINVAL;

	is_packet_write = ((cmd.cmd.cmd & 0xf0) == 0x80);

	if (is_packet_write) {
		tx_len = sizeof(*tx) + len;
		tx = malloc(tx_len);
		if (!tx)
			return -ENOMEM;
		memcpy(tx->tail, buf, len);
	} else {
		tx_len = sizeof(*tx);
		tx = &_tx;
	}

	tx->nr = 0;
	tx->cmd = cmd.cmd.cmd;
	tx->data = cmd.cmd.data;

	egress = write(fd, tx, tx_len);

	if (is_packet_write)
		free(tx);

	if (egress < 0) {
		perror("write");
		return -EIO;
	}

	if ((size_t)egress != tx_len) {
		return -EIO;
	}

	ingress = read(fd, &rx_buf[0], cmd.rsp.len);
	if (ingress < 0) {
		perror("read");
		return -EIO;
	}

	if (ingress != cmd.rsp.len)
		return -EIO;

	if (rx_buf[cmd.rsp.len - 1] == DS3900_RSP_BAD)
		return -EBADMSG;

	if (rx_buf[cmd.rsp.len - 1] != cmd.rsp.rsp)
		return -EBADE;

	if (buf)
		memcpy(buf, rx_buf, len);

	return 0;
}

static ssize_t ds3900_smbus_read_byte(int fd, uint8_t reg)
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

static ssize_t ds3900_smbus_write_byte(int fd, uint8_t reg, uint8_t val)
{
	struct ds3900_cmd cmd;

	cmd = ds3900_cmd_packet_write;
	ds3900_packet_op(&cmd, reg, sizeof(val));
	return ds3900_xfer(fd, cmd, &val, sizeof(val));
}

static ssize_t ds3900_smbus_read_word(int fd, uint8_t reg)
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

static ssize_t ds3900_smbus_write_word(int fd, uint8_t reg, uint16_t val)
{
	struct ds3900_cmd cmd;

	cmd = ds3900_cmd_packet_write;
	ds3900_packet_op(&cmd, reg, sizeof(val));
	return ds3900_xfer(fd, cmd, &val, sizeof(val));
}

/* Something's wrong with this, all data bytes are 0xff */
static ssize_t ds3900_smbus_read_block(int fd, uint8_t dev, uint8_t reg,
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

	printf("Receiving %d bytes\n", count);

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
	{
		int cleanup;

		cleanup = ds3900_xfer(fd, ds3900_cmd_2wire_recover, NULL, 0);
		if (cleanup < 0)
			fprintf(stderr, "Bus recovery failed: %s\n",
				strerror(-cleanup));
	}

	return rc;
}

static int do_ds3900_revision(int fd)
{
	uint8_t buf[2];
	int rc;

	rc = ds3900_xfer(fd, ds3900_cmd_read_revision, &buf[0], sizeof(buf));
	if (rc < 0) {
		fprintf(stderr, "Transfer failure: %d\n", rc);
		return rc;
	}

	printf("DS3900 revision: %d.%d\n", buf[0], buf[1]);

	return 0;
}

static int ds3900_packet_device_address(int fd, uint8_t dev)
{
	struct ds3900_cmd cmd;

	cmd = ds3900_cmd_packet_device_address;
	cmd.cmd.data = dev << 1;
	return ds3900_xfer(fd, cmd, NULL, 0);
}

static int do_ds3900_get(int fd, int dev, int reg, size_t width)
{
	uint8_t *data = NULL;
	ssize_t rc;

	switch (width) {
		case 0:
			rc = ds3900_smbus_read_block(fd, dev, reg, &data, 0);
			break;
		case 1:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = ds3900_smbus_read_byte(fd, reg);
			break;
		case 2:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = ds3900_smbus_read_word(fd, reg);
			break;
		default:
			return -EINVAL;
	}

	if (rc < 0) {
		fprintf(stderr, "Transfer failure: %ld\n", rc);
		return rc;
	}

	if (width == 1) {
		printf("0x%x: 0x%02lx\n", reg, rc);
	} else if (width == 2) {
		printf("0x%x: 0x%04lx\n", reg, rc);
	} else {
		int i;

		if (!data)
			return 0;

		i = 0;
		while (i < rc) {
			int j;

			printf("0x%02x: ", i);

			for (j = 0; i < rc && j < 16; i++, j++) {
				if (isprint(data[i]))
				printf(" %c%s", data[i], rc ? " " : "");
				else
					printf("%02x%s", data[i], rc ? " " : "");
			}

			printf("\n");
		}
	}

	return 0;
}

static int do_ds3900_set(int fd, int dev, int reg, int val, size_t width)
{
	int rc;

	switch (width) {
		case 1:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = ds3900_smbus_write_byte(fd, reg, val);
			break;
		case 2:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = ds3900_smbus_write_word(fd, reg, val);
			break;
		default:
			return -EINVAL;
	}

	if (rc < 0) {
		fprintf(stderr, "Transfer failure: %d\n", rc);
		return rc;
	}

	return 0;
}

static int smbus_parse_width(const char *width)
{
	if (!strlen(width))
		return -EINVAL;

	switch (width[0]) {
		case 'b':
			return 1;
		case 'w':
			return 2;
		case 's':
			return 0;
		default:
			return -EINVAL;
	}
}

int main(int argc, const char *argv[])
{
	const char *subcmd;
	const char *path;
	int fd;
	int rc;

	if (argc < 3) {
		help(argv[0]);
		exit(EXIT_FAILURE);
	}

	path = argv[1];
	subcmd = argv[2];

	fd = open(path, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	if (!strcmp("revision", subcmd)) {
		rc = do_ds3900_revision(fd);
	} else if (!strcmp("device", subcmd)) {
		const char *dev_str;
		unsigned long dev;

		if (argc < 4) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		dev_str = argv[3];
		dev = strtoul(dev_str, NULL, 0);
		rc = ds3900_packet_device_address(fd, dev);
	} else if (!strcmp("get", subcmd)) {
		const char *dev_str, *reg_str, *width_str;
		unsigned long dev, reg;
		int width;

		if (argc < 5) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		dev_str = argv[3];
		dev = strtoul(dev_str, NULL, 0);

		reg_str = argv[4];
		reg = strtoul(reg_str, NULL, 0);

		if (argc > 5) {
			width_str = argv[5];
			width = smbus_parse_width(width_str);
			if (width < 0) {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}
		} else {
			width = 1;
		}

		rc = do_ds3900_get(fd, dev, reg, width);
	} else if (!strcmp("set", subcmd)) {
		const char *dev_str, *reg_str, *val_str, *width_str;
		unsigned long dev, reg, val;
		int width;

		if (argc < 6) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		dev_str = argv[3];
		dev = strtoul(dev_str, NULL, 0);

		reg_str = argv[4];
		reg = strtoul(reg_str, NULL, 0);

		val_str = argv[5];
		val = strtoul(val_str, NULL, 0);

		if (argc > 6) {
			width_str = argv[6];
			width = smbus_parse_width(width_str);
			if (width < 0) {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}
		} else {
			width = 1;
		}

		rc = do_ds3900_set(fd, dev, reg, val, width);
	} else {
		help(argv[0]);
		rc = EXIT_FAILURE;
		goto cleanup_fd;
	}

	rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;

cleanup_fd:
	close(fd);

	exit(rc);
}
