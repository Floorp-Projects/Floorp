/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import "cocoa_render_view.h"
#include "trace.h"

using namespace webrtc;

@implementation CocoaRenderView

-(void)initCocoaRenderView:(NSOpenGLPixelFormat*)fmt{
	
	self = [super initWithFrame:[self frame] pixelFormat:[fmt autorelease]];
	if (self == nil){
		
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not create instance", __FUNCTION__, __LINE__); 
	}
	
	
	_nsOpenGLContext = [self openGLContext];

}

-(NSOpenGLContext*)nsOpenGLContext {
    return _nsOpenGLContext;
}

-(void)initCocoaRenderViewFullScreen:(NSOpenGLPixelFormat*)fmt{
	
	NSRect screenRect = [[NSScreen mainScreen]frame];
//	[_windowRef setFrame:screenRect];
//	[_windowRef setBounds:screenRect];
	self = [super initWithFrame:screenRect	pixelFormat:[fmt autorelease]];
	if (self == nil){
		
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not create instance", __FUNCTION__, __LINE__); 
	}
	
	_nsOpenGLContext = [self openGLContext];

}

@end


