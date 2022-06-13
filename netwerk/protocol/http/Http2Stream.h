/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2Stream_h
#define mozilla_net_Http2Stream_h

#include "Http2StreamBase.h"

namespace mozilla::net {

class Http2Stream : public Http2StreamBase {
 public:
  Http2Stream(nsAHttpTransaction* httpTransaction, Http2Session* session,
              int32_t priority, uint64_t bcId)
      : Http2StreamBase(httpTransaction, session, priority, bcId) {}

  void Close(nsresult reason) override;
  Http2Stream* GetHttp2Stream() override { return this; }
  uint32_t GetWireStreamId() override;

  nsresult OnWriteSegment(char* buf, uint32_t count,
                          uint32_t* countWritten) override;

  nsresult CheckPushCache();
  Http2PushedStream* PushSource() { return mPushSource; }
  bool IsReadingFromPushStream();
  void ClearPushSource();

 protected:
  ~Http2Stream();

 private:
  // For Http2Push
  void AdjustPushedPriority();
  Http2PushedStream* mPushSource{nullptr};
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http2Stream_h
