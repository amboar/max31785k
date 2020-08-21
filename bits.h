/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2020 IBM Corp. */

#define BIT(x) (1UL << (x))
#define GENMASK(h, l) (((2UL << (h)) - 1) & ~((2UL << (l)) - 1))
