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
//  cocoa_full_screen_window.h
//
//

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_COCOA_FULL_SCREEN_WINDOW_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_COCOA_FULL_SCREEN_WINDOW_H_

#import <Cocoa/Cocoa.h>
//#define GRAB_ALL_SCREENS 1

@interface CocoaFullScreenWindow : NSObject {
	NSWindow*			_window;
}

-(id)init;
-(void)grabFullScreen;
-(void)releaseFullScreen;
-(NSWindow*)window;

@end

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_COCOA_FULL_SCREEN_WINDOW_H_
