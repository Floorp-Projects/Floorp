/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

void nsDeleteObserved::AddDeleteObserver(nsDeleteObserver* aDeleteObserver)
{
	if (! mDeleteObserverArray)
		mDeleteObserverArray = new nsVoidArray();

	if (mDeleteObserverArray)
		mDeleteObserverArray->AppendElement(aDeleteObserver);
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

void nsDeleteObserved::RemoveDeleteObserver(nsDeleteObserver* aDeleteObserver)
{
	if (mDeleteObserverArray)
		mDeleteObserverArray->RemoveElement(aDeleteObserver);
}
