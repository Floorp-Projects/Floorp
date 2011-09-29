/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#ifndef __NSCLIENTAUTHREMEMBER_H__
#define __NSCLIENTAUTHREMEMBER_H__

#include "mozilla/ReentrantMonitor.h"
#include "nsTHashtable.h"
#include "nsIObserver.h"
#include "nsIX509Cert.h"
#include "nsAutoPtr.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsClientAuthRemember
{
public:

  nsClientAuthRemember()
  {
  }
  
  nsClientAuthRemember(const nsClientAuthRemember &other)
  {
    this->operator=(other);
  }

  nsClientAuthRemember &operator=(const nsClientAuthRemember &other)
  {
    mAsciiHost = other.mAsciiHost;
    mFingerprint = other.mFingerprint;
    mDBKey = other.mDBKey;
    return *this;
  }

  nsCString mAsciiHost;
  nsCString mFingerprint;
  nsCString mDBKey;
};


// hash entry class
class nsClientAuthRememberEntry : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const char* KeyType;
    typedef const char* KeyTypePointer;

    // do nothing with aHost - we require mHead to be set before we're live!
    nsClientAuthRememberEntry(KeyTypePointer aHostWithCertUTF8)
    {
    }

    nsClientAuthRememberEntry(const nsClientAuthRememberEntry& toCopy)
    {
      mSettings = toCopy.mSettings;
    }

    ~nsClientAuthRememberEntry()
    {
    }

    KeyType GetKey() const
    {
      return HostWithCertPtr();
    }

    KeyTypePointer GetKeyPointer() const
    {
      return HostWithCertPtr();
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
      return !strcmp(HostWithCertPtr(), aKey);
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

    enum { ALLOW_MEMMOVE = PR_FALSE };

    // get methods
    inline const nsCString &HostWithCert() const { return mHostWithCert; }

    inline KeyTypePointer HostWithCertPtr() const
    {
      return mHostWithCert.get();
    }

    nsClientAuthRemember mSettings;
    nsCString mHostWithCert;
};

class nsClientAuthRememberService : public nsIObserver,
                                    public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsClientAuthRememberService();
  ~nsClientAuthRememberService();

  nsresult Init();

  static void GetHostWithCert(const nsACString & aHostName, 
                              const nsACString & nickname, nsACString& _retval);

  nsresult RememberDecision(const nsACString & aHostName, 
                            CERTCertificate *aServerCert, CERTCertificate *aClientCert);
  nsresult HasRememberedDecision(const nsACString & aHostName, 
                                 CERTCertificate *aServerCert, 
                                 nsACString & aCertDBKey, bool *_retval);

  void ClearRememberedDecisions();

protected:
    mozilla::ReentrantMonitor monitor;
    nsTHashtable<nsClientAuthRememberEntry> mSettingsTable;

    void RemoveAllFromMemory();
    nsresult AddEntryToList(const nsACString &host, 
                            const nsACString &server_fingerprint,
                            const nsACString &db_key);
};

#endif
