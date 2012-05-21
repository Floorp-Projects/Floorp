/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookie_h__
#define nsCookie_h__

#include "nsICookie.h"
#include "nsICookie2.h"
#include "nsString.h"

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
             PRInt64         aExpiry,
             PRInt64         aLastAccessed,
             PRInt64         aCreationTime,
             bool            aIsSession,
             bool            aIsSecure,
             bool            aIsHttpOnly)
     : mName(aName)
     , mValue(aValue)
     , mHost(aHost)
     , mPath(aPath)
     , mEnd(aEnd)
     , mExpiry(aExpiry)
     , mLastAccessed(aLastAccessed)
     , mCreationTime(aCreationTime)
     , mIsSession(aIsSession != false)
     , mIsSecure(aIsSecure != false)
     , mIsHttpOnly(aIsHttpOnly != false)
    {
    }

  public:
    // Generate a unique and monotonically increasing creation time. See comment
    // in nsCookie.cpp.
    static PRInt64 GenerateUniqueCreationTime(PRInt64 aCreationTime);

    // public helper to create an nsCookie object. use |operator delete|
    // to destroy an object created by this method.
    static nsCookie * Create(const nsACString &aName,
                             const nsACString &aValue,
                             const nsACString &aHost,
                             const nsACString &aPath,
                             PRInt64           aExpiry,
                             PRInt64           aLastAccessed,
                             PRInt64           aCreationTime,
                             bool              aIsSession,
                             bool              aIsSecure,
                             bool              aIsHttpOnly);

    virtual ~nsCookie() {}

    // fast (inline, non-xpcom) getters
    inline const nsDependentCString Name()  const { return nsDependentCString(mName, mValue - 1); }
    inline const nsDependentCString Value() const { return nsDependentCString(mValue, mHost - 1); }
    inline const nsDependentCString Host()  const { return nsDependentCString(mHost, mPath - 1); }
    inline const nsDependentCString RawHost() const { return nsDependentCString(IsDomain() ? mHost + 1 : mHost, mPath - 1); }
    inline const nsDependentCString Path()  const { return nsDependentCString(mPath, mEnd); }
    inline PRInt64 Expiry()                 const { return mExpiry; }        // in seconds
    inline PRInt64 LastAccessed()           const { return mLastAccessed; }  // in microseconds
    inline PRInt64 CreationTime()           const { return mCreationTime; }  // in microseconds
    inline bool IsSession()               const { return mIsSession; }
    inline bool IsDomain()                const { return *mHost == '.'; }
    inline bool IsSecure()                const { return mIsSecure; }
    inline bool IsHttpOnly()              const { return mIsHttpOnly; }

    // setters
    inline void SetExpiry(PRInt64 aExpiry)        { mExpiry = aExpiry; }
    inline void SetLastAccessed(PRInt64 aTime)    { mLastAccessed = aTime; }
    inline void SetIsSession(bool aIsSession)   { mIsSession = (bool) aIsSession; }
    // Set the creation time manually, overriding the monotonicity checks in
    // Create(). Use with caution!
    inline void SetCreationTime(PRInt64 aTime)    { mCreationTime = aTime; }

  protected:
    // member variables
    // we use char* ptrs to store the strings in a contiguous block,
    // so we save on the overhead of using nsCStrings. However, we
    // store a terminating null for each string, so we can hand them
    // out as nsAFlatCStrings.
    const char  *mName;
    const char  *mValue;
    const char  *mHost;
    const char  *mPath;
    const char  *mEnd;
    PRInt64      mExpiry;
    PRInt64      mLastAccessed;
    PRInt64      mCreationTime;
    bool mIsSession;
    bool mIsSecure;
    bool mIsHttpOnly;
};

#endif // nsCookie_h__
