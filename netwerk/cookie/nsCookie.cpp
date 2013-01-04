/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookie.h"
#include "nsUTF8ConverterService.h"
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
static int64_t gLastCreationTime;

int64_t
nsCookie::GenerateUniqueCreationTime(int64_t aCreationTime)
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
                 int64_t           aExpiry,
                 int64_t           aLastAccessed,
                 int64_t           aCreationTime,
                 bool              aIsSession,
                 bool              aIsSecure,
                 bool              aIsHttpOnly)
{
  // Ensure mValue contains a valid UTF-8 sequence. Otherwise XPConnect will
  // truncate the string after the first invalid octet.
  nsUTF8ConverterService converter;
  nsAutoCString aUTF8Value;
  converter.ConvertStringToUTF8(aValue, "UTF-8", false, true, 1, aUTF8Value);

  // find the required string buffer size, adding 4 for the terminating nulls
  const uint32_t stringLength = aName.Length() + aUTF8Value.Length() +
                                aHost.Length() + aPath.Length() + 4;

  // allocate contiguous space for the nsCookie and its strings -
  // we store the strings in-line with the nsCookie to save allocations
  void *place = ::operator new(sizeof(nsCookie) + stringLength);
  if (!place)
    return nullptr;

  // assign string members
  char *name, *value, *host, *path, *end;
  name = static_cast<char *>(place) + sizeof(nsCookie);
  StrBlockCopy(aName, aUTF8Value, aHost, aPath,
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
NS_IMETHODIMP nsCookie::GetExpiry(int64_t *aExpiry)        { *aExpiry = Expiry();       return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsSession(bool *aIsSession)   { *aIsSession = IsSession(); return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsDomain(bool *aIsDomain)     { *aIsDomain = IsDomain();   return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsSecure(bool *aIsSecure)     { *aIsSecure = IsSecure();   return NS_OK; }
NS_IMETHODIMP nsCookie::GetIsHttpOnly(bool *aHttpOnly)   { *aHttpOnly = IsHttpOnly(); return NS_OK; }
NS_IMETHODIMP nsCookie::GetStatus(nsCookieStatus *aStatus) { *aStatus = 0;              return NS_OK; }
NS_IMETHODIMP nsCookie::GetPolicy(nsCookiePolicy *aPolicy) { *aPolicy = 0;              return NS_OK; }
NS_IMETHODIMP nsCookie::GetCreationTime(int64_t *aCreation){ *aCreation = CreationTime(); return NS_OK; }
NS_IMETHODIMP nsCookie::GetLastAccessed(int64_t *aTime)    { *aTime = LastAccessed();   return NS_OK; }

// compatibility method, for use with the legacy nsICookie interface.
// here, expires == 0 denotes a session cookie.
NS_IMETHODIMP
nsCookie::GetExpires(uint64_t *aExpires)
{
  if (IsSession()) {
    *aExpires = 0;
  } else {
    *aExpires = Expiry() > 0 ? Expiry() : 1;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsCookie, nsICookie2, nsICookie)
