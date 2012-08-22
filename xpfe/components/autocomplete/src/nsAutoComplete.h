/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoComplete_h___
#define nsAutoComplete_h___

#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsIAutoCompleteResults.h"


/******************************************************************************
 * nsAutoCompleteItem
 ******************************************************************************/
class nsAutoCompleteItem : public nsIAutoCompleteItem
{
public:
	nsAutoCompleteItem();
	virtual ~nsAutoCompleteItem();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETEITEM

private:
    nsString mValue;
    nsString mComment;
    nsCString mClassName;
    
    nsCOMPtr<nsISupports> mParam;
};

/******************************************************************************
 * nsAutoCompleteResults
 ******************************************************************************/
class nsAutoCompleteResults : public nsIAutoCompleteResults
{
public:
	nsAutoCompleteResults();
	virtual ~nsAutoCompleteResults();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULTS
  
private:
    nsString mSearchString;    
    nsCOMPtr<nsISupportsArray> mItems;
    int32_t mDefaultItemIndex;

    nsCOMPtr<nsISupports> mParam;
};

#endif
