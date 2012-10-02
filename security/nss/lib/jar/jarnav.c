/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  JARNAV.C
 *
 *  JAR stuff needed for client only.
 *
 */

#include "jar.h"
#include "jarint.h"

/* from proto.h */
extern MWContext *FE_GetInitContext(void);

/* To return an MWContext for Java */
static MWContext *(*jar_fn_FindSomeContext) (void) = NULL;

/* To fabricate an MWContext for FE_GetPassword */
static MWContext *(*jar_fn_GetInitContext) (void) = NULL;

/*
 *  J A R _ i n i t
 *
 *  Initialize the JAR functions.
 *
 */

void JAR_init (void)
{
    JAR_init_callbacks (XP_GetString, NULL, NULL);
}

/*
 *  J A R _ s e t _ c o n t e x t
 *
 *  Set the jar window context for use by PKCS11, since
 *  it may be needed to prompt the user for a password.
 *
 */
int 
JAR_set_context(JAR *jar, MWContext *mw)
{
    if (mw) {
	jar->mw = mw;
    } else {
	/* jar->mw = XP_FindSomeContext(); */
	jar->mw = NULL;
	/*
	 * We can't find a context because we're in startup state and none
	 * exist yet. go get an FE_InitContext that only works at 
	 * initialization time.
	 */
	/* Turn on the mac when we get the FE_ function */
	if (jar->mw == NULL) {
	    jar->mw = jar_fn_GetInitContext();
	}
    }
    return 0;
}
