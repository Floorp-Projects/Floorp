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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
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

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookieManager2.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "nsCookie.h"
#include "nsString.h"
#include "nsTHashtable.h"

#include "nsInt64.h"

struct nsCookieAttributes;
struct nsListIter;
struct nsEnumerationData;
class nsIPrefBranch;
class nsICookieConsent;
class nsICookiePermission;
class nsIPrefBranch;
class nsIObserverService;
class nsIURI;
class nsIChannel;
class nsITimer;
class nsIFile;

// hash entry class
class nsCookieEntry : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    // do nothing with aHost - we require mHead to be set before we're live!
    nsCookieEntry(KeyTypePointer aHost)
     : mHead(nsnull)
    {
    }

    nsCookieEntry(const nsCookieEntry& toCopy)
    {
      // if we end up here, things will break. nsTHashtable shouldn't
      // allow this, since we set ALLOW_MEMMOVE to true.
      NS_NOTREACHED("nsCookieEntry copy constructor is forbidden!");
    }

    ~nsCookieEntry()
    {
      // walk the linked list, and de-link everything by releasing & nulling.
      // this allows the parent host entry to be deleted by the hashtable.
      // note: we know mHead cannot be null here - we always set mHead to a
      // valid nsCookie (if it were null, the hashtable wouldn't be able to find
      // this entry, because the key string is provided by mHead).
      nsCookie *current = mHead, *next;
      do {
        next = current->Next();
        NS_RELEASE(current);
      } while ((current = next));
    }

    KeyType GetKey() const
    {
      return HostPtr();
    }

    KeyTypePointer GetKeyPointer() const
    {
      return HostPtr();
    }

    PRBool KeyEquals(KeyTypePointer aKey) const
    {
      return !strcmp(HostPtr(), aKey);
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return aKey;
    }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      // PL_DHashStringKey doesn't use the table parameter, so we can safely
      // pass nsnull
      return PL_DHashStringKey(nsnull, aKey);
    }

    enum { ALLOW_MEMMOVE = PR_TRUE };

    // get methods
    inline const nsDependentCString Host() const { return mHead->Host(); }

    // linked list management helper
    inline nsCookie*& Head() { return mHead; }

    inline KeyTypePointer HostPtr() const
    {
      return mHead->Host().get();
    }

  private:
    nsCookie *mHead;
};

/******************************************************************************
 * nsCookieService:
 * class declaration
 ******************************************************************************/

class nsCookieService : public nsICookieService
                      , public nsICookieManager2
                      , public nsIObserver
                      , public nsSupportsWeakReference
{
  public:
    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSICOOKIESERVICE
    NS_DECL_NSICOOKIEMANAGER
    NS_DECL_NSICOOKIEMANAGER2

    nsCookieService();
    virtual ~nsCookieService();
    static nsCookieService*       GetSingleton();
    nsresult                      Init();

  protected:
    void                          PrefChanged(nsIPrefBranch *aPrefBranch);
    nsresult                      Read();
    nsresult                      Write();
    PRBool                        SetCookieInternal(nsIURI *aHostURI, nsIChannel *aChannel, nsDependentCString &aCookieHeader, nsInt64 aServerTime, nsCookieStatus aStatus, nsCookiePolicy aPolicy);
    void                          AddInternal(nsCookie *aCookie, nsInt64 aCurrentTime, nsIURI *aHostURI, const char *aCookieHeader);
    void                          RemoveCookieFromList(nsListIter &aIter);
    PRBool                        AddCookieToList(nsCookie *aCookie);
    static PRBool                 GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter, nsASingleFragmentCString::const_char_iterator &aEndIter, nsDependentCSubstring &aTokenString, nsDependentCSubstring &aTokenValue, PRBool &aEqualsFound);
    static PRBool                 ParseAttributes(nsDependentCString &aCookieHeader, nsCookieAttributes &aCookie);
    static PRBool                 IsIPAddress(const nsAFlatCString &aHost);
    static PRBool                 IsInDomain(const nsACString &aDomain, const nsACString &aHost, PRBool aIsDomain = PR_TRUE);
    static PRBool                 IsForeign(nsIURI *aHostURI, nsIURI *aFirstURI);
    nsCookieStatus                CheckPrefs(nsIURI *aHostURI, nsIURI *aFirstURI, nsIChannel *aChannel, const char *aCookieHeader, nsCookiePolicy &aPolicy);
    static PRBool                 CheckDomain(nsCookieAttributes &aCookie, nsIURI *aHostURI);
    static PRBool                 CheckPath(nsCookieAttributes &aCookie, nsIURI *aHostURI);
    static PRBool                 GetExpiry(nsCookieAttributes &aCookie, nsInt64 aServerTime, nsInt64 aCurrentTime, nsCookieStatus aStatus);
    void                          RemoveAllFromMemory();
    void                          RemoveExpiredCookies(nsInt64 aCurrentTime);
    PRBool                        FindCookie(const nsAFlatCString &aHost, const nsAFlatCString &aName, const nsAFlatCString &aPath, nsListIter &aIter);
    void                          FindOldestCookie(nsEnumerationData &aData);
    PRUint32                      CountCookiesFromHost(nsCookie *aCookie, nsEnumerationData &aData);
    void                          NotifyRejected(nsIURI *aHostURI);
    void                          NotifyChanged(nsICookie2 *aCookie, const PRUnichar *aData);

    // Use LazyWrite to save the cookies file on a timer. It will write
    // the file only once if repeatedly hammered quickly.
    void                          LazyWrite();
    static void                   DoLazyWrite(nsITimer *aTimer, void *aClosure);

  protected:
    // cached members
    nsCOMPtr<nsIFile>             mCookieFile;
    nsCOMPtr<nsIObserverService>  mObserverService;
    nsCOMPtr<nsICookieConsent>    mP3PService;
    nsCOMPtr<nsICookiePermission> mPermissionService;

    // impl members
    nsCOMPtr<nsITimer>            mWriteTimer;
    nsTHashtable<nsCookieEntry>   mHostTable;
    PRUint32                      mCookieCount;
    PRPackedBool                  mCookieChanged;
    PRPackedBool                  mCookieIconVisible;

    // cached prefs
    PRUint8                       mCookiesPermissions;   // BEHAVIOR_{ACCEPT, REJECTFOREIGN, REJECT, P3P}
    PRUint16                      mMaxNumberOfCookies;
    PRUint16                      mMaxCookiesPerHost;

    // private static member, used to cache a ptr to nsCookieService,
    // so we can make nsCookieService a singleton xpcom object.
    static nsCookieService        *gCookieService;

    // this callback needs access to member functions
    friend PLDHashOperator PR_CALLBACK removeExpiredCallback(nsCookieEntry *aEntry, void *aArg);
};

#endif // nsCookieService_h__
