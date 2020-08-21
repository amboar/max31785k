// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 IBM Corp.

#include "ds3900.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ds3900_hid_out_report {
	uint8_t nr;
	uint8_t cmd;
	uint8_t data;
	uint8_t tail[];
};

void ds3900_packet_op(struct ds3900_cmd *cmd, uint8_t reg, uint8_t len)
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

int ds3900_xfer(int fd, const struct ds3900_cmd cmd, void *buf, size_t len)
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

	if (egress < 0)
		return -errno;

	if ((size_t)egress != tx_len)
		return -EIO;

	ingress = read(fd, &rx_buf[0], cmd.rsp.len);
	if (ingress < 0)
		return -errno;

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

int ds3900_packet_device_address(int fd, uint8_t dev)
{
	struct ds3900_cmd cmd;

	cmd = ds3900_cmd_packet_device_address;
	cmd.cmd.data = dev << 1;
	return ds3900_xfer(fd, cmd, NULL, 0);
}
