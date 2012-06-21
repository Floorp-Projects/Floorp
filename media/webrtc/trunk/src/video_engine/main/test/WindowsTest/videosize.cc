/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "VideoSize.h"
int GetWidthHeight( VideoSize size, int& width, int& height)
{
	switch(size)
	{
	case SQCIF:
		width = 128;
		height = 96;
		return 0;
	case QQVGA:
		width = 160;
		height = 120;
		return 0;
	case QCIF:
		width = 176;
		height = 144;
		return 0;
	case CGA:
		width = 320;
		height = 200;
		return 0;
	case QVGA:
		width = 320;
		height = 240;
		return 0;
	case SIF:
		width = 352;
		height = 240;
		return 0;
    case WQVGA:
		width = 400;
		height = 240;
		return 0;
	case CIF:
		width = 352;
		height = 288;
		return 0;
	case W288P:
		width = 512;
		height = 288;
		return 0;
    case W368P:
        width = 640;
        height = 368;
        return 0;
	case S_448P:
		width = 576;
		height = 448;
		return 0;
	case VGA:
		width = 640;
		height = 480;
		return 0;
	case S_432P:
		width = 720;
		height = 432;
		return 0;
	case W432P:
		width = 768;
		height = 432;
		return 0;
	case S_4SIF:
		width = 704;
		height = 480;
		return 0;
	case W448P:
		width = 768;
		height = 448;
		return 0;
	case NTSC:
		width = 720;
		height = 480;
		return 0;
    case FW448P:
        width = 800;
        height = 448;
        return 0;
    case S_768x480P:
        width = 768;
        height = 480;
        return 0;
    case WVGA:
		width = 800;
		height = 480;
		return 0;
	case S_4CIF:
		width = 704;
		height = 576;
		return 0;
	case SVGA:
		width = 800;
		height = 600; 
		return 0;
    case W544P:
        width = 960;
        height = 544;
        return 0;
	case W576P:
		width = 1024;
		height = 576;
		return 0;
	case HD:
		width = 960;
		height = 720;
		return 0;
	case XGA:
		width = 1024;
		height = 768;
		return 0;
	case FULL_HD:
		width = 1440;
		height = 1080;
		return 0;	
	case WHD:
		width = 1280;
		height = 720;
		return 0;
    case UXGA:
        width = 1600;
        height = 1200;
        return 0;
	case WFULL_HD:
		width = 1920;
		height = 1080;
		return 0;
	default:
		return -1;
	}
	return -1;
}