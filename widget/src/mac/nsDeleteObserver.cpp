/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#include "nsDeleteObserver.h"
#include "nsVoidArray.h"


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

nsDeleteObserved::nsDeleteObserved(void* aObject)
{
	mDeleteObserverArray = nsnull;
	mObject = (aObject ? aObject : this);
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

nsDeleteObserved::~nsDeleteObserved()
{
	if (mDeleteObserverArray)
	{
		// notify observers of the delete
		for (PRInt32 i = 0; i < mDeleteObserverArray->Count(); i ++)
		{
			nsDeleteObserver* deleteObserver = static_cast<nsDeleteObserver*>(mDeleteObserverArray->ElementAt(i));
			if (deleteObserver)
				deleteObserver->NotifyDelete(mObject);
		}

		delete mDeleteObserverArray;
		mDeleteObserverArray = nsnull;
	}
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

PRBool nsDeleteObserved::AddDeleteObserver(nsDeleteObserver* aDeleteObserver)
{
	if (! mDeleteObserverArray)
		mDeleteObserverArray = new nsVoidArray();

	if (mDeleteObserverArray)
		return mDeleteObserverArray->AppendElement(aDeleteObserver);
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

PRBool nsDeleteObserved::RemoveDeleteObserver(nsDeleteObserver* aDeleteObserver)
{
	if (mDeleteObserverArray)
		return mDeleteObserverArray->RemoveElement(aDeleteObserver);
	return PR_FALSE;
}
