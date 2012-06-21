/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../interface/vie_autotest_android.h"

#include <android/log.h>
#include <stdio.h>

#include "vie_autotest.h"
#include "vie_autotest_defines.h"

int ViEAutoTestAndroid::RunAutotest(int testSelection, int subTestSelection,
                                    void* window1, void* window2,
                                    void* javaVM, void* env, void* context) {
  ViEAutoTest vieAutoTest(window1, window2);
  ViETest::Log("RunAutoTest(%d, %d)", testSelection, subTestSelection);
  webrtc::VideoEngine::SetAndroidObjects(javaVM, context);
#ifndef WEBRTC_ANDROID_OPENSLES
  // voice engine calls into ADM directly
  webrtc::VoiceEngine::SetAndroidAudioDeviceObjects(javaVM, env, context);
#endif

  if (subTestSelection == 0) {
    // Run all selected test
    switch (testSelection) {
      case 0:
        vieAutoTest.ViEStandardTest();
        break;
      case 1:
        vieAutoTest.ViEAPITest();
        break;
      case 2:
        vieAutoTest.ViEExtendedTest();
        break;
      case 3:
        vieAutoTest.ViELoopbackCall();
        break;
      default:
        break;
    }
  }

  switch (testSelection) {
    case 0: // Specific standard test
      switch (subTestSelection) {
        case 1: // base
          vieAutoTest.ViEBaseStandardTest();
          break;

        case 2: // capture
          vieAutoTest.ViECaptureStandardTest();
          break;

        case 3: // codec
          vieAutoTest.ViECodecStandardTest();
          break;

        case 5: //encryption
          vieAutoTest.ViEEncryptionStandardTest();
          break;

        case 6: // file
          vieAutoTest.ViEFileStandardTest();
          break;

        case 7: // image process
          vieAutoTest.ViEImageProcessStandardTest();
          break;

        case 8: // network
          vieAutoTest.ViENetworkStandardTest();
          break;

        case 9: // Render
          vieAutoTest.ViERenderStandardTest();
          break;

        case 10: // RTP/RTCP
          vieAutoTest.ViERtpRtcpStandardTest();
          break;

        default:
          break;
      }
      break;

    case 1:// specific API
      switch (subTestSelection) {
        case 1: // base
          vieAutoTest.ViEBaseAPITest();
          break;

        case 2: // capture
          vieAutoTest.ViECaptureAPITest();
          break;

        case 3: // codec
          vieAutoTest.ViECodecAPITest();
          break;

        case 5: //encryption
          vieAutoTest.ViEEncryptionAPITest();
          break;

        case 6: // file
          vieAutoTest.ViEFileAPITest();
          break;

        case 7: // image process
          vieAutoTest.ViEImageProcessAPITest();
          break;

        case 8: // network
          vieAutoTest.ViENetworkAPITest();
          break;

        case 9: // Render
          vieAutoTest.ViERenderAPITest();
          break;

        case 10: // RTP/RTCP
          vieAutoTest.ViERtpRtcpAPITest();
          break;
        case 11:
          break;

        default:
          break;
      }
      break;

    case 2:// specific extended
      switch (subTestSelection) {
        case 1: // base
          vieAutoTest.ViEBaseExtendedTest();
          break;

        case 2: // capture
          vieAutoTest.ViECaptureExtendedTest();
          break;

        case 3: // codec
          vieAutoTest.ViECodecExtendedTest();
          break;

        case 5: //encryption
          vieAutoTest.ViEEncryptionExtendedTest();
          break;

        case 6: // file
          vieAutoTest.ViEFileExtendedTest();
          break;

        case 7: // image process
          vieAutoTest.ViEImageProcessExtendedTest();
          break;

        case 8: // network
          vieAutoTest.ViENetworkExtendedTest();
          break;

        case 9: // Render
          vieAutoTest.ViERenderExtendedTest();
          break;

        case 10: // RTP/RTCP
          vieAutoTest.ViERtpRtcpExtendedTest();
          break;

        case 11:
          break;

        default:
          break;
      }
      break;

    case 3:
      vieAutoTest.ViELoopbackCall();
      break;

    default:
      break;
    }

  return 0;
}

int main(int argc, char** argv) {
  // TODO(leozwang): Add real tests here
  return 0;
}
