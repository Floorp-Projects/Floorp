/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"

#include <memory>
#include <utility>

#include "api/units/time_delta.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/linux/wayland/test/fake_screencast_stream.h"
#include "modules/desktop_capture/rgba_color.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Ge;
using ::testing::Invoke;

namespace webrtc {

constexpr TimeDelta kShortWait = TimeDelta::Seconds(2);
constexpr TimeDelta kLongWait = TimeDelta::Seconds(10);

constexpr int kBytesPerPixel = 4;
constexpr int32_t kWidth = 800;
constexpr int32_t kHeight = 640;

class PipeWireStreamTest : public ::testing::Test,
                           public FakeScreenCastStream::Observer,
                           public SharedScreenCastStream::Observer {
 public:
  PipeWireStreamTest()
      : fake_screencast_stream_(
            std::make_unique<FakeScreenCastStream>(this, kWidth, kHeight)),
        shared_screencast_stream_(new SharedScreenCastStream()) {
    shared_screencast_stream_->SetObserver(this);
  }

  ~PipeWireStreamTest() override {}

  // FakeScreenCastPortal::Observer
  MOCK_METHOD(void, OnFrameRecorded, (), (override));
  MOCK_METHOD(void, OnStreamReady, (uint32_t stream_node_id), (override));
  MOCK_METHOD(void, OnStartStreaming, (), (override));
  MOCK_METHOD(void, OnStopStreaming, (), (override));

  // SharedScreenCastStream::Observer
  MOCK_METHOD(void, OnCursorPositionChanged, (), (override));
  MOCK_METHOD(void, OnCursorShapeChanged, (), (override));
  MOCK_METHOD(void, OnDesktopFrameChanged, (), (override));
  MOCK_METHOD(void, OnFailedToProcessBuffer, (), (override));

  void StartScreenCastStream(uint32_t stream_node_id) {
    shared_screencast_stream_->StartScreenCastStream(stream_node_id);
  }

 protected:
  uint recorded_frames_ = 0;
  bool streaming_ = false;
  std::unique_ptr<FakeScreenCastStream> fake_screencast_stream_;
  rtc::scoped_refptr<SharedScreenCastStream> shared_screencast_stream_;
};

TEST_F(PipeWireStreamTest, TestPipeWire) {
  // Set expectations for PipeWire to successfully connect both streams
  rtc::Event waitConnectEvent;
  EXPECT_CALL(*this, OnStreamReady(_))
      .WillOnce(Invoke(this, &PipeWireStreamTest::StartScreenCastStream));
  EXPECT_CALL(*this, OnStartStreaming).WillOnce([&waitConnectEvent] {
    waitConnectEvent.Set();
  });

  // Give it some time to connect, the order between these shouldn't matter, but
  // we need to be sure we are connected before we proceed to work with frames.
  waitConnectEvent.Wait(kLongWait);

  rtc::Event frameRetrievedEvent;
  EXPECT_CALL(*this, OnFrameRecorded).Times(3);
  EXPECT_CALL(*this, OnDesktopFrameChanged)
      .WillRepeatedly([&frameRetrievedEvent] { frameRetrievedEvent.Set(); });

  // Record a frame in FakePipeWireStream
  RgbaColor red_color(255, 0, 0);
  fake_screencast_stream_->RecordFrame(red_color);
  frameRetrievedEvent.Wait(kShortWait);

  // Retrieve a frame from SharedScreenCastStream
  frameRetrievedEvent.Wait(kShortWait);
  std::unique_ptr<SharedDesktopFrame> frame =
      shared_screencast_stream_->CaptureFrame();

  // Check frame parameters
  ASSERT_NE(frame, nullptr);
  ASSERT_NE(frame->data(), nullptr);
  EXPECT_EQ(frame->rect().width(), kWidth);
  EXPECT_EQ(frame->rect().height(), kHeight);
  EXPECT_EQ(frame->stride(), frame->rect().width() * kBytesPerPixel);
  EXPECT_EQ(frame->data()[0], static_cast<uint8_t>(red_color.ToUInt32()));

  // Test DesktopFrameQueue
  RgbaColor green_color(0, 255, 0);
  fake_screencast_stream_->RecordFrame(green_color);
  frameRetrievedEvent.Wait(kShortWait);
  std::unique_ptr<SharedDesktopFrame> frame2 =
      shared_screencast_stream_->CaptureFrame();
  ASSERT_NE(frame2, nullptr);
  ASSERT_NE(frame2->data(), nullptr);
  EXPECT_EQ(frame2->rect().width(), kWidth);
  EXPECT_EQ(frame2->rect().height(), kHeight);
  EXPECT_EQ(frame2->stride(), frame->rect().width() * kBytesPerPixel);
  EXPECT_EQ(frame2->data()[0], static_cast<uint8_t>(green_color.ToUInt32()));

  // Thanks to DesktopFrameQueue we should be able to have two frames shared
  EXPECT_EQ(frame->IsShared(), true);
  EXPECT_EQ(frame2->IsShared(), true);
  EXPECT_NE(frame->data(), frame2->data());

  // This should result into overwriting a frame in use
  rtc::Event frameRecordedEvent;
  RgbaColor blue_color(0, 0, 255);
  EXPECT_CALL(*this, OnFailedToProcessBuffer).WillOnce([&frameRecordedEvent] {
    frameRecordedEvent.Set();
  });

  fake_screencast_stream_->RecordFrame(blue_color);
  frameRecordedEvent.Wait(kShortWait);

  // Test disconnection from stream
  EXPECT_CALL(*this, OnStopStreaming);
  shared_screencast_stream_->StopScreenCastStream();
}

}  // namespace webrtc
