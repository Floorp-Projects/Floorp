/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#include <string.h>

/*
** fprintf to a PRFileDesc
*/
PR_IMPLEMENT(PRUint32) PR_fprintf(PRFileDesc* fd, const char *fmt, ...)
{
    va_list ap;
    PRUint32 rv;

    va_start(ap, fmt);
    rv = PR_vfprintf(fd, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(PRUint32) PR_vfprintf(PRFileDesc* fd, const char *fmt, va_list ap)
{
    /* XXX this could be better */
    PRUint32 rv, len;
    char* msg = PR_vsmprintf(fmt, ap);
    if (NULL == msg) {
        return -1;
    }
    len = strlen(msg);
#ifdef XP_OS2
    /*
     * OS/2 really needs a \r for every \n.
     * In the future we should try to use scatter-gather instead of a
     * succession of PR_Write.
     */
    if (isatty(PR_FileDesc2NativeHandle(fd))) {
        PRUint32 last = 0, idx;
        PRInt32 tmp;
        rv = 0;
        for (idx = 0; idx < len+1; idx++) {
            if ((idx - last > 0) && (('\n' == msg[idx]) || (idx == len))) {
                tmp = PR_Write(fd, msg + last, idx - last);
                if (tmp >= 0) {
                    rv += tmp;
                }
                last = idx;
            }
            /*
             * if current character is \n, and
             * previous character isn't \r, and
             * next character isn't \r
             */
            if (('\n' == msg[idx]) &&
                ((0 == idx) || ('\r' != msg[idx-1])) &&
                ('\r' != msg[idx+1])) {
                /* add extra \r */
                tmp = PR_Write(fd, "\r", 1);
                if (tmp >= 0) {
                    rv += tmp;
                }
            }
        }
    } else {
        rv = PR_Write(fd, msg, len);
    }
#else
    rv = PR_Write(fd, msg, len);
#endif
    PR_DELETE(msg);
    return rv;
}
