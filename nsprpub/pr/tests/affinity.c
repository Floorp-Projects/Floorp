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

#include "nspr.h"
#include "pprthred.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XP_BEOS

/*
 * Test PR_GetThreadAffinityMask
 *		The function is called by each of local, global and global bound threads
 *		The test should be run on both single and multi-cpu systems
 */
static void PR_CALLBACK thread_start(void *arg)
{
PRUint32 mask = 0;

	if (PR_GetThreadAffinityMask(PR_GetCurrentThread(), &mask)) 
		printf("\tthread_start: PR_GetCurrentThreadAffinityMask failed\n");
	else
		printf("\tthread_start: AffinityMask = 0x%x\n",mask);

}

int main(int argc, char **argv)
{
	PRThread *t;

	printf("main: creating local thread\n");

	t = PR_CreateThread(PR_USER_THREAD,
				  thread_start, 0, 
				  PR_PRIORITY_NORMAL,
				  PR_LOCAL_THREAD,
				  PR_JOINABLE_THREAD,
				  0);

	if (NULL == t) {
		printf("main: cannot create local thread\n");
		exit(1);
	}

	PR_JoinThread(t);

	printf("main: creating global thread\n");
	t = PR_CreateThread(PR_USER_THREAD,
				  thread_start, 0, 
				  PR_PRIORITY_NORMAL,
				  PR_GLOBAL_THREAD,
				  PR_JOINABLE_THREAD,
				  0);

	if (NULL == t) {
		printf("main: cannot create global thread\n");
		exit(1);
	}

	PR_JoinThread(t);

	printf("main: creating global bound thread\n");
	t = PR_CreateThread(PR_USER_THREAD,
				  thread_start, 0, 
				  PR_PRIORITY_NORMAL,
				  PR_GLOBAL_BOUND_THREAD,
				  PR_JOINABLE_THREAD,
				  0);

	if (NULL == t) {
		printf("main: cannot create global bound thread\n");
		exit(1);
	}

	PR_JoinThread(t);

    return 0;
}

#else /* !XP_BEOS */

int main()
{
	printf( "This test is not supported on the BeOS\n" );
	return 0;
}
#endif /* !XP_BEOS */
