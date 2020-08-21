// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 IBM Corp.

#include "ds3900.h"
#include "smbus.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int do_ds3900_get(int fd, int dev, int reg, size_t width)
{
	uint8_t *data = NULL;
	ssize_t rc;

	switch (width) {
		case 0:
			rc = smbus_read_block(fd, dev, reg, &data, 0);
			break;
		case 1:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = smbus_read_byte(fd, reg);
			break;
		case 2:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = smbus_read_word(fd, reg);
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

			rc = smbus_write_byte(fd, reg, val);
			break;
		case 2:
			rc = ds3900_packet_device_address(fd, dev);
			if (rc < 0)
				return rc;

			rc = smbus_write_word(fd, reg, val);
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

static void help(const char *name)
{
	fprintf(stderr, "USAGE: %s HIDRAW SUBCOMMAND\n", name);
}

static const uint8_t max31785_address = 0x52;

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
	} else if (!strcmp("get", subcmd)) {
		const char *reg_str, *width_str;
		unsigned long reg;
		int width;

		if (argc < 4) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		reg_str = argv[3];
		reg = strtoul(reg_str, NULL, 0);

		if (argc > 4) {
			width_str = argv[4];
			width = smbus_parse_width(width_str);
			if (width < 0) {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}
		} else {
			width = 1;
		}

		rc = do_ds3900_get(fd, max31785_address, reg, width);
	} else if (!strcmp("set", subcmd)) {
		const char *reg_str, *val_str, *width_str;
		unsigned long reg, val;
		int width;

		if (argc < 5) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		reg_str = argv[3];
		reg = strtoul(reg_str, NULL, 0);

		val_str = argv[4];
		val = strtoul(val_str, NULL, 0);

		if (argc > 5) {
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

		rc = do_ds3900_set(fd, max31785_address, reg, val, width);
	} else if (!strcmp("thrash-pages", subcmd)) {
		bool match;
		unsigned i;
		int page;

		if (argc < 3) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		rc = ds3900_packet_device_address(fd, max31785_address);
		if (rc < 0) {
			fprintf(stderr, "Failed to set device address: %s", strerror(-rc));
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		page = 0;
		match = true;
		for (i = 0; match; i++) {
			if (!(i % 100))
				printf("%u\n", i);

			rc = smbus_write_byte(fd, 0, page);
			if (rc < 0) {
				fprintf(stderr, "Failed to set page: %s", strerror(-rc));
				break;
			}
			rc = smbus_read_byte(fd, 0);
			if (rc < 0) {
				fprintf(stderr, "Failed to get page: %s", strerror(-rc));
				break;
			}
			match = rc == page;
			if (!match)
				fprintf(stderr,
					"Page mismatch found at iteration %u: set %u, read %u\n",
					i, page, rc);
			page = (page + 1) % 22;
		}
	} else {
		help(argv[0]);
		rc = EXIT_FAILURE;
		goto cleanup_fd;
	}

cleanup_fd:
	rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;

	close(fd);

	exit(rc);
}
