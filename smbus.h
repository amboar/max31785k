/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2020 IBM Corp. */

#include <stdint.h>
#include <sys/types.h>

ssize_t smbus_read_byte(int fd, uint8_t reg);
ssize_t smbus_write_byte(int fd, uint8_t reg, uint8_t val);
ssize_t smbus_read_word(int fd, uint8_t reg);
ssize_t smbus_write_word(int fd, uint8_t reg, uint16_t val);
ssize_t smbus_read_block(int fd, uint8_t dev, uint8_t reg, uint8_t **buf,
			 size_t len);
