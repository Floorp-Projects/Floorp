/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
