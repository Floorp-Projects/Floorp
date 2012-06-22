/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_VIDEOSIZE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_VIDEOSIZE_H_
#include "StdAfx.h"
enum VideoSize
	{
		UNDEFINED, 
		SQCIF,     // 128*96       = 12 288
		QQVGA,     // 160*120      = 19 200
		QCIF,      // 176*144      = 25 344
        CGA,       // 320*200      = 64 000
		QVGA,      // 320*240      = 76 800
        SIF,       // 352*240      = 84 480
		WQVGA,     // 400*240      = 96 000
		CIF,       // 352*288      = 101 376
        W288P,     // 512*288      = 147 456 (WCIF)
        W368P,     // 640*368      = 235 520
        S_448P,      // 576*448      = 281 088
		VGA,       // 640*480      = 307 200
        S_432P,      // 720*432      = 311 040
        W432P,     // 768*432      = 331 776 (a.k.a WVGA 16:9)
        S_4SIF,      // 704*480      = 337 920
        W448P,     // 768*448      = 344 064
		NTSC,		// 720*480      = 345 600
        FW448P,    // 800*448      = 358 400
        S_768x480P,  // 768*480      = 368 640 (a.k.a WVGA 16:10)
		WVGA,      // 800*480      = 384 000
		S_4CIF,      // 704576      = 405 504
		SVGA,      // 800*600      = 480 000
        W544P,     // 960*544      = 522 240
        W576P,     // 1024*576     = 589 824 (W4CIF)
		HD,        // 960*720      = 691 200
		XGA,       // 1024*768     = 786 432
		WHD,       // 1280*720     = 921 600
		FULL_HD,   // 1440*1080    = 1 555 200
        UXGA,      // 1600*1200    = 1 920 000
		WFULL_HD,  // 1920*1080    = 2 073 600
		NUMBER_OF_VIDEO_SIZE
	};

int GetWidthHeight(VideoSize size, int& width, int& height);


#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_VIDEOSIZE_H_
