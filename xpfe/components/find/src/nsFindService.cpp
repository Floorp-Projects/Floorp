/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Simon Fraser <sfraser@netscape.com>
 */
 
/*
 * The sole purpose of the Find service is to store globally the
 * last used Find settings
 *
 */


#include "nsFindService.h"


nsFindService::nsFindService()
: mFindBackwards(PR_FALSE)
, mWrapFind(PR_FALSE)
, mEntireWord(PR_FALSE)
, mMatchCase(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}


nsFindService::~nsFindService()
{
}

NS_IMPL_ISUPPORTS1(nsFindService, nsIFindService);

/* attribute AString searchString; */
NS_IMETHODIMP nsFindService::GetSearchString(nsAWritableString & aSearchString)
{
    aSearchString = mSearchString;
    return NS_OK;
}

NS_IMETHODIMP nsFindService::SetSearchString(const nsAReadableString & aSearchString)
{
    mSearchString = aSearchString;
    return NS_OK;
}

/* attribute AString replaceString; */
NS_IMETHODIMP nsFindService::GetReplaceString(nsAWritableString & aReplaceString)
{
    aReplaceString = mReplaceString;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetReplaceString(const nsAReadableString & aReplaceString)
{
    mReplaceString = aReplaceString;
    return NS_OK;
}

/* attribute boolean findBackwards; */
NS_IMETHODIMP nsFindService::GetFindBackwards(PRBool *aFindBackwards)
{
    NS_ENSURE_ARG_POINTER(aFindBackwards);
    *aFindBackwards = mFindBackwards;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetFindBackwards(PRBool aFindBackwards)
{
    mFindBackwards = aFindBackwards;
    return NS_OK;
}

/* attribute boolean wrapFind; */
NS_IMETHODIMP nsFindService::GetWrapFind(PRBool *aWrapFind)
{
    NS_ENSURE_ARG_POINTER(aWrapFind);
    *aWrapFind = mWrapFind;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetWrapFind(PRBool aWrapFind)
{
    mWrapFind = aWrapFind;
    return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP nsFindService::GetEntireWord(PRBool *aEntireWord)
{
    NS_ENSURE_ARG_POINTER(aEntireWord);
    *aEntireWord = mEntireWord;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetEntireWord(PRBool aEntireWord)
{
    mEntireWord = aEntireWord;
    return NS_OK;
}

/* attribute boolean matchCase; */
NS_IMETHODIMP nsFindService::GetMatchCase(PRBool *aMatchCase)
{
    NS_ENSURE_ARG_POINTER(aMatchCase);
    *aMatchCase = mMatchCase;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetMatchCase(PRBool aMatchCase)
{
    mMatchCase = aMatchCase;
    return NS_OK;
}


nsFindService*   nsFindService::gFindService;

nsFindService*
nsFindService::GetSingleton()
{
  if (!gFindService) {
    gFindService = new nsFindService();
    if (gFindService)
      NS_ADDREF(gFindService);
  }
  NS_IF_ADDREF(gFindService);
  return gFindService;
}

void
nsFindService::FreeSingleton()
{
    NS_IF_RELEASE(gFindService);
}

