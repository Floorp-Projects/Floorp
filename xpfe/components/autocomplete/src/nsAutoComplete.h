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

#ifndef nsAutoComplete_h___
#define nsAutoComplete_h___

#include "nslayout.h"
#include "nsString.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIAutoCompleteListener.h"
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
    nsString mClassName;
    
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
    PRInt32 mDefaultItemIndex;

    nsCOMPtr<nsISupports> mParam;
};

#endif
