/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsAutoComplete.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/ModuleUtils.h"

/******************************************************************************
 * nsAutoCompleteItem
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(nsAutoCompleteItem, nsIAutoCompleteItem)

nsAutoCompleteItem::nsAutoCompleteItem()
{
}

nsAutoCompleteItem::~nsAutoCompleteItem()
{
}

NS_IMETHODIMP nsAutoCompleteItem::GetValue(nsAString& aValue)
{
    aValue.Assign(mValue);
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteItem::SetValue(const nsAString& aValue)
{
    mValue.Assign(aValue);
    return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteItem::GetComment(PRUnichar * *aComment)
{
    if (!aComment) return NS_ERROR_NULL_POINTER;
    *aComment = ToNewUnicode(mComment);
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
    *aClassName = ToNewCString(mClassName);
    return NS_OK;
}
NS_IMETHODIMP nsAutoCompleteItem::SetClassName(const char * aClassName)
{
    mClassName.Assign(aClassName);
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
    mItems(do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID)),
    mDefaultItemIndex(0)
{
}

nsAutoCompleteResults::~nsAutoCompleteResults()
{
}

NS_IMETHODIMP nsAutoCompleteResults::GetSearchString(PRUnichar * *aSearchString)
{
    if (!aSearchString) return NS_ERROR_NULL_POINTER;
    *aSearchString = ToNewUnicode(mSearchString);  
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

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteResults)

NS_DEFINE_NAMED_CID(NS_AUTOCOMPLETERESULTS_CID);
NS_DEFINE_NAMED_CID(NS_AUTOCOMPLETEITEM_CID);

const mozilla::Module::CIDEntry kAutoCompleteCIDs[] = {
  { &kNS_AUTOCOMPLETERESULTS_CID, false, NULL, nsAutoCompleteResultsConstructor },
  { &kNS_AUTOCOMPLETEITEM_CID, false, NULL, nsAutoCompleteItemConstructor },
  { NULL }
};

const mozilla::Module::ContractIDEntry kAutoCompleteContracts[] = {
  { NS_AUTOCOMPLETERESULTS_CONTRACTID, &kNS_AUTOCOMPLETERESULTS_CID },
  { NS_AUTOCOMPLETEITEM_CONTRACTID, &kNS_AUTOCOMPLETEITEM_CID },
  { NULL }
};

static const mozilla::Module kAutoCompleteModule = {
  mozilla::Module::kVersion,
  kAutoCompleteCIDs,
  kAutoCompleteContracts
};

NSMODULE_DEFN(xpAutoComplete) = &kAutoCompleteModule;
