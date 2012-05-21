/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProxyInfo_h__
#define nsProxyInfo_h__

#include "nsIProxyInfo.h"
#include "nsString.h"

// Use to support QI nsIProxyInfo to nsProxyInfo
#define NS_PROXYINFO_IID \
{ /* ed42f751-825e-4cc2-abeb-3670711a8b85 */         \
    0xed42f751,                                      \
    0x825e,                                          \
    0x4cc2,                                          \
    {0xab, 0xeb, 0x36, 0x70, 0x71, 0x1a, 0x8b, 0x85} \
}

// This class is exposed to other classes inside Necko for fast access
// to the nsIProxyInfo attributes.
class nsProxyInfo : public nsIProxyInfo
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PROXYINFO_IID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROXYINFO

  // Cheap accessors for use within Necko
  const nsCString &Host()  { return mHost; }
  PRInt32          Port()  { return mPort; }
  const char      *Type()  { return mType; }
  PRUint32         Flags() { return mFlags; }

private:
  friend class nsProtocolProxyService;

  nsProxyInfo(const char *type = nsnull)
    : mType(type)
    , mPort(-1)
    , mFlags(0)
    , mResolveFlags(0)
    , mTimeout(PR_UINT32_MAX)
    , mNext(nsnull)
  {}

  ~nsProxyInfo()
  {
    NS_IF_RELEASE(mNext);
  }

  const char  *mType;  // pointer to statically allocated value
  nsCString    mHost;
  PRInt32      mPort;
  PRUint32     mFlags;
  PRUint32     mResolveFlags;
  PRUint32     mTimeout;
  nsProxyInfo *mNext;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsProxyInfo, NS_PROXYINFO_IID)

#endif // nsProxyInfo_h__
