/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsRDFTreeColumn.h"

static NS_DEFINE_IID(kITreeColumnIID, NS_ITREECOLUMN_IID);

const PRUint32 nsRDFTreeColumn::kDefaultWidth = 64; // XXX

////////////////////////////////////////////////////////////////////////

nsRDFTreeColumn::nsRDFTreeColumn(const nsString& name)
    : mName(name),
      mWidth(kDefaultWidth),
      mSortState(eColumnSortState_Unsorted),
      mDesiredPercentage(0.0)
{
    NS_INIT_REFCNT();
}


nsRDFTreeColumn::~nsRDFTreeColumn(void)
{
}


NS_IMPL_ADDREF(nsRDFTreeColumn);
NS_IMPL_RELEASE(nsRDFTreeColumn);
NS_IMPL_QUERY_INTERFACE(nsRDFTreeColumn, kITreeColumnIID);


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsRDFTreeColumn::GetPixelWidth(PRUint32& width) const
{
    width = mWidth;
    return NS_OK;
}



NS_IMETHODIMP
nsRDFTreeColumn::GetDesiredPercentage(double& percentage) const
{
    percentage = mDesiredPercentage;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeColumn::GetSortState(nsColumnSortState& answer) const
{
    answer = mSortState;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeColumn::GetColumnName(nsString& name) const
{
    name = mName;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFTreeColumn::SetPixelWidth(PRUint32 newWidth)
{
    mWidth = newWidth;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

void
nsRDFTreeColumn::SetVisibility(PRBool visible)
{
    mVisible = visible;
}


PRBool
nsRDFTreeColumn::IsVisible(void) const
{
    return mVisible;
}

