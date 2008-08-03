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

#include <stdio.h>
#include "nspr.h"
#include "plgetopt.h"

/*
 * Create a thread that exits right away; useful for testing race conditions in thread
 * creation
 */

int _debug_on = 0;
#define DPRINTF(arg) if (_debug_on) printf arg

static void housecleaning(void *cur_time);

int main (int argc, char **argv)
{
	static PRIntervalTime thread_start_time;
	static PRThread *housekeeping_tid = NULL;
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "d");

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
	{
		if (PL_OPT_BAD == os) continue;
		switch (opt->option)
		{
			case 'd':  /* debug mode */
				_debug_on = 1;
				break;
			default:
				break;
		}
	}
	PL_DestroyOptState(opt);

	if (( housekeeping_tid = 
		PR_CreateThread (PR_USER_THREAD, housecleaning,  (void*)&thread_start_time,
						 PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0)) 
																		== NULL ) {
		fprintf(stderr,
			"simple_test: Error - PR_CreateThread failed: (%ld, %ld)\n",
									  PR_GetError(), PR_GetOSError());
		exit( 1 );
	}
	PR_Cleanup();
	return(0);
}

static void
housecleaning (void *cur_time) 
{
  DPRINTF(("Child Thread exiting\n"));
}
