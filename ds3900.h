/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2020 IBM Corp. */

#include <stdint.h>
#include <sys/types.h>

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

extern const struct ds3900_cmd ds3900_cmd_packet_write;
extern const struct ds3900_cmd ds3900_cmd_packet_read;
extern const struct ds3900_cmd ds3900_cmd_packet_device_address;

#define DS3900_CMD_2WIRE_READ_BYTE_NACK		0x00
#define DS3900_CMD_2WIRE_READ_BYTE_ACK		0x01
extern const struct ds3900_cmd ds3900_cmd_2wire_start;
extern const struct ds3900_cmd ds3900_cmd_2wire_start_repeat;
extern const struct ds3900_cmd ds3900_cmd_2wire_write_byte;
extern const struct ds3900_cmd ds3900_cmd_2wire_read_byte;
extern const struct ds3900_cmd ds3900_cmd_2wire_stop;
extern const struct ds3900_cmd ds3900_cmd_2wire_recover;

extern const struct ds3900_cmd ds3900_cmd_read_revision;

void ds3900_packet_op(struct ds3900_cmd *cmd, uint8_t reg, uint8_t len);
int ds3900_xfer(int fd, const struct ds3900_cmd cmd, void *buf, size_t len);
int ds3900_packet_device_address(int fd, uint8_t dev);
