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
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIChromeEntry.h"

class nsChromeEntry : public nsIChromeEntry
{
public:
  NS_DECL_ISUPPORTS

  // nsIChromeEntry methods:
  NS_DECL_NSICHROMEENTRY

  // nsChromeEntry methods:
  nsChromeEntry();
  virtual ~nsChromeEntry();

protected:
  // Member vars
};

nsChromeEntry::nsChromeEntry()
{
	NS_INIT_REFCNT();
}

nsChromeEntry::~nsChromeEntry()
{}

NS_IMPL_ISUPPORTS1(nsChromeEntry, nsIChromeEntry)

/* attribute wstring name; */
NS_IMETHODIMP nsChromeEntry::GetName(PRUnichar * *aName) { return NS_OK; }
NS_IMETHODIMP nsChromeEntry::SetName(const PRUnichar * aName) { return NS_OK; }

/* attribute wstring displayText; */
NS_IMETHODIMP nsChromeEntry::GetDisplayText(PRUnichar * *aDisplayText) { return NS_OK; }
NS_IMETHODIMP nsChromeEntry::SetDisplayText(const PRUnichar * aDisplayText) { return NS_OK; }

/* attribute wstring versionNumber; */
NS_IMETHODIMP nsChromeEntry::GetVersionNumber(PRUnichar * *aVersionNumber) { return NS_OK; }
NS_IMETHODIMP nsChromeEntry::SetVersionNumber(const PRUnichar * aVersionNumber) { return NS_OK; }

/* attribute wstring author; */
NS_IMETHODIMP nsChromeEntry::GetAuthor(PRUnichar * *aAuthor) { return NS_OK; }
NS_IMETHODIMP nsChromeEntry::SetAuthor(const PRUnichar * aAuthor) { return NS_OK; }

/* attribute wstring siteURL; */
NS_IMETHODIMP nsChromeEntry::GetSiteURL(PRUnichar * *aSiteURL) { return NS_OK; }
NS_IMETHODIMP nsChromeEntry::SetSiteURL(const PRUnichar * aSiteURL) { return NS_OK; }

/* attribute wstring previewImageURL; */
NS_IMETHODIMP nsChromeEntry::GetPreviewImageURL(PRUnichar * *aPreviewImageURL) { return NS_OK; }
NS_IMETHODIMP nsChromeEntry::SetPreviewImageURL(const PRUnichar * aPreviewImageURL) { return NS_OK; }

//////////////////////////////////////////////////////////////////////

nsresult
NS_NewChromeEntry(nsIChromeEntry** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsChromeEntry* chromeEntry = new nsChromeEntry();
    if (chromeEntry == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chromeEntry);
    *aResult = chromeEntry;
    return NS_OK;
}
