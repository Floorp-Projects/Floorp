/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    PRIntn  prflags=0, prmode=0;

    /* Get read/write flags */
    if (strchr(mode, 'r') && !strchr(mode, '+')) {
	prflags |= PR_RDONLY;
    } else if( (strchr(mode, 'w') || strchr(mode, 'a')) &&
	!strchr(mode,'+') ) {
	prflags |= PR_WRONLY;
    } else {
	prflags |= PR_RDWR;
    }

    /* Create a new file? */
    if (strchr(mode, 'w') || strchr(mode, 'a')) {
	prflags |= PR_CREATE_FILE;
    }

    /* Append? */
    if (strchr(mode, 'a')) {
	prflags |= PR_APPEND;
    }

    /* Truncate? */
    if (strchr(mode, 'w')) {
	prflags |= PR_TRUNCATE;
    }

    /* We can't do umask because it isn't XP.  Choose some default
	   mode for created files */
    prmode = 0755;

    return PR_Open(name, prflags, prmode);
}
