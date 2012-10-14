/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_linux_tst.h"
#include "cpr_errno.h"
#include <stdarg.h>
#include "plat_api.h"

void
err_exit (void)
{
    *(volatile int *) 0xdeadbeef = 0x12345678;
}


/* main process entry function */
int
main (int argc, char **argv, char **env)
{
    int ret;
    char *q;

    buginf("CPR test...\n");
    //err_exit();

    cprTestCmd(argc, argv);

    return 0;
}


long
strlib_mem_used (void)
{
    return (0);
}

#define LOG_MAX 255

int
debugif_printf (const char *_format, ...)
{
    char fmt_buf[LOG_MAX + 1];
    int rc;
    va_list ap;

    va_start(ap, _format);
    rc = vsnprintf(fmt_buf, LOG_MAX, _format, ap);
    va_end(ap);

    if (rc <= 0) {
        return rc;
    }
    printf("%s", fmt_buf);

    return rc;
}

