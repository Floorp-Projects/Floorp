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

#include "nspr.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsHTTreeDataModel.h"
#include "nsWidgetsCID.h"
#include "nsDataModelWidget.h"
#include "nsTreeColumn.h"
#include "nsHTTreeItem.h"
#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsHTDataModel::nsHTDataModel() 
{
	mListener = nsnull;
	mImageGroup = nsnull;

	// Init. one of our hard-coded values
	mSingleNode = new nsHTTreeItem(); // TODO: This is all ridiculously illegal.
	((nsHTTreeItem*)mSingleNode)->SetDataModel((nsHTTreeDataModel*)this); // This is very bad. Just doing to test.
}

//--------------------------------------------------------------------
nsHTDataModel::~nsHTDataModel()
{
	if (mImageGroup)
	{
		mImageGroup->Interrupt();
		NS_RELEASE(mImageGroup);
	}

	// Delete hard-coded value
	delete mSingleNode;
}

void nsHTDataModel::SetDataModelListenerDelegate(nsDataModelWidget* pWidget)
{
	NS_IF_RELEASE(mImageGroup);
	mListener = pWidget;
	if (pWidget != nsnull && NS_NewImageGroup(&mImageGroup) == NS_OK) 
	{
		nsIDeviceContext * deviceCtx = pWidget->GetDeviceContext();
		mImageGroup->Init(deviceCtx, nsnull);
		NS_RELEASE(deviceCtx);
	}
}

// Hierarchical Data Model Implementation ---------------------


nsHierarchicalDataItem* nsHTDataModel::GetRootDelegate() const
{
	return (nsHTTreeItem*)mSingleNode;
}


PRUint32 nsHTDataModel::GetFirstVisibleItemIndexDelegate() const
{
	return 0;
}

void nsHTDataModel::SetFirstVisibleItemIndexDelegate(PRUint32 n)
{
}

nsHierarchicalDataItem* nsHTDataModel::GetNthItemDelegate(PRUint32 n) const
{
	return (nsHTTreeItem*)mSingleNode;
}

void nsHTDataModel::ImageLoaded(nsHierarchicalDataItem* pItem)
{
	if (mListener)
	{
		// Send it on along. Let the view know what happened.
		mListener->HandleDataModelEvent(cDMImageLoaded, pItem);
	}
}