/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rps_test.h"

#include <assert.h>
#include <string.h> // memcmp
#include <time.h>

#include "vp8.h"

VP8RpsTest::VP8RpsTest(float bitRate)
    : VP8NormalAsyncTest(bitRate),
      decoder2_(webrtc::VP8Decoder::Create()),
      sli_(false) {
}

VP8RpsTest::VP8RpsTest()
    : VP8NormalAsyncTest("VP8 Reference Picture Selection Test",
                         "VP8 Reference Picture Selection Test", 1),
      decoder2_(webrtc::VP8Decoder::Create()),
      sli_(false) {
}

VP8RpsTest::~VP8RpsTest() {
  if (decoder2_) {
    decoder2_->Release();
    delete decoder2_;
  }
  decoded_frame2_.Free();
}

void VP8RpsTest::Perform() {
  _inname = "test/testFiles/foreman_cif.yuv";
  CodecSettings(352, 288, 30, _bitRate);
  Setup();
  _inputVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);
  _decodedVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);
  decoded_frame2_.VerifyAndAllocate(_lengthSourceFrame);

  // Enable RPS functionality
  _inst.codecSpecific.VP8.pictureLossIndicationOn = true;
  _inst.codecSpecific.VP8.feedbackModeOn = true;

  if(_encoder->InitEncode(&_inst, 4, 1460) < 0)
    exit(EXIT_FAILURE);

  _decoder->InitDecode(&_inst,1);
  decoder2_->InitDecode(&_inst,1);

  FrameQueue frameQueue;
  VideoEncodeCompleteCallback encCallback(_encodedFile, &frameQueue, *this);
  RpsDecodeCompleteCallback decCallback(&_decodedVideoBuffer);
  RpsDecodeCompleteCallback decCallback2(&decoded_frame2_);
  _encoder->RegisterEncodeCompleteCallback(&encCallback);
  _decoder->RegisterDecodeCompleteCallback(&decCallback);
  decoder2_->RegisterDecodeCompleteCallback(&decCallback2);

  if (SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK)
    exit(EXIT_FAILURE);

  _totalEncodeTime = _totalDecodeTime = 0;
  _totalEncodePipeTime = _totalDecodePipeTime = 0;
  bool complete = false;
  _framecnt = 0;
  _encFrameCnt = 0;
  _decFrameCnt = 0;
  _sumEncBytes = 0;
  _lengthEncFrame = 0;
  double starttime = clock()/(double)CLOCKS_PER_SEC;
  while (!complete) {
    CodecSpecific_InitBitrate();
    complete = EncodeRps(&decCallback2);
    if (!frameQueue.Empty() || complete) {
      while (!frameQueue.Empty()) {
        _frameToDecode =
            static_cast<FrameQueueTuple *>(frameQueue.PopFrame());
        int lost = DoPacketLoss();
        if (lost == 2) {
            // Lost the whole frame, continue
            _missingFrames = true;
            delete _frameToDecode;
            _frameToDecode = NULL;
            continue;
        }
        int ret = Decode(lost);
        delete _frameToDecode;
        _frameToDecode = NULL;
        if (ret < 0) {
            fprintf(stderr,"\n\nError in decoder: %d\n\n", ret);
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) {
            _framecnt++;
        }
        else {
            fprintf(stderr,
                "\n\nPositive return value from decode!\n\n");
        }
      }
    }
  }
  double endtime = clock()/(double)CLOCKS_PER_SEC;
  double totalExecutionTime = endtime - starttime;
  printf("Total execution time: %.1f s\n", totalExecutionTime);
  _sumEncBytes = encCallback.EncodedBytes();
  double actualBitRate = ActualBitRate(_encFrameCnt) / 1000.0;
  double avgEncTime = _totalEncodeTime / _encFrameCnt;
  double avgDecTime = _totalDecodeTime / _decFrameCnt;
  printf("Actual bitrate: %f kbps\n", actualBitRate);
  printf("Average encode time: %.1f ms\n", 1000 * avgEncTime);
  printf("Average decode time: %.1f ms\n", 1000 * avgDecTime);
  printf("Average encode pipeline time: %.1f ms\n",
         1000 * _totalEncodePipeTime / _encFrameCnt);
  printf("Average decode pipeline  time: %.1f ms\n",
         1000 * _totalDecodePipeTime / _decFrameCnt);
  printf("Number of encoded frames: %u\n", _encFrameCnt);
  printf("Number of decoded frames: %u\n", _decFrameCnt);
  (*_log) << "Actual bitrate: " << actualBitRate << " kbps\tTarget: " <<
      _bitRate << " kbps" << std::endl;
  (*_log) << "Average encode time: " << avgEncTime << " s" << std::endl;
  (*_log) << "Average decode time: " << avgDecTime << " s" << std::endl;
  _encoder->Release();
  _decoder->Release();
  Teardown();
}

bool VP8RpsTest::EncodeRps(RpsDecodeCompleteCallback* decodeCallback) {
  _lengthEncFrame = 0;
  size_t bytes_read = fread(_sourceBuffer, 1, _lengthSourceFrame, _sourceFile);
  if (bytes_read < _lengthSourceFrame)
    return true;
  _inputVideoBuffer.CopyBuffer(_lengthSourceFrame, _sourceBuffer);
  _inputVideoBuffer.SetTimeStamp((unsigned int)
      (_encFrameCnt * 9e4 / _inst.maxFramerate));
  _inputVideoBuffer.SetWidth(_inst.width);
  _inputVideoBuffer.SetHeight(_inst.height);
  webrtc::VideoFrame rawImage;
  VideoBufferToRawImage(_inputVideoBuffer, rawImage);
  if (feof(_sourceFile) != 0) {
      return true;
  }
  _encodeCompleteTime = 0;
  _encodeTimes[rawImage.TimeStamp()] = tGetTime();
  webrtc::VideoFrameType frameType = webrtc::kDeltaFrame;

  webrtc::CodecSpecificInfo* codecSpecificInfo = CreateEncoderSpecificInfo();
  codecSpecificInfo->codecSpecific.VP8.pictureIdRPSI =
      decodeCallback->LastDecodedRefPictureId(
          &codecSpecificInfo->codecSpecific.VP8.hasReceivedRPSI);
  if (sli_) {
    codecSpecificInfo->codecSpecific.VP8.pictureIdSLI =
        decodeCallback->LastDecodedPictureId();
    codecSpecificInfo->codecSpecific.VP8.hasReceivedSLI = true;
    sli_ = false;
  }
  printf("Encoding: %u\n", _framecnt);
  int ret = _encoder->Encode(rawImage, codecSpecificInfo, frameType);
  if (ret < 0)
    printf("Failed to encode: %u\n", _framecnt);

  if (codecSpecificInfo != NULL) {
      delete codecSpecificInfo;
      codecSpecificInfo = NULL;
  }
  if (_encodeCompleteTime > 0) {
      _totalEncodeTime += _encodeCompleteTime -
          _encodeTimes[rawImage.TimeStamp()];
  }
  else {
      _totalEncodeTime += tGetTime() - _encodeTimes[rawImage.TimeStamp()];
  }
  return false;
}

//#define FRAME_LOSS 1

