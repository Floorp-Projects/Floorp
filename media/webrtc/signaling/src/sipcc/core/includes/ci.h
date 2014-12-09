/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CI_INCLUDED_H
#define _CI_INCLUDED_H

#include "plat_api.h"
#include "plat_debug.h"


/* return codes for CI command processing */
#define CI_OK             (0)
#define CI_ERROR          (1)
#define CI_INVALID        (2)
#define CI_AMBIGUOUS      (3)

/* flags for CI processing */
#define CI_PROMPT         (0x0001)

/*
 * Prototypes for public functions
 */
void ci_init();
int ci_process_input(const char *str, char *wkspace, int wklen);
int32_t ci_show_cmds(int32_t argc, const char *argv[]);
ci_callback ci_set_interceptor(ci_callback func);

int ci_err_too_few(void);        /* "Too few arguments"   */
int ci_err_too_many(void);       /* "Too many arguments"  */
int ci_err_inv_arg(void);        /* "Invalid argument"    */
uint32_t ci_streval(const char *str);

#endif /* _CI_INCLUDED_H */
