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

#include "nsError.h"
#include "nsVoidArray.h"

#include "prthread.h"
#include "prlog.h"
#include "prmem.h"


typedef struct SSMThreadPrivateData
{
	PRThread *thd;
	PRUintn indx;
} SSMThreadPrivateData;



static nsVoidArray*		gThreadsList = NULL;
static PRUintn 				gThreadIndex = 0;


#ifdef DEBUG
// just here to satisfy linkage in debug mode. nsTraceRefcnt is a class used for tracking
// memory usage in debug builds. This is fragile, and a hack.
class nsTraceRefcnt
{

  static NS_COM void LogCtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize);

  static NS_COM void LogDtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize);

};

void nsTraceRefcnt::LogCtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize)
{}

void nsTraceRefcnt::LogDtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize)
{}
#endif


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

