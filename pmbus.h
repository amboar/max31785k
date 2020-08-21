/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2020 IBM Corp. */

#include <stdint.h>

enum pmbus_fan_mode { pmbus_fan_mode_pwm, pmbus_fan_mode_rpm };
enum pmbus_fan { pmbus_fan_1 = 1, pmbus_fan_2, pmbus_fan_3, pmbus_fan_4 };

int pmbus_read_byte(int fd, uint8_t page, uint8_t reg);
int pmbus_write_byte(int fd, uint8_t page, uint8_t reg, uint8_t val);
int pmbus_read_word(int fd, uint8_t page, uint8_t reg);
int pmbus_write_word(int fd, uint8_t page, uint8_t reg, uint16_t val);

int pmbus_fan_config_get_enabled(int fd, uint8_t page, enum pmbus_fan fan);
int pmbus_fan_config_get_mode(int fd, uint8_t page, enum pmbus_fan fan);
int pmbus_fan_config_set_mode(int fd, uint8_t page, enum pmbus_fan fan,
			      enum pmbus_fan_mode mode);
int pmbus_fan_command_get(int fd, uint8_t page, enum pmbus_fan fan);
int pmbus_fan_command_set(int fd, uint8_t page, enum pmbus_fan fan,
			  uint16_t rate);
int pmbus_read_fan_speed(int fd, uint8_t page, enum pmbus_fan fan);
