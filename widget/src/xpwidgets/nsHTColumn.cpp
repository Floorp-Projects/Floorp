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
#include "nsTreeColumn.h"
#include "nsHTColumn.h"
#include "nsIContent.h"
#include "nsHTDataModel.h"

nsHTColumn::nsHTColumn(nsIContent* pContent) : nsTreeColumn()
{
  mPixelWidth = 25;
  mDesiredPercentage = 0.33;
  mContentNode = pContent;
  NS_ADDREF(mContentNode);
}

//--------------------------------------------------------------------
nsHTColumn::~nsHTColumn()
{
	NS_IF_RELEASE(mContentNode);
}


// TreeColumn Implementation ---------------------
int nsHTColumn::GetPixelWidth() const
{
	return mPixelWidth;
}

double nsHTColumn::GetDesiredPercentage() const
{
	return mDesiredPercentage;
}

void nsHTColumn::GetColumnName(nsString& name) const
{
	// Need to look at our content node and get its tag name.
	nsIAtom* pAtom = nsnull;
	mContentNode->GetTag(pAtom);
	pAtom->ToString(name);
	NS_IF_RELEASE(pAtom);
}

void nsHTColumn::GetColumnDisplayText(nsString& displayText) const
{
	nsHTDataModel::GetChildTextForNode(mContentNode, displayText);
}

PRBool nsHTColumn::IsSortColumn() const
{
	return PR_FALSE;
}

void nsHTColumn::SetPixelWidth(int newWidth)
{
	mPixelWidth = newWidth;
}

void nsHTColumn::SetDesiredPercentage(double newPercentage)
{
	mDesiredPercentage = newPercentage;
}
