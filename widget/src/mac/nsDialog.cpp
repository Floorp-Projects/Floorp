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

#include "nsDialog.h"

NS_IMPL_ADDREF(nsDialog);
NS_IMPL_RELEASE(nsDialog);

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsDialog::nsDialog() : nsMacWindow(), nsIDialog()
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "nsDialog");

  mIsDialog = PR_TRUE;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsDialog::~nsDialog()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsresult nsDialog::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
	if (NULL == aInstancePtr) {
	    return NS_ERROR_NULL_POINTER;
	}

	static NS_DEFINE_IID(kIDialog, NS_IDIALOG_IID);
	if (aIID.Equals(kIDialog)) {
	    *aInstancePtr = (void*) ((nsIDialog*)this);
	    AddRef();
	    return NS_OK;
	}

	return nsMacWindow::QueryInterface(aIID,aInstancePtr);
}

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::SetLabel(const nsString& aText)
{
	return SetTitle(aText);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::GetLabel(nsString& aBuffer)
{
	Str255 title;
	::GetWTitle(mWindowPtr, title);
	if (title[0] >= sizeof(title) - 1)
		title[0] = sizeof(title) - 1;
	title[title[0] + 1] = '\0';

	aBuffer = (char*)&title[1];
  return NS_OK;
}

