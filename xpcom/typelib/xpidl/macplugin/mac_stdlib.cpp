/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

/*
	mac_stdlib.cpp

	replacement functions for the CodeWarrior plugin.
	
	by Patrick C. Beard.
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

// simply throw us out of here!

jmp_buf exit_jump;
int exit_status = 0;

void std::exit(int status)
{
	exit_status = status;
	longjmp(exit_jump, -1);
}
