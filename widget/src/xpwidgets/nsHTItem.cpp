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
#include "nsHTItem.h"
#include "nsIContent.h"

nsHTItem::nsHTItem(nsIContent* pContent, nsHierarchicalDataModel* pDataModel)
{
	NS_ADDREF(pContent);
    mContentNode = pContent;
	mDataModel = pDataModel;
	mIndentationLevel = 0;
}

//--------------------------------------------------------------------
nsHTItem::~nsHTItem()
{
	NS_IF_RELEASE(mContentNode);
}

PRBool nsHTItem::IsExpandedDelegate() const
{
	nsString attrValue;
	nsresult result = mContentNode->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
	return (result == NS_CONTENT_ATTR_NO_VALUE ||
	        (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));	
}

void nsHTItem::ToggleOpenStateDelegate() 
{
	nsString attrValue;
	// Need to set the attribute's value.
	if (IsExpandedDelegate())
		attrValue = "false";
	else attrValue = "true";

	// Set it and wait for the callback.
	mContentNode->SetAttribute("open", attrValue, PR_TRUE); 
}

PRUint32 nsHTItem::GetIndentationLevelDelegate() const
{
	return mIndentationLevel;
}

void nsHTItem::SetIndentationLevelDelegate(PRUint32 n)
{
	mIndentationLevel = n;
}
