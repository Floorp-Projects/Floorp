/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLoadContextInfo_h__
#define nsLoadContextInfo_h__

#include "nsILoadContextInfo.h"

class nsIChannel;
class nsILoadContext;

namespace mozilla {
namespace net {

class LoadContextInfo : public nsILoadContextInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILOADCONTEXTINFO

  LoadContextInfo(bool aIsPrivate, uint32_t aAppId, bool aIsInBrowser, bool aIsAnonymous);

private:
  virtual ~LoadContextInfo();

protected:
  uint32_t mAppId;
  bool mIsPrivate : 1;
  bool mIsInBrowser : 1;
  bool mIsAnonymous : 1;
};

LoadContextInfo *
GetLoadContextInfo(nsIChannel * aChannel);

LoadContextInfo *
GetLoadContextInfo(nsILoadContext * aLoadContext,
                   bool aAnonymous);

LoadContextInfo *
GetLoadContextInfo(nsILoadContextInfo* aInfo);

LoadContextInfo *
GetLoadContextInfo(bool const aIsPrivate = false,
                   uint32_t const aAppId = nsILoadContextInfo::NO_APP_ID,
                   bool const aIsInBrowserElement = false,
                   bool const aIsAnonymous = false);

} // net
} // mozilla

#endif
