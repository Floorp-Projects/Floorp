/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_RPS_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_RPS_TEST_H_

#include "vp8.h"
#include "normal_async_test.h"

class RpsDecodeCompleteCallback;

class VP8RpsTest : public VP8NormalAsyncTest {
 public:
  VP8RpsTest(float bitRate);
  VP8RpsTest();
  virtual ~VP8RpsTest();
  virtual void Perform();
 private:
  VP8RpsTest(std::string name, std::string description, unsigned int testNo)
  : VP8NormalAsyncTest(name, description, testNo) {}
  virtual bool EncodeRps(RpsDecodeCompleteCallback* decodeCallback);
  virtual int Decode(int lossValue = 0);

  static bool CheckIfBitExact(const void *ptrA, unsigned int aLengthBytes,
      const void *ptrB, unsigned int bLengthBytes);

  webrtc::VP8Decoder* decoder2_;
  TestVideoBuffer decoded_frame2_;
  bool sli_;
};

class RpsDecodeCompleteCallback : public webrtc::DecodedImageCallback {
 public:
  RpsDecodeCompleteCallback(TestVideoBuffer* buffer);
  WebRtc_Word32 Decoded(webrtc::VideoFrame& decodedImage);
  bool DecodeComplete();
  WebRtc_Word32 ReceivedDecodedReferenceFrame(const WebRtc_UWord64 picture_id);
  WebRtc_Word32 ReceivedDecodedFrame(const WebRtc_UWord64 picture_id);
  WebRtc_UWord64 LastDecodedPictureId() const;
  WebRtc_UWord64 LastDecodedRefPictureId(bool *updated);

 private:
  TestVideoBuffer* decoded_frame_;
  bool decode_complete_;
  WebRtc_UWord64 last_decoded_picture_id_;
  WebRtc_UWord64 last_decoded_ref_picture_id_;
  bool updated_ref_picture_id_;
};

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_RPS_TEST_H_
