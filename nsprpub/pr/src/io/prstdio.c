/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
