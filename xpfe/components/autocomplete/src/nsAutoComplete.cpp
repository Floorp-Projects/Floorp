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
 * Original Author: Jean-Francois Ducarroz (ducarroz@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "prtypes.h"
#include "nsAutoComplete.h"

/******************************************************************************
 * nsAutoCompleteItem
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(nsAutoCompleteItem, nsIAutoCompleteItem)

nsAutoCompleteItem::nsAutoCompleteItem()
{
    NS_INIT_ISUPPORTS();
}

nsAutoCompleteItem::~nsAutoCompleteItem()
{
}

NS_IMETHODIMP nsAutoCompleteItem::GetValue(PRUnichar * *aValue)
{
    if (!aValue) return NS_ERROR_NULL_POINTER;   
    *aValue = mValue.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteItem::SetValue(const PRUnichar * aValue)
{
    mValue = aValue;
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteItem::GetComment(PRUnichar * *aComment)
{
    if (!aComment) return NS_ERROR_NULL_POINTER;
    *aComment = mComment.ToNewUnicode();
    return NS_OK;
}
NS_IMETHODIMP nsAutoCompleteItem::SetComment(const PRUnichar * aComment)
{
    mComment = aComment;
    return NS_OK;
}

/* attribute string className; */
NS_IMETHODIMP nsAutoCompleteItem::GetClassName(char * *aClassName)
{
    if (!aClassName) return NS_ERROR_NULL_POINTER;
    *aClassName = mClassName.ToNewCString();
    return NS_OK;
}
NS_IMETHODIMP nsAutoCompleteItem::SetClassName(const char * aClassName)
{
    mClassName.AssignWithConversion(aClassName);
    return NS_OK;
}

/* attribute nsISupports param; */
NS_IMETHODIMP nsAutoCompleteItem::GetParam(nsISupports * *aParam)
{
    if (!aParam) return NS_ERROR_NULL_POINTER;
    *aParam = mParam;
    NS_IF_ADDREF(*aParam);
    return NS_OK;
}
NS_IMETHODIMP nsAutoCompleteItem::SetParam(nsISupports * aParam)
{
    mParam = aParam;
    return NS_OK;
}


/******************************************************************************
 * nsAutoCompleteResults
 ******************************************************************************/
NS_IMPL_ISUPPORTS1(nsAutoCompleteResults, nsIAutoCompleteResults)

nsAutoCompleteResults::nsAutoCompleteResults() :
    mDefaultItemIndex(0)
{
    NS_NewISupportsArray(getter_AddRefs(mItems));
    NS_INIT_ISUPPORTS();
}

nsAutoCompleteResults::~nsAutoCompleteResults()
{
}

NS_IMETHODIMP nsAutoCompleteResults::GetSearchString(PRUnichar * *aSearchString)
{
    if (!aSearchString) return NS_ERROR_NULL_POINTER;
    *aSearchString = mSearchString.ToNewUnicode();  
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::SetSearchString(const PRUnichar * aSearchString)
{
    mSearchString = aSearchString;
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::GetParam(nsISupports * *aParam)
{
    if (!aParam) return NS_ERROR_NULL_POINTER;
    *aParam = mParam;
    NS_IF_ADDREF(*aParam);
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::SetParam(nsISupports * aParam)
{
    mParam = aParam;
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::GetItems(nsISupportsArray * *aItems)
{
    if (!aItems) return NS_ERROR_NULL_POINTER;
    *aItems = mItems;
    NS_IF_ADDREF(*aItems);   
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::SetItems(nsISupportsArray * aItems)
{
    mItems = aItems;
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::GetDefaultItemIndex(PRInt32 *aDefaultItemIndex)
{
    if (!aDefaultItemIndex) return NS_ERROR_NULL_POINTER;
    *aDefaultItemIndex = mDefaultItemIndex;  
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteResults::SetDefaultItemIndex(PRInt32 aDefaultItemIndex)
{
    mDefaultItemIndex = aDefaultItemIndex;
    return NS_OK;
}
