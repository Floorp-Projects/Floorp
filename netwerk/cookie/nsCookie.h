/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookie_h__
#define nsCookie_h__

#include "nsICookie.h"
#include "nsICookie2.h"
#include "nsString.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Preferences.h"

using mozilla::OriginAttributes;

/** 
 * The nsCookie class is the main cookie storage medium for use within cookie
 * code. It implements nsICookie2, which extends nsICookie, a frozen interface
 * for xpcom access of cookie objects.
 */

/******************************************************************************
 * nsCookie:
 * implementation
 ******************************************************************************/

class nsCookie : public nsICookie2
{
  public:
    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOOKIE
    NS_DECL_NSICOOKIE2

  private:
    // for internal use only. see nsCookie::Create().
    nsCookie(const char     *aName,
             const char     *aValue,
             const char     *aHost,
             const char     *aPath,
             const char     *aEnd,
             int64_t         aExpiry,
             int64_t         aLastAccessed,
             int64_t         aCreationTime,
             bool            aIsSession,
             bool            aIsSecure,
             bool            aIsHttpOnly,
             const OriginAttributes& aOriginAttributes)
     : mName(aName)
     , mValue(aValue)
     , mHost(aHost)
     , mPath(aPath)
     , mEnd(aEnd)
     , mExpiry(aExpiry)
     , mLastAccessed(aLastAccessed)
     , mCreationTime(aCreationTime)
     , mIsSession(aIsSession)
     , mIsSecure(aIsSecure)
     , mIsHttpOnly(aIsHttpOnly)
     , mOriginAttributes(aOriginAttributes)
    {
    }

    static int CookieStaleThreshold()
    {
      static bool initialized = false;
      static int value = 60;
      if (!initialized) {
        mozilla::Preferences::AddIntVarCache(&value, "network.cookie.staleThreshold", 60);
        initialized = true;
      }
      return value;
    }

  public:
    // Generate a unique and monotonically increasing creation time. See comment
    // in nsCookie.cpp.
    static int64_t GenerateUniqueCreationTime(int64_t aCreationTime);

    // public helper to create an nsCookie object. use |operator delete|
    // to destroy an object created by this method.
    static nsCookie * Create(const nsACString &aName,
                             const nsACString &aValue,
                             const nsACString &aHost,
                             const nsACString &aPath,
                             int64_t           aExpiry,
                             int64_t           aLastAccessed,
                             int64_t           aCreationTime,
                             bool              aIsSession,
                             bool              aIsSecure,
                             bool              aIsHttpOnly,
                             const OriginAttributes& aOriginAttributes);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    // fast (inline, non-xpcom) getters
    inline const nsDependentCString Name()  const { return nsDependentCString(mName, mValue - 1); }
    inline const nsDependentCString Value() const { return nsDependentCString(mValue, mHost - 1); }
    inline const nsDependentCString Host()  const { return nsDependentCString(mHost, mPath - 1); }
    inline const nsDependentCString RawHost() const { return nsDependentCString(IsDomain() ? mHost + 1 : mHost, mPath - 1); }
    inline const nsDependentCString Path()  const { return nsDependentCString(mPath, mEnd); }
    inline int64_t Expiry()                 const { return mExpiry; }        // in seconds
    inline int64_t LastAccessed()           const { return mLastAccessed; }  // in microseconds
    inline int64_t CreationTime()           const { return mCreationTime; }  // in microseconds
    inline bool IsSession()               const { return mIsSession; }
    inline bool IsDomain()                const { return *mHost == '.'; }
    inline bool IsSecure()                const { return mIsSecure; }
    inline bool IsHttpOnly()              const { return mIsHttpOnly; }

    // setters
    inline void SetExpiry(int64_t aExpiry)        { mExpiry = aExpiry; }
    inline void SetLastAccessed(int64_t aTime)    { mLastAccessed = aTime; }
    inline void SetIsSession(bool aIsSession)     { mIsSession = aIsSession; }
    // Set the creation time manually, overriding the monotonicity checks in
    // Create(). Use with caution!
    inline void SetCreationTime(int64_t aTime)    { mCreationTime = aTime; }

    bool IsStale() const;

  protected:
    virtual ~nsCookie() {}

  private:
    // member variables
    // we use char* ptrs to store the strings in a contiguous block,
    // so we save on the overhead of using nsCStrings. However, we
    // store a terminating null for each string, so we can hand them
    // out as nsAFlatCStrings.
    //
    // Please update SizeOfIncludingThis if this strategy changes.
    const char  *mName;
    const char  *mValue;
    const char  *mHost;
    const char  *mPath;
    const char  *mEnd;
    int64_t      mExpiry;
    int64_t      mLastAccessed;
    int64_t      mCreationTime;
    bool mIsSession;
    bool mIsSecure;
    bool mIsHttpOnly;
    mozilla::OriginAttributes mOriginAttributes;
};

#endif // nsCookie_h__
