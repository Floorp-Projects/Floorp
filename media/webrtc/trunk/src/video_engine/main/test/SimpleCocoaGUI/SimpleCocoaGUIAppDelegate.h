/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  SimpleCocoaGUIAppDelegate.h
//

#import <Cocoa/Cocoa.h>
#include <iostream>
using namespace std;

@class ViECocoaRenderView;

#include "GUI_Defines.h"

#include "common_types.h"
#include "voe_base.h"

#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_file.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "vie_errors.h"



@interface SimpleCocoaGUIAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow*						_window;
	IBOutlet NSOpenGLView*			_vieCocoaRenderView1;
	IBOutlet NSOpenGLView*			_vieCocoaRenderView2;
	IBOutlet NSButton*				_butRestartLoopback;
	VideoEngine*				_ptrViE;
	ViEBase*					_ptrViEBase;
	ViECapture*					_ptrViECapture;
	ViERender*					_ptrViERender;
	ViECodec*					_ptrViECodec;
	ViENetwork*					_ptrViENetwork;
	
	bool							_fullScreen;
	int								_videoChannel;
	
	int _captureId;
	
	VideoEngine* ptrViE;
	ViEBase* ptrViEBase;
	ViECapture* ptrViECapture;
	ViERTP_RTCP* ptrViERtpRtcp;
	ViERender* ptrViERender;
	ViECodec* ptrViECodec;
	ViENetwork* ptrViENetwork;
}

@property (assign) IBOutlet NSWindow* window;
-(void)createUI:(bool)fullScreen;
-(void)initViECocoaTest;
-(void)initializeVariables;
-(void)NSLogVideoCodecs;
-(void)startViECocoaTest;
-(int)initLoopback;
-(int)ioLooback;
-(int)startLoopback;
-(int)stopLooback;

-(IBAction)handleRestart:(id)sender;


@end
