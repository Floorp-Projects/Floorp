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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef __RECENTBADCERTS_H__
#define __RECENTBADCERTS_H__

#include "mozilla/ReentrantMonitor.h"
#include "nsIRecentBadCertsService.h"
#include "nsTHashtable.h"
#include "nsString.h"
#include "secitem.h"

class RecentBadCert
{
public:

  RecentBadCert()
  {
    mDERCert.len = 0;
    mDERCert.data = nsnull;
    isDomainMismatch = PR_FALSE;
    isNotValidAtThisTime = PR_FALSE;
    isUntrusted = PR_FALSE;
  }

  ~RecentBadCert()
  {
    Clear();
  }

  void Clear()
  {
    mHostWithPort.Truncate();
    if (mDERCert.len)
      nsMemory::Free(mDERCert.data);
    mDERCert.len = 0;
    mDERCert.data = nsnull;
  }

  nsString mHostWithPort;
  SECItem mDERCert;
  bool isDomainMismatch;
  bool isNotValidAtThisTime;
  bool isUntrusted;

private:
  RecentBadCert(const RecentBadCert &other)
  {
    NS_NOTREACHED("RecentBadCert(const RecentBadCert &other) not implemented");
    this->operator=(other);
  }

  RecentBadCert &operator=(const RecentBadCert &other)
  {
    NS_NOTREACHED("RecentBadCert &operator=(const RecentBadCert &other) not implemented");
    return *this;
  }
};

class nsRecentBadCertsService : public nsIRecentBadCertsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRECENTBADCERTSSERVICE

  nsRecentBadCertsService();
  ~nsRecentBadCertsService();

  nsresult Init();

protected:
    mozilla::ReentrantMonitor monitor;

    enum {const_recently_seen_list_size = 5};
    RecentBadCert mCerts[const_recently_seen_list_size];

    // will be in the range of 0 to list_size-1
    PRUint32 mNextStorePosition;
};

#define NS_RECENTBADCERTS_CID { /* e7caf8c0-3570-47fe-aa1b-da47539b5d07 */ \
    0xe7caf8c0,                                                        \
    0x3570,                                                            \
    0x47fe,                                                            \
    {0xaa, 0x1b, 0xda, 0x47, 0x53, 0x9b, 0x5d, 0x07}                   \
  }

#endif
