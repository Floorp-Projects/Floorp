/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */


#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>
#include "primpl.h"

extern PRSize _PR_MD_GetRandomNoise( void *buf, PRSize size )
{
    struct timeval tv;
    int n = 0;
    int s;

    GETTIMEOFDAY(&tv);

    if ( size >= 0 ) {
        s = _pr_CopyLowBits((char*)buf+n, size, &tv.tv_usec, sizeof(tv.tv_usec));
        size -= s;
        n += s;
    }
    if ( size >= 0 ) {
        s = _pr_CopyLowBits((char*)buf+n, size, &tv.tv_sec, sizeof(tv.tv_usec));
        size -= s;
        n += s;
    }

    return n;
} /* end _PR_MD_GetRandomNoise() */
