/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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
