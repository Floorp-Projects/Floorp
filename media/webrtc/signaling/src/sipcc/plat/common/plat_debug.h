/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PLAT_DEBUG_H__
#define __PLAT_DEBUG_H__
#include "cc_constants.h"
#include "debug.h"

typedef cc_int32_t (*ci_callback)(cc_int32_t argc, const char *argv[]);
typedef struct ci_cmd_block
{
    const char           *cmd;
    ci_callback          func;
    struct ci_cmd_block  *next;
} ci_cmd_block_t;

void debug_bind_keyword(const char *cmd, cc_int32_t *flag_ptr);
void bind_debug_func_keyword(const char *cmd, debug_callback func);
void bind_show_keyword(const char *cmd, show_callback func);
void bind_show_tech_keyword(const char *cmd, show_callback func,
                                    cc_boolean show_tech);
void bind_clear_keyword(const char *cmd, clear_callback func);
void ci_bind_cmd(const char *cmd, ci_callback func, ci_cmd_block_t *blk);

#endif
