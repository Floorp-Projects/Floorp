/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "base/basictypes.h"
#include "MediaStreamList.h"
#ifdef MOZILLA_INTERNAL_API
#include "mozilla/dom/MediaStreamListBinding.h"
#endif
#include "nsIScriptGlobalObject.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"

namespace mozilla {
namespace dom {

MediaStreamList::MediaStreamList(PeerConnectionImpl* peerConnection,
                                 StreamType type)
  : mPeerConnection(peerConnection),
    mType(type)
{
}

MediaStreamList::~MediaStreamList()
{
}

#ifdef MOZILLA_INTERNAL_API
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(MediaStreamList)
#else
NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaStreamList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(MediaStreamList)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
#endif

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
MediaStreamList::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
#ifdef MOZILLA_INTERNAL_API
  return MediaStreamListBinding::Wrap(cx, this, aGivenProto);
#else
  return nullptr;
#endif
}

nsISupports*
MediaStreamList::GetParentObject()
{
  return mPeerConnection->GetWindow();
}

template<class T>
static DOMMediaStream*
GetStreamFromInfo(T* info, bool& found)
{
  if (!info) {
    found = false;
    return nullptr;
  }

  found = true;
  return info->GetMediaStream();
}

DOMMediaStream*
MediaStreamList::IndexedGetter(uint32_t index, bool& found)
{
  if (!mPeerConnection->media()) { // PeerConnection closed
    found = false;
    return nullptr;
  }
  if (mType == Local) {
    return GetStreamFromInfo(mPeerConnection->media()->
      GetLocalStreamByIndex(index), found);
  }

  return GetStreamFromInfo(mPeerConnection->media()->
    GetRemoteStreamByIndex(index), found);
}

uint32_t
MediaStreamList::Length()
{
  if (!mPeerConnection->media()) { // PeerConnection closed
    return 0;
  }
  return mType == Local ? mPeerConnection->media()->LocalStreamsLength() :
      mPeerConnection->media()->RemoteStreamsLength();
}

} // namespace dom
} // namespace mozilla
