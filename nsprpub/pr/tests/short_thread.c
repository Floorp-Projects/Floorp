/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
