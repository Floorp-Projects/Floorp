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
     , mIsSession(aIsSession != PR_FALSE)
     , mIsSecure(aIsSecure != PR_FALSE)
     , mIsHttpOnly(aIsHttpOnly != PR_FALSE)
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
