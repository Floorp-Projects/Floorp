/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
