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
#include "TArray.h"
#include "TArrayIterator.h"
#include "prthread.h"
#include "prlog.h"
#include "prmem.h"


typedef struct SSMThreadPrivateData
{
	PRThread *thd;
	PRUintn indx;
} SSMThreadPrivateData;

TArray<PRThread *>* myThreads = NULL;
PRUintn thdIndex = 0;

void 
SSM_MacDelistThread(void *priv)
{
	// remove this lump from the list of active threads
	myThreads->Remove((PRThread *) priv);
}

void
SSM_KillAllThreads(void)
{
	int i;
	SSMThreadPrivateData *priv = NULL;
	PRStatus rv;

	if (myThreads != nil) {
		TArrayIterator<PRThread*> iterator(*myThreads);
		PRThread	*thd;
		while (iterator.Next(thd)) {
			rv = PR_Interrupt(thd); // thread data dtor will deallocate (*priv)
			PR_ASSERT(rv == PR_SUCCESS);
		}
	}
}

PRThread *
SSM_CreateAndRegisterThread(PRThreadType type,
                     void (*start)(void *arg),
                     void *arg,
                     PRThreadPriority priority,
                     PRThreadScope scope,
                     PRThreadState state,
                     PRUint32 stackSize)
{
	PRThread *thd;

	if (!myThreads)
	{
		PR_NewThreadPrivateIndex(&thdIndex, SSM_MacDelistThread);
		myThreads = new TArray<PRThread*>;
	}
	
	thd = PR_CreateThread(type, start, arg, priority, scope, state, stackSize);
	if (thd)
	{
		/* Add this thread to our list of threads */
		myThreads->AddItem(thd);
	}
}

