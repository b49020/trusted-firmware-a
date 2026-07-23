/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CMD_DB_H
#define CMD_DB_H

#include <stdint.h>

/*
 * cmd_db_query_addr - look up a resource address in the RPMh command DB.
 *
 * @res_id: null-terminated resource identifier string (max 8 chars, e.g.
 *          "cx.lvl", "mx.lvl", "xo.lvl", "vrm.soc").
 *
 * Returns the TCS address (slave ID + offset) on success, 0 on failure.
 */
uint32_t cmd_db_query_addr(const char *res_id);

/*
 * cmd_db_query_len - return the length in bytes of a resource's aux data.
 *
 * @res_id: null-terminated resource identifier string (max 8 chars).
 *
 * Returns the aux-data length on success, 0 if the resource is not found.
 */
uint32_t cmd_db_query_len(const char *res_id);

/*
 * cmd_db_query_aux_data - copy a resource's aux data into a caller buffer.
 *
 * @res_id: null-terminated resource identifier string (max 8 chars).
 * @len:    in/out - on entry, the size of @buf; on success, the number of
 *          bytes copied (min of @buf size and the stored aux length).
 * @buf:    destination buffer for the aux data.
 *
 * Returns 0 on success, -1 on invalid arguments or if the resource is not
 * found.
 */
int cmd_db_query_aux_data(const char *res_id, uint8_t *len, uint8_t *buf);

#endif /* CMD_DB_H */
