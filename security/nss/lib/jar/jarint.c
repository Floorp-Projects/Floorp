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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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

/*
 * Internal libjar routines.
 */

#include "jar.h"
#include "jarint.h"

/*-----------------------------------------------------------------------
 * JAR_FOPEN_to_PR_Open
 * Translate JAR_FOPEN arguments to PR_Open arguments
 */
PRFileDesc*
JAR_FOPEN_to_PR_Open(const char* name, const char *mode)
{

	PRIntn	prflags=0, prmode=0;

	/* Get read/write flags */
	if(strchr(mode, 'r') && !strchr(mode, '+')) {
		prflags |= PR_RDONLY;
	} else if( (strchr(mode, 'w') || strchr(mode, 'a')) &&
			!strchr(mode,'+') ) {
		prflags |= PR_WRONLY;
	} else {
		prflags |= PR_RDWR;
	}

	/* Create a new file? */
	if(strchr(mode, 'w') || strchr(mode, 'a')) {
		prflags |= PR_CREATE_FILE;
	}

	/* Append? */
	if(strchr(mode, 'a')) {
		prflags |= PR_APPEND;
	}

	/* Truncate? */
	if(strchr(mode, 'w')) {
		prflags |= PR_TRUNCATE;
	}

	/* We can't do umask because it isn't XP.  Choose some default
	   mode for created files */
	prmode = 0755;

	return PR_Open(name, prflags, prmode);
}
