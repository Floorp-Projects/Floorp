/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamList_h__
#define MediaStreamList_h__

#include "mozilla/ErrorResult.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "jspubtd.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"

class nsIDOMMediaStream;
namespace sipcc {
class PeerConnectionImpl;
} // namespace sipcc

namespace mozilla {
namespace dom {

class MediaStreamList : public NonRefcountedDOMObject
{
public:
  enum StreamType {
    Local,
    Remote
  };

  MediaStreamList(sipcc::PeerConnectionImpl* peerConnection, StreamType type);
  ~MediaStreamList();

  JSObject* WrapObject(JSContext* cx, ErrorResult& error);

  nsIDOMMediaStream* IndexedGetter(uint32_t index, bool& found);
  uint32_t Length();

private:
  nsRefPtr<sipcc::PeerConnectionImpl> mPeerConnection;
  StreamType mType;
};

} // namespace dom
} // namespace mozilla

#endif // MediaStreamList_h__
