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
 *  JARNAV.C
 *
 *  JAR stuff needed for client only.
 *
 */

#include "jar.h"
#include "jarint.h"

/* from proto.h */
#ifdef MOZILLA_CLIENT_OLD
extern MWContext *XP_FindSomeContext(void);
#endif

/* sigh */
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
#ifdef MOZILLA_CLIENT_OLD
  JAR_init_callbacks (XP_GetString, XP_FindSomeContext, FE_GetInitContext);
#else
  JAR_init_callbacks (XP_GetString, NULL, NULL);
#endif
  }

/*
 *  J A R _ s e t _ c o n t e x t
 *
 *  Set the jar window context for use by PKCS11, since
 *  it may be needed to prompt the user for a password.
 *
 */

int JAR_set_context (JAR *jar, MWContext *mw)
  {
  if (mw)
    {
    jar->mw = mw;
    }
  else
    {
    /* jar->mw = XP_FindSomeContext(); */
    jar->mw = NULL;

    /*
     * We can't find a context because we're in startup state and none
     * exist yet. go get an FE_InitContext that only works at initialization
     * time.
     */

    /* Turn on the mac when we get the FE_ function */
    if (jar->mw == NULL)
      {
      jar->mw = jar_fn_GetInitContext();
      }
   }

  return 0;
  }
