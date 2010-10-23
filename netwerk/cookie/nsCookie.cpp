/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCookie.h"
#include <stdlib.h>

/******************************************************************************
 * nsCookie:
 * string helper impl
 ******************************************************************************/

// copy aSource strings into contiguous storage provided in aDest1,
// providing terminating nulls for each destination string.
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
  char *toBegin = aDest1;
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

/******************************************************************************
 * nsCookie:
 * creation helper
 ******************************************************************************/

// This is a counter that keeps track of the last used creation time, each time
// we create a new nsCookie. This is nominally the time (in microseconds) the
// cookie was created, but is guaranteed to be monotonically increasing for
// cookies added at runtime after the database has been read in. This is
// necessary to enforce ordering among cookies whose creation times would
// otherwise overlap, since it's possible two cookies may be created at the same
// time, or that the system clock isn't monotonic.
static PRInt64 gLastCreationTime;

PRInt64
nsCookie::GenerateUniqueCreationTime(PRInt64 aCreationTime)
{
  // Check if the creation time given to us is greater than the running maximum
  // (it should always be monotonically increasing).
  if (aCreationTime > gLastCreationTime) {
    gLastCreationTime = aCreationTime;
    return aCreationTime;
  }

  // Make up our own.
  return ++gLastCreationTime;
}

nsCookie *
nsCookie::Create(const nsACString &aName,
                 const nsACString &aValue,
                 const nsACString &aHost,
                 const nsACString &aPath,
                 PRInt64           aExpiry,
                 PRInt64           aLastAccessed,
                 PRInt64           aCreationTime,
                 PRBool            aIsSession,
                 PRBool            aIsSecure,
                 PRBool            aIsHttpOnly)
{
  // find the required string buffer size, adding 4 for the terminating nulls
  const PRUint32 stringLength = aName.Length() + aValue.Length() +
                                aHost.Length() + aPath.Length() + 4;

  // allocate contiguous space for the nsCookie and its strings -
  // we store the strings in-line with the nsCookie to save allocations
  void *place = ::operator new(sizeof(nsCookie) + stringLength);
  if (!place)
    return nsnull;

  // assign string members
  char *name, *value, *host, *path, *end;
  name = static_cast<char *>(place) + sizeof(nsCookie);
  StrBlockCopy(aName, aValue, aHost, aPath,
               name, value, host, path, end);

  // If the creationTime given to us is higher than the running maximum, update
  // our maximum.
  if (aCreationTime > gLastCreationTime)
    gLastCreationTime = aCreationTime;

  // construct the cookie. placement new, oh yeah!
  return new (place) nsCookie(name, value, host, path, end,
                              aExpiry, aLastAccessed, aCreationTime,
                              aIsSession, aIsSecure, aIsHttpOnly);
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
NS_IMETHODIMP nsCookie::GetIsHttpOnly(PRBool *aHttpOnly)   { *aHttpOnly = IsHttpOnly(); return NS_OK; }
NS_IMETHODIMP nsCookie::GetStatus(nsCookieStatus *aStatus) { *aStatus = 0;              return NS_OK; }
NS_IMETHODIMP nsCookie::GetPolicy(nsCookiePolicy *aPolicy) { *aPolicy = 0;              return NS_OK; }
NS_IMETHODIMP nsCookie::GetCreationTime(PRInt64 *aCreation){ *aCreation = CreationTime(); return NS_OK; }
NS_IMETHODIMP nsCookie::GetLastAccessed(PRInt64 *aTime)    { *aTime = LastAccessed();   return NS_OK; }

// compatibility method, for use with the legacy nsICookie interface.
// here, expires == 0 denotes a session cookie.
NS_IMETHODIMP
nsCookie::GetExpires(PRUint64 *aExpires)
{
  if (IsSession()) {
    *aExpires = 0;
  } else {
    *aExpires = Expiry() > 0 ? Expiry() : 1;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsCookie, nsICookie2, nsICookie)
