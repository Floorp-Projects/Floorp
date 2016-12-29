/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_render/mac/cocoa_full_screen_window.h"
#include "webrtc/system_wrappers/include/trace.h"

using namespace webrtc;

@implementation CocoaFullScreenWindow

-(id)init{	
	
	self = [super init];
	if(!self){
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d COULD NOT CREATE INSTANCE", __FUNCTION__, __LINE__); 
		return nil;
	}
	
	
	WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, 0, "%s:%d Created instance", __FUNCTION__, __LINE__); 
	return self;
}

-(void)grabFullScreen{
	
#ifdef GRAB_ALL_SCREENS
	if(CGCaptureAllDisplays() != kCGErrorSuccess)
#else
	if(CGDisplayCapture(kCGDirectMainDisplay) != kCGErrorSuccess)
#endif
	{
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not capture main level", __FUNCTION__, __LINE__); 
	}
	
	// get the shielding window level
	int windowLevel = CGShieldingWindowLevel();
	
	// get the screen rect of main display
	NSRect screenRect = [[NSScreen mainScreen]frame];
	
	_window = [[NSWindow alloc]initWithContentRect:screenRect 
										   styleMask:NSBorderlessWindowMask
											 backing:NSBackingStoreBuffered
											   defer:NO
											  screen:[NSScreen mainScreen]];
	
	[_window setLevel:windowLevel];
	[_window setBackgroundColor:[NSColor blackColor]];
	[_window makeKeyAndOrderFront:nil];

}
 
-(void)releaseFullScreen
{
	[_window orderOut:self];
	
#ifdef GRAB_ALL_SCREENS
	if(CGReleaseAllDisplays() != kCGErrorSuccess)
#else
	if(CGDisplayRelease(kCGDirectMainDisplay) != kCGErrorSuccess)
#endif
	{
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not release the displays", __FUNCTION__, __LINE__); 
	}		
}

- (NSWindow*)window
{
  return _window;
}

- (void) dealloc
{
	[self releaseFullScreen];
	[super dealloc];
}	


	
@end
