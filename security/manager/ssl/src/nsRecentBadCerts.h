/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RECENTBADCERTS_H__
#define __RECENTBADCERTS_H__

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

#include "nsIRecentBadCertsService.h"
#include "nsTHashtable.h"
#include "nsString.h"
#include "cert.h"
#include "secitem.h"

class RecentBadCert
{
public:

  RecentBadCert()
  {
    mDERCert.len = 0;
    mDERCert.data = nullptr;
    isDomainMismatch = false;
    isNotValidAtThisTime = false;
    isUntrusted = false;
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
    mDERCert.data = nullptr;
  }

  nsString mHostWithPort;
  SECItem mDERCert;
  bool isDomainMismatch;
  bool isNotValidAtThisTime;
  bool isUntrusted;

private:
  RecentBadCert(const RecentBadCert &other) MOZ_DELETE;
  RecentBadCert &operator=(const RecentBadCert &other) MOZ_DELETE;
};

class nsRecentBadCerts MOZ_FINAL : public nsIRecentBadCerts
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRECENTBADCERTS

  nsRecentBadCerts();
  ~nsRecentBadCerts();

protected:
    mozilla::ReentrantMonitor monitor;

    enum {const_recently_seen_list_size = 5};
    RecentBadCert mCerts[const_recently_seen_list_size];

    // will be in the range of 0 to list_size-1
    uint32_t mNextStorePosition;
};

#define NS_RECENTBADCERTS_CID { /* e7caf8c0-3570-47fe-aa1b-da47539b5d07 */ \
    0xe7caf8c0,                                                        \
    0x3570,                                                            \
    0x47fe,                                                            \
    {0xaa, 0x1b, 0xda, 0x47, 0x53, 0x9b, 0x5d, 0x07}                   \
  }

#endif
