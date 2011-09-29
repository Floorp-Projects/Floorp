/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
/*
 * The sole purpose of the Find service is to store globally the
 * last used Find settings
 *
 */


#include "nsFindService.h"


nsFindService::nsFindService()
: mFindBackwards(PR_FALSE)
, mWrapFind(PR_TRUE)
, mEntireWord(PR_FALSE)
, mMatchCase(PR_FALSE)
{
}


nsFindService::~nsFindService()
{
}

NS_IMPL_ISUPPORTS1(nsFindService, nsIFindService)

/* attribute AString searchString; */
NS_IMETHODIMP nsFindService::GetSearchString(nsAString & aSearchString)
{
    aSearchString = mSearchString;
    return NS_OK;
}

NS_IMETHODIMP nsFindService::SetSearchString(const nsAString & aSearchString)
{
    mSearchString = aSearchString;
    return NS_OK;
}

/* attribute AString replaceString; */
NS_IMETHODIMP nsFindService::GetReplaceString(nsAString & aReplaceString)
{
    aReplaceString = mReplaceString;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetReplaceString(const nsAString & aReplaceString)
{
    mReplaceString = aReplaceString;
    return NS_OK;
}

/* attribute boolean findBackwards; */
NS_IMETHODIMP nsFindService::GetFindBackwards(bool *aFindBackwards)
{
    NS_ENSURE_ARG_POINTER(aFindBackwards);
    *aFindBackwards = mFindBackwards;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetFindBackwards(bool aFindBackwards)
{
    mFindBackwards = aFindBackwards;
    return NS_OK;
}

/* attribute boolean wrapFind; */
NS_IMETHODIMP nsFindService::GetWrapFind(bool *aWrapFind)
{
    NS_ENSURE_ARG_POINTER(aWrapFind);
    *aWrapFind = mWrapFind;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetWrapFind(bool aWrapFind)
{
    mWrapFind = aWrapFind;
    return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP nsFindService::GetEntireWord(bool *aEntireWord)
{
    NS_ENSURE_ARG_POINTER(aEntireWord);
    *aEntireWord = mEntireWord;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetEntireWord(bool aEntireWord)
{
    mEntireWord = aEntireWord;
    return NS_OK;
}

/* attribute boolean matchCase; */
NS_IMETHODIMP nsFindService::GetMatchCase(bool *aMatchCase)
{
    NS_ENSURE_ARG_POINTER(aMatchCase);
    *aMatchCase = mMatchCase;
    return NS_OK;
}
NS_IMETHODIMP nsFindService::SetMatchCase(bool aMatchCase)
{
    mMatchCase = aMatchCase;
    return NS_OK;
}

