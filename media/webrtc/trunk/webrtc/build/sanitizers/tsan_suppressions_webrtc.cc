/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the WebRTC suppressions for ThreadSanitizer.
// Please refer to
// http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for more info.

#if defined(THREAD_SANITIZER)

// Please make sure the code below declares a single string variable
// kTSanDefaultSuppressions contains TSan suppressions delimited by newlines.
// See http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for the instructions on writing suppressions.
char kTSanDefaultSuppressions[] =

// WebRTC specific suppressions.

// Split up suppressions covered previously by thread.cc and messagequeue.cc.
"race:rtc::MessageQueue::Quit\n"
"race:FileVideoCapturerTest::VideoCapturerListener::OnFrameCaptured\n"
"race:vp8cx_remove_encoder_threads\n"
"race:third_party/libvpx/source/libvpx/vp9/common/vp9_scan.h\n"

// Usage of trace callback and trace level is racy in libjingle_media_unittests.
// https://code.google.com/p/webrtc/issues/detail?id=3372
"race:webrtc::TraceImpl::WriteToFile\n"
"race:webrtc::VideoEngine::SetTraceFilter\n"
"race:webrtc::VoiceEngine::SetTraceFilter\n"
"race:webrtc::Trace::set_level_filter\n"
"race:webrtc::GetStaticInstance<webrtc::TraceImpl>\n"

// Audio processing
// https://code.google.com/p/webrtc/issues/detail?id=2521 for details.
"race:webrtc/modules/audio_processing/aec/aec_core.c\n"
"race:webrtc/modules/audio_processing/aec/aec_rdft.c\n"

// rtc_unittest
// https://code.google.com/p/webrtc/issues/detail?id=3911 for details.
"race:rtc::AsyncInvoker::OnMessage\n"
"race:rtc::FireAndForgetAsyncClosure<FunctorB>::Execute\n"
"race:rtc::MessageQueueManager::Clear\n"
"race:rtc::Thread::Clear\n"
// https://code.google.com/p/webrtc/issues/detail?id=2080
"race:webrtc/base/logging.cc\n"
"race:webrtc/base/sharedexclusivelock_unittest.cc\n"
"race:webrtc/base/signalthread_unittest.cc\n"
// https://code.google.com/p/webrtc/issues/detail?id=4456
"deadlock:rtc::MessageQueueManager::Clear\n"
"deadlock:rtc::MessageQueueManager::ClearInternal\n"

// libjingle_p2p_unittest
// https://code.google.com/p/webrtc/issues/detail?id=2079
"race:webrtc/base/testclient.cc\n"
"race:webrtc/base/virtualsocketserver.cc\n"
"race:talk/p2p/base/stunserver_unittest.cc\n"

// third_party/usrsctp
// TODO(jiayl): https://code.google.com/p/webrtc/issues/detail?id=3492
"race:user_sctp_timer_iterate\n"

// Potential deadlocks detected after roll in r6516.
// https://code.google.com/p/webrtc/issues/detail?id=3509
"deadlock:webrtc::RTCPReceiver::SetSsrcs\n"
"deadlock:webrtc::test::UdpSocketManagerPosixImpl::RemoveSocket\n"
"deadlock:webrtc::vcm::VideoReceiver::RegisterPacketRequestCallback\n"
"deadlock:webrtc::ViECaptureImpl::ConnectCaptureDevice\n"
"deadlock:webrtc::ViEChannel::StartSend\n"
"deadlock:webrtc::ViECodecImpl::GetSendSideDelay\n"
"deadlock:webrtc::ViEEncoder::OnLocalSsrcChanged\n"
"deadlock:webrtc::ViESender::RegisterSendTransport\n"

// TODO(pbos): Trace events are racy due to lack of proper POD atomics.
// https://code.google.com/p/webrtc/issues/detail?id=2497
"race:*trace_event_unique_catstatic*\n"

// End of suppressions.
;  // Please keep this semicolon.

#endif  // THREAD_SANITIZER
