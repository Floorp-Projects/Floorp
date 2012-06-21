/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  GUI_Defines.h
 *
 */


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_SIMPLECOCOAGUI_GUI_DEFINES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_SIMPLECOCOAGUI_GUI_DEFINES_H_

#define		ViE_TEST(x) if(-1 == x){ \
int errNum = _ptrViEBase->LastError();	\
NSLog(@"ERROR: %d at %s:%d", errNum, __FUNCTION__, __LINE__); \
} 


// Video Engine Related
#define	V_CAPTURE_DEVICE_INDEX		0
#define V_VIE_CAPTURE_ID			747
#define V_DEVICE_NAME_LENGTH		256
#define V_CODEC_INDEX		2
#define V_IP_ADDRESS		"127.0.0.1"
#define V_RTP_PORT			8000



#endif	// WEBRTC_VIDEO_ENGINE_MAIN_TEST_SIMPLECOCOAGUI_GUI_DEFINES_H_