int VP8RpsTest::Decode(int lossValue) {
  _sumEncBytes += _frameToDecode->_frame->GetLength();
  webrtc::EncodedImage encodedImage;
  VideoEncodedBufferToEncodedImage(*(_frameToDecode->_frame), encodedImage);
  encodedImage._completeFrame = !lossValue;
  _decodeCompleteTime = 0;
  _decodeTimes[encodedImage._timeStamp] = clock()/(double)CLOCKS_PER_SEC;
  int ret = _decoder->Decode(encodedImage, _missingFrames, NULL,
                             _frameToDecode->_codecSpecificInfo);
  // Drop every 10th frame for the second decoder
#if FRAME_LOSS
  if (_framecnt == 0 || _framecnt % 10 != 0) {
    printf("Decoding: %u\n", _framecnt);
    if (_framecnt > 1 && (_framecnt - 1) % 10 == 0)
      _missingFrames = true;
#else
  if (true) {
    if (_framecnt > 0 && _framecnt % 10 == 0) {
      encodedImage._length = std::rand() % encodedImage._length;
      printf("Decoding with loss: %u\n", _framecnt);
    }
    else
      printf("Decoding: %u\n", _framecnt);
#endif
    int ret2 = decoder2_->Decode(encodedImage, _missingFrames, NULL,
                                 _frameToDecode->_codecSpecificInfo,
                                 0 /* dummy */);

    // check return values
    if (ret < 0 || ret2 < 0) {
      return -1;
    } else if (ret2 == WEBRTC_VIDEO_CODEC_ERR_REQUEST_SLI ||
        ret2 == WEBRTC_VIDEO_CODEC_REQUEST_SLI) {
      sli_ = true;
    }

    // compare decoded images
#if FRAME_LOSS
    if (!_missingFrames) {
      if (!CheckIfBitExact(_decodedVideoBuffer.GetBuffer(),
        _decodedVideoBuffer.GetLength(),
        decoded_frame2_.GetBuffer(), _decodedVideoBuffer.GetLength())) {
        fprintf(stderr,"\n\nRPS decoder different from master: %u\n\n",
                _framecnt);
        return -1;
      }
    }
#else
    if (_framecnt > 0 && _framecnt % 10 != 0) {
      if (!CheckIfBitExact(_decodedVideoBuffer.GetBuffer(),
        _decodedVideoBuffer.GetLength(),
        decoded_frame2_.GetBuffer(), _decodedVideoBuffer.GetLength())) {
        fprintf(stderr,"\n\nRPS decoder different from master: %u\n\n",
                _framecnt);
        return -1;
      }
    }
#endif
  }
#if FRAME_LOSS
  else
    printf("Dropping %u\n", _framecnt);
#endif
  _missingFrames = false;
  return 0;
}


bool
VP8RpsTest::CheckIfBitExact(const void* ptrA, unsigned int aLengthBytes,
                            const void* ptrB, unsigned int bLengthBytes) {
  if (aLengthBytes != bLengthBytes)
    return false;
  return memcmp(ptrA, ptrB, aLengthBytes) == 0;
}

RpsDecodeCompleteCallback::RpsDecodeCompleteCallback(TestVideoBuffer* buffer)
    : decoded_frame_(buffer),
      decode_complete_(false),
      last_decoded_picture_id_(0),
      last_decoded_ref_picture_id_(0),
      updated_ref_picture_id_(false) {
}

WebRtc_Word32 RpsDecodeCompleteCallback::Decoded(webrtc::VideoFrame& image) {
  decoded_frame_->VerifyAndAllocate(image.Length());
  decoded_frame_->CopyBuffer(image.Length(), image.Buffer());
  decoded_frame_->SetWidth(image.Width());
  decoded_frame_->SetHeight(image.Height());
  decoded_frame_->SetTimeStamp(image.TimeStamp());
  decode_complete_ = true;
  return 0;
}

bool RpsDecodeCompleteCallback::DecodeComplete() {
  if (decode_complete_)
  {
    decode_complete_ = false;
    return true;
  }
  return false;
}

WebRtc_Word32 RpsDecodeCompleteCallback::ReceivedDecodedReferenceFrame(
    const WebRtc_UWord64 picture_id) {
  last_decoded_ref_picture_id_ = picture_id & 0x7FFF;
  updated_ref_picture_id_ = true;
  return 0;
}

WebRtc_Word32 RpsDecodeCompleteCallback::ReceivedDecodedFrame(
    const WebRtc_UWord64 picture_id) {
  last_decoded_picture_id_ = picture_id & 0x3F;
  return 0;
}

WebRtc_UWord64 RpsDecodeCompleteCallback::LastDecodedPictureId() const {
  return last_decoded_picture_id_;
}

WebRtc_UWord64 RpsDecodeCompleteCallback::LastDecodedRefPictureId(
    bool *updated) {
  if (updated)
    *updated = updated_ref_picture_id_;
  updated_ref_picture_id_ = false;
  return last_decoded_ref_picture_id_;
}
