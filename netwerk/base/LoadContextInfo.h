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

class LoadContextInfo final : public nsILoadContextInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILOADCONTEXTINFO

  LoadContextInfo(bool aIsAnonymous, OriginAttributes aOriginAttributes);

private:
  virtual ~LoadContextInfo() = default;

protected:
  bool mIsAnonymous : 1;
  OriginAttributes mOriginAttributes;
};

class LoadContextInfoFactory : public nsILoadContextInfoFactory
{
  virtual ~LoadContextInfoFactory() = default;
public:
  NS_DECL_ISUPPORTS // deliberately not thread-safe
  NS_DECL_NSILOADCONTEXTINFOFACTORY
};

LoadContextInfo*
GetLoadContextInfo(nsIChannel *aChannel);

LoadContextInfo*
GetLoadContextInfo(nsILoadContext *aLoadContext,
                   bool aIsAnonymous);

LoadContextInfo*
GetLoadContextInfo(nsIDOMWindow *aLoadContext,
                   bool aIsAnonymous);

LoadContextInfo*
GetLoadContextInfo(nsILoadContextInfo *aInfo);

LoadContextInfo*
GetLoadContextInfo(bool const aIsAnonymous,
                   OriginAttributes const &aOriginAttributes);

} // namespace net
} // namespace mozilla

#endif
