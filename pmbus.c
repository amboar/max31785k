// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 IBM Corp.

#include "bits.h"
#include "pmbus.h"
#include "smbus.h"

#include <stdint.h>

#define PMBUS_PAGE			0x00

#define PMBUS_FAN_CONFIG_12		0x3a
#define   PMBUS_FAN_CONFIG_1_ENABLED	BIT(7)
#define   PMBUS_FAN_CONFIG_1_MODE	BIT(6)
#define   PMBUS_FAN_CONFIG_1_PULSE	GENMASK(5, 4)
#define   PMBUS_FAN_CONFIG_2_ENABLED	BIT(3)
#define   PMBUS_FAN_CONFIG_2_MODE	BIT(2)
#define   PMBUS_FAN_CONFIG_2_PULSE	GENMASK(1, 0)
#define PMBUS_FAN_COMMAND_1		0x3b
#define PMBUS_FAN_COMMAND_2		0x3c
#define PMBUS_FAN_CONFIG_34		0x3d
#define PMBUS_FAN_COMMAND_3		0x3e
#define PMBUS_FAN_COMMAND_4		0x3f

#define PMBUS_STATUS_BYTE		0x78
#define PMBUS_STATUS_WORD		0x79
#define PMBUS_STATUS_CML		0x7e
#define PMBUS_STATUS_OTHER		0x7f
#define PMBUS_STATUS_FANS_12		0x81
#define PMBUS_STATUS_FANS_34		0x82

#define PMBUS_READ_FAN_SPEED_1		0x90
#define PMBUS_READ_FAN_SPEED_2		0x91
#define PMBUS_READ_FAN_SPEED_3		0x92
#define PMBUS_READ_FAN_SPEED_4		0x93

static const uint8_t pmbus_fan_config_reg_map[] = {
	[pmbus_fan_1] = PMBUS_FAN_CONFIG_12,
	[pmbus_fan_2] = PMBUS_FAN_CONFIG_12,
	[pmbus_fan_3] = PMBUS_FAN_CONFIG_34,
	[pmbus_fan_4] = PMBUS_FAN_CONFIG_34,
};

static const uint8_t pmbus_fan_config_enabled_map[] = {
	[pmbus_fan_1] = PMBUS_FAN_CONFIG_1_ENABLED,
	[pmbus_fan_2] = PMBUS_FAN_CONFIG_2_ENABLED,
	[pmbus_fan_3] = PMBUS_FAN_CONFIG_1_ENABLED,
	[pmbus_fan_4] = PMBUS_FAN_CONFIG_2_ENABLED,
};

static const uint8_t pmbus_fan_config_mode_map[] = {
	[pmbus_fan_1] = PMBUS_FAN_CONFIG_1_MODE,
	[pmbus_fan_2] = PMBUS_FAN_CONFIG_2_MODE,
	[pmbus_fan_3] = PMBUS_FAN_CONFIG_1_MODE,
	[pmbus_fan_4] = PMBUS_FAN_CONFIG_2_MODE,
};

static const uint8_t pmbus_fan_command_reg_map[] = {
	[pmbus_fan_1] = PMBUS_FAN_COMMAND_1,
	[pmbus_fan_2] = PMBUS_FAN_COMMAND_2,
	[pmbus_fan_3] = PMBUS_FAN_COMMAND_3,
	[pmbus_fan_4] = PMBUS_FAN_COMMAND_4,
};

static const uint8_t pmbus_read_fan_speed_reg_map[] = {
	[pmbus_fan_1] = PMBUS_READ_FAN_SPEED_1,
	[pmbus_fan_2] = PMBUS_READ_FAN_SPEED_2,
	[pmbus_fan_3] = PMBUS_READ_FAN_SPEED_3,
	[pmbus_fan_4] = PMBUS_READ_FAN_SPEED_4,
};

int pmbus_read_byte(int fd, uint8_t page, uint8_t reg)
{
	int rc;

	rc = smbus_write_byte(fd, PMBUS_PAGE, page);
	if (rc < 0)
		return rc;

	return smbus_read_byte(fd, reg);
}

int pmbus_write_byte(int fd, uint8_t page, uint8_t reg, uint8_t val)
{
	int rc;

	rc = smbus_write_byte(fd, PMBUS_PAGE, page);
	if (rc < 0)
		return rc;

	return smbus_write_byte(fd, reg, val);
}

int pmbus_read_word(int fd, uint8_t page, uint8_t reg)
{
	int rc;

	rc = smbus_write_byte(fd, PMBUS_PAGE, page);
	if (rc < 0)
		return rc;

	return smbus_read_word(fd, reg);
}

int pmbus_write_word(int fd, uint8_t page, uint8_t reg, uint16_t val)
{
	int rc;

	rc = smbus_write_byte(fd, PMBUS_PAGE, page);
	if (rc < 0)
		return rc;

	return smbus_write_word(fd, reg, val);
}

int pmbus_fan_config_get_enabled(int fd, uint8_t page, enum pmbus_fan fan)
{
	uint8_t reg, flag;
	int rc;

	reg = pmbus_fan_config_reg_map[fan];
	flag = pmbus_fan_config_enabled_map[fan];

	rc = pmbus_read_byte(fd, page, reg);
	if (rc < 0)
		return rc;

	return !!(rc & flag);
}

int pmbus_fan_config_get_mode(int fd, uint8_t page, enum pmbus_fan fan)
{
	uint8_t reg, flag;
	int rc;

	reg = pmbus_fan_config_reg_map[fan];
	flag = pmbus_fan_config_mode_map[fan];

	rc = pmbus_read_byte(fd, page, reg);
	if (rc < 0)
		return rc;

	return rc & flag ? pmbus_fan_mode_rpm : pmbus_fan_mode_pwm;
}

int pmbus_fan_config_set_mode(int fd, uint8_t page, enum pmbus_fan fan,
			      enum pmbus_fan_mode mode)
{
	uint8_t reg, flag, val;
	int rc;

	reg = pmbus_fan_config_reg_map[fan];
	flag = pmbus_fan_config_mode_map[fan];

	rc = pmbus_read_byte(fd, page, reg);
	if (rc < 0)
		return rc;

	val = rc;
	val &= ~flag;
	val |= mode * flag;

	return pmbus_write_byte(fd, page, reg, val);
}

int pmbus_fan_command_get(int fd, uint8_t page, enum pmbus_fan fan)
{
	return pmbus_read_word(fd, page, pmbus_fan_command_reg_map[fan]);
}

int pmbus_fan_command_set(int fd, uint8_t page, enum pmbus_fan fan,
			  uint16_t rate)
{
	return pmbus_write_word(fd, page, pmbus_fan_command_reg_map[fan], rate);
}

int pmbus_read_fan_speed(int fd, uint8_t page, enum pmbus_fan fan)
{
	return pmbus_read_word(fd, page, pmbus_read_fan_speed_reg_map[fan]);
}
