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
 * Portions created by the Initial Developer are Copyright (C) 2000
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

/*
 * Test whether classic NSPR's select() wrapper properly blocks
 * the periodic SIGALRM clocks.  On some platforms (such as
 * HP-UX and SINIX) an interrupted select() system call is
 * restarted with the originally specified timeout, ignoring
 * the time that has elapsed.  If a select() call is interrupted
 * repeatedly, it will never time out.  (See Bugzilla bug #39674.)
 */

#if !defined(XP_UNIX)

/*
 * This test is applicable to Unix only.
 */

int main()
{
    return 0;
}

#else /* XP_UNIX */

#include "nspr.h"

#include <sys/time.h>
#include <stdio.h>

int main()
{
    struct timeval timeout;
    int rv;

    PR_SetError(0, 0);  /* force NSPR to initialize */
    PR_EnableClockInterrupts();

    /* 2 seconds timeout */
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    rv = select(1, NULL, NULL, NULL, &timeout);
    printf("select returned %d\n", rv);
    return 0;
}

#endif /* XP_UNIX */
