/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamList_h__
#define MediaStreamList_h__

#include "mozilla/ErrorResult.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "nsWrapperCache.h"

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#else
#include "DOMMediaStream.h"
#endif

namespace mozilla {
class PeerConnectionImpl;
namespace dom {

class MediaStreamList : public nsISupports,
                        public nsWrapperCache
{
public:
  enum StreamType {
    Local,
    Remote
  };

  MediaStreamList(PeerConnectionImpl* peerConnection, StreamType type);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaStreamList)

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
    override;
  nsISupports* GetParentObject();

  DOMMediaStream* IndexedGetter(uint32_t index, bool& found);
  uint32_t Length();

private:
  virtual ~MediaStreamList();

  RefPtr<PeerConnectionImpl> mPeerConnection;
  StreamType mType;
};

} // namespace dom
} // namespace mozilla

#endif // MediaStreamList_h__
