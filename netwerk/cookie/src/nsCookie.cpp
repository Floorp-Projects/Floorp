/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Daniel Witte.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCookie.h"

/******************************************************************************
 * nsCookie:
 * string helper impl
 ******************************************************************************/

// allocate contiguous storage and copy aSource strings,
// providing terminating nulls for each destination string.
// XXX consider arena allocation here
static inline void
StrBlockCopy(const nsACString &aSource1,
             const nsACString &aSource2,
             const nsACString &aSource3,
             const nsACString &aSource4,
             char             *&aDest1,
             char             *&aDest2,
             char             *&aDest3,
             char             *&aDest4,
             char             *&aDestEnd)
{
  // find the required buffer size, adding 4 for the terminating nulls
  const PRUint32 totalLength = aSource1.Length() + aSource2.Length() + aSource3.Length() + aSource4.Length() + 4;

  char *toBegin = NS_STATIC_CAST(char*, nsMemory::Alloc(totalLength * sizeof(char)));
  NS_ASSERTION(toBegin, "out of memory allocating for nsCookie!");

  aDest1 = toBegin;
  if (toBegin) {
    nsACString::const_iterator fromBegin, fromEnd;

    *copy_string(aSource1.BeginReading(fromBegin), aSource1.EndReading(fromEnd), toBegin) = char(0);
    aDest2 = ++toBegin;
    *copy_string(aSource2.BeginReading(fromBegin), aSource2.EndReading(fromEnd), toBegin) = char(0);
    aDest3 = ++toBegin;
    *copy_string(aSource3.BeginReading(fromBegin), aSource3.EndReading(fromEnd), toBegin) = char(0);
    aDest4 = ++toBegin;
    *copy_string(aSource4.BeginReading(fromBegin), aSource4.EndReading(fromEnd), toBegin) = char(0);
    aDestEnd = toBegin;
  }
}

/******************************************************************************
 * nsCookie:
 * ctor/dtor
 ******************************************************************************/

nsCookie::nsCookie(const nsACString &aName,
                   const nsACString &aValue,
                   const nsACString &aHost,
                   const nsACString &aPath,
                   nsInt64          aExpiry,
                   nsInt64          aLastAccessed,
                   PRBool           aIsSession,
                   PRBool           aIsSecure,
                   nsCookieStatus   aStatus,
                   nsCookiePolicy   aPolicy)
 : mNext(nsnull)
 , mExpiry(aExpiry)
 , mLastAccessed(aLastAccessed)
 , mRefCnt(0)
 , mIsSession(aIsSession != PR_FALSE)
 , mIsSecure(aIsSecure != PR_FALSE)
 , mStatus(aStatus)
 , mPolicy(aPolicy)
{
  // allocate a new (contiguous) string, and assign string members
  StrBlockCopy(aName, aValue, aHost, aPath,
               mName, mValue, mHost, mPath, mEnd);
}

nsCookie::~nsCookie()
{
  if (mName)
    nsMemory::Free(mName);
}

/******************************************************************************
 * nsCookie:
 * xpcom impl
 ******************************************************************************/

// xpcom getters
NS_IMETHODIMP nsCookie::GetName(nsACString &aName)         { aName = Name();            return NS_OK; }
NS_IMETHODIMP nsCookie::GetValue(nsACString &aValue)       { aValue = Value();          return NS_OK; }
NS_IMETHODIMP nsCookie::GetHost(nsACString &aHost)         { aHost = Host();            return NS_OK; }
NS_IMETHODIMP nsCookie::GetRawHost(nsACString &aHost)      { aHost = RawHost();         return NS_OK; }
NS_IMETHODIMP nsCookie::GetPath(nsACString &aPath)         { aPath = Path();            return NS_OK; }
NS_IMETHODIMP nsCookie::GetExpiry(PRInt64 *aExpiry)        { *aExpiry = Expiry();       return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsSession(PRBool *aIsSession)   { *aIsSession = IsSession(); return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsDomain(PRBool *aIsDomain)     { *aIsDomain = IsDomain();   return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsSecure(PRBool *aIsSecure)     { *aIsSecure = IsSecure();   return NS_OK; }
NS_IMETHODIMP nsCookie::GetStatus(nsCookieStatus *aStatus) { *aStatus = Status();       return NS_OK; }
NS_IMETHODIMP nsCookie::GetPolicy(nsCookiePolicy *aPolicy) { *aPolicy = Policy();       return NS_OK; }

// compatibility method, for use with the legacy nsICookie interface.
// here, expires == 0 denotes a session cookie.
NS_IMETHODIMP
nsCookie::GetExpires(PRUint64 *aExpires)
{
  if (IsSession()) {
    *aExpires = 0;
  } else {
    *aExpires = Expiry() > nsInt64(0) ? PRInt64(Expiry()) : 1;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsCookie, nsICookie, nsICookie2)
