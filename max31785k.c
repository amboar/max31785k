// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 IBM Corp.

#include "ds3900.h"
#include "pmbus.h"
#include "smbus.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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
	} else if (!strcmp("fan", subcmd)) {
		if (argc < 5) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		if (strcmp("speed", argv[3])) {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
		}

		if (!strcmp("get", argv[4])) {
			const char *page_str, *fan_str;
			enum pmbus_fan_mode mode;
			int page, fan;
			int16_t rate;

			if (argc < 7) {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}

			page_str = argv[5];
			page = strtoul(page_str, NULL, 0);

			fan_str = argv[6];
			fan = strtoul(fan_str, NULL, 0);

			rc = ds3900_packet_device_address(fd, max31785_address);
			if (rc < 0) {
				fprintf(stderr, "Failed to set device address: %s", strerror(-rc));
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}

			rc = pmbus_fan_config_get_enabled(fd, page, fan);
			if (rc < 0) {
				fprintf(stderr, "pmbus_fan_config_enabled: %d\n", rc);
				goto cleanup_fd;
			}

			if (!rc) {
				fprintf(stderr, "Fan %d:%d is disabled\n", page, fan);
				goto cleanup_fd;
			}

			rc = pmbus_fan_config_get_mode(fd, page, fan);
			if (rc < 0) {
				fprintf(stderr, "pmbus_fan_config_mode: %d\n", rc);
				goto cleanup_fd;
			}

			mode = rc;

			rc = pmbus_fan_command_get(fd, page, fan);
			if (rc < 0) {
				fprintf(stderr, "pmbus_get_fan_command: %d\n", rc);
				goto cleanup_fd;
			}

			rate = (int16_t)rc;
			if (mode == pmbus_fan_mode_pwm)
				rate /= 100;

			rc = pmbus_read_fan_speed(fd, page, fan);
			if (rc < 0) {
				fprintf(stderr, "pmbus_fan_speed_get: %d\n", rc);
				goto cleanup_fd;
			}

			if (rate < 0)
				printf("Automatic fan control, measured %dRPM\n", rc);
			else
				printf("Commanded %"PRId16"%s, measured %dRPM\n", rate, mode == pmbus_fan_mode_rpm ? "RPM" : "% duty", rc);
			rc = 0;
		} else if (!strcmp("set", argv[4])) {
			const char *page_str, *fan_str, *rate_str;
			char *mode_str;
			enum pmbus_fan_mode mode;
			int page, fan, rate;

			if (argc < 8) {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}

			page_str = argv[5];
			page = strtoul(page_str, NULL, 0);

			fan_str = argv[6];
			fan = strtoul(fan_str, NULL, 0);

			rate_str = argv[7];
			rate = strtoul(rate_str, &mode_str, 0);

			if (!strlen(mode_str)) {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}

			if (!strcasecmp("rpm", mode_str)) {
				mode = pmbus_fan_mode_rpm;
			} else if (!strcasecmp("%", mode_str)) {
				mode = pmbus_fan_mode_pwm;
				rate *= 100;
			} else {
				help(argv[0]);
				rc = EXIT_FAILURE;
				goto cleanup_fd;
			}

			rc = ds3900_packet_device_address(fd, max31785_address);
			if (rc < 0) {
				fprintf(stderr, "Failed to set device address: %s", strerror(-rc));
				goto cleanup_fd;
			}

			rc = pmbus_fan_config_get_enabled(fd, page, fan);
			if (rc < 0) {
				fprintf(stderr, "pmbus_fan_config_enabled: %d\n", rc);
				goto cleanup_fd;
			}

			if (!rc) {
				fprintf(stderr, "Fan %d:%d is disabled\n", page, fan);
				goto cleanup_fd;
			}

			rc = pmbus_fan_config_set_mode(fd, page, fan, mode);
			if (rc < 0) {
				fprintf(stderr, "pmbus_fan_config_set_mode: %d\n", rc);
				goto cleanup_fd;
			}

			rc = pmbus_fan_command_set(fd, page, fan, rate);
			if (rc < 0) {
				fprintf(stderr, "pmbus_fan_config_set_mode: %d\n", rc);
				goto cleanup_fd;
			}

		} else {
			help(argv[0]);
			rc = EXIT_FAILURE;
			goto cleanup_fd;
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
