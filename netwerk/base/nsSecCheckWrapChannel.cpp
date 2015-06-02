/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecCheckWrapChannel.h"
#include "nsHttpChannel.h"
#include "nsCOMPtr.h"

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetChannelWrapperLog()
{
  static PRLogModuleInfo* gChannelWrapperPRLog;
  if (!gChannelWrapperPRLog) {
    gChannelWrapperPRLog = PR_NewLogModule("ChannelWrapper");
  }
  return gChannelWrapperPRLog;
}
#endif

#define CHANNELWRAPPERLOG(args) PR_LOG(GetChannelWrapperLog(), 4, args)

NS_IMPL_ADDREF(nsSecCheckWrapChannelBase)
NS_IMPL_RELEASE(nsSecCheckWrapChannelBase)

NS_INTERFACE_MAP_BEGIN(nsSecCheckWrapChannelBase)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannel, mHttpChannel)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannelInternal, mHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIHttpChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsISecCheckWrapChannel)
NS_INTERFACE_MAP_END

//---------------------------------------------------------
// nsSecCheckWrapChannelBase implementation
//---------------------------------------------------------

nsSecCheckWrapChannelBase::nsSecCheckWrapChannelBase(nsIChannel* aChannel)
 : mChannel(aChannel)
 , mHttpChannel(do_QueryInterface(aChannel))
 , mHttpChannelInternal(do_QueryInterface(aChannel))
 , mRequest(do_QueryInterface(aChannel))
{
  MOZ_ASSERT(mChannel, "can not create a channel wrapper without a channel");
}

nsSecCheckWrapChannelBase::~nsSecCheckWrapChannelBase()
{
}

//---------------------------------------------------------
// nsISecCheckWrapChannel implementation
//---------------------------------------------------------

NS_IMETHODIMP
nsSecCheckWrapChannelBase::GetInnerChannel(nsIChannel **aInnerChannel)
{
  NS_IF_ADDREF(*aInnerChannel = mChannel);
  return NS_OK;
}

//---------------------------------------------------------
// nsSecCheckWrapChannel implementation
//---------------------------------------------------------

nsSecCheckWrapChannel::nsSecCheckWrapChannel(nsIChannel* aChannel,
                                             nsILoadInfo* aLoadInfo)
 : nsSecCheckWrapChannelBase(aChannel)
 , mLoadInfo(aLoadInfo)
{
#ifdef PR_LOGGING
  {
    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    if (uri) {
      uri->GetSpec(spec);
    }
    CHANNELWRAPPERLOG(("nsSecCheckWrapChannel::nsSecCheckWrapChannel [%p] (%s)",this, spec.get()));
  }
#endif
}

nsSecCheckWrapChannel::~nsSecCheckWrapChannel()
{
}

//---------------------------------------------------------
// nsIChannel implementation
//---------------------------------------------------------

NS_IMETHODIMP
nsSecCheckWrapChannel::GetLoadInfo(nsILoadInfo** aLoadInfo)
{
  CHANNELWRAPPERLOG(("nsSecCheckWrapChannel::GetLoadInfo() [%p]",this));
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsSecCheckWrapChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  CHANNELWRAPPERLOG(("nsSecCheckWrapChannel::SetLoadInfo() [%p]", this));
  mLoadInfo = aLoadInfo;
  return NS_OK;
}
