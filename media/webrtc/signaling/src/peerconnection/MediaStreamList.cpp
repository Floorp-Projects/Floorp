/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "MediaStreamList.h"
#ifdef MOZILLA_INTERNAL_API
#include "mozilla/dom/MediaStreamListBinding.h"
#endif
#include "nsIScriptGlobalObject.h"
#include "PeerConnectionImpl.h"

namespace mozilla {
namespace dom {

MediaStreamList::MediaStreamList(sipcc::PeerConnectionImpl* peerConnection,
                                 StreamType type)
  : mPeerConnection(peerConnection),
    mType(type)
{
  MOZ_COUNT_CTOR(mozilla::dom::MediaStreamList);
}

MediaStreamList::~MediaStreamList()
{
  MOZ_COUNT_DTOR(mozilla::dom::MediaStreamList);
}

JSObject*
MediaStreamList::WrapObject(JSContext* cx, ErrorResult& error)
{
#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<nsIScriptGlobalObject> global =
    do_QueryInterface(mPeerConnection->GetWindow());
  JSObject* scope = global->GetGlobalJSObject();
  if (!scope) {
    error.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSAutoCompartment ac(cx, scope);
  JSObject* obj = MediaStreamListBinding::Wrap(cx, scope, this);
  if (!obj) {
    error.Throw(NS_ERROR_FAILURE);
  }
  return obj;
#else
  return nullptr;
#endif
}

template<class T>
static nsIDOMMediaStream*
GetStreamFromInfo(T* info, bool& found)
{
  if (!info) {
    found = false;
    return nullptr;
  }

  found = true;
  return info->GetMediaStream();
}

nsIDOMMediaStream*
MediaStreamList::IndexedGetter(uint32_t index, bool& found)
{
  if (mType == Local) {
    return GetStreamFromInfo(mPeerConnection->GetLocalStream(index), found);
  }

  return GetStreamFromInfo(mPeerConnection->GetRemoteStream(index), found);
}

uint32_t
MediaStreamList::Length()
{
  return mType == Local ? mPeerConnection->LocalStreamsLength() :
                          mPeerConnection->RemoteStreamsLength();
}

} // namespace dom
} // namespace mozilla
