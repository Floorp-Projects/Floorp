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
//#include "TArray.h"
//#include "TArrayIterator.h"


#include "prthread.h"
#include "prlog.h"
#include "prmem.h"

#include "nsError.h"
#include "nsVoidArray.h"

#include "macglue.h"


#include "nsCOMPtr.h"

#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nspr.h"
#include "nscore.h"
#include "nsError.h"

//#define MAC_PL_EVENT_TWEAKING

// Class IDs...
static NS_DEFINE_CID(kEventQueueCID,  NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,  NS_EVENTQUEUESERVICE_CID);

typedef struct SSMThreadPrivateData
{
	PRThread *thd;
	PRUintn indx;
} SSMThreadPrivateData;



static nsVoidArray*		gThreadsList = NULL;
static PRUintn 			gThreadIndex = 0;



PRUintn GetThreadIndex()
{
	return gThreadIndex;
}

void 
SSM_MacDelistThread(void *priv)
{
    PR_ASSERT(gThreadsList);
	
    PRBool	removed = gThreadsList->RemoveElement(priv);
    PR_ASSERT(removed);
	
    if (gThreadsList->Count() == 0)
    {
        delete gThreadsList;
        gThreadsList = NULL;
    }
}

static PRBool InterruptThreadEnumerator(void* aElement, void *aData)
{
	PRThread* theThread = (PRThread *)aElement;
	
	nsresult rv = PR_Interrupt(theThread); // thread data dtor will deallocate (*priv)
	PR_ASSERT(rv == PR_SUCCESS);
	
	return PR_TRUE;		// continue
}



void
SSM_KillAllThreads(void)
{
	if (gThreadsList)
	{
		gThreadsList->EnumerateForwards(InterruptThreadEnumerator, NULL);
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

	if (!gThreadsList)
	{
		PR_NewThreadPrivateIndex(&gThreadIndex, SSM_MacDelistThread);
		gThreadsList = new nsVoidArray;
	}
	
	PRThread *newThread = PR_CreateThread(type, start, arg, priority, scope, state, stackSize);
	if (newThread)
	{
		/* Add this thread to our list of threads */
		gThreadsList->AppendElement((void *)newThread);
	}
	
	return newThread;
}

void ProcessEventQ(void)
{
}

PRInt32 SetupEventQ(void)
{
	nsresult res = NS_InitXPCOM(NULL, NULL);
	NS_ASSERTION( NS_SUCCEEDED(res), "NS_InitXPCOM failed" );
	
	if (NS_FAILED(res))
		return -1;
	return 0;
}
