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
//  cocoa_render_view.h
//

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_COCOA_RENDER_VIEW_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_COCOA_RENDER_VIEW_H_

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>
#import <OpenGL/OpenGL.h>

@interface CocoaRenderView : NSOpenGLView {
  NSOpenGLContext* _nsOpenGLContext;
}

-(void)initCocoaRenderView:(NSOpenGLPixelFormat*)fmt;
-(void)initCocoaRenderViewFullScreen:(NSOpenGLPixelFormat*)fmt;
-(NSOpenGLContext*)nsOpenGLContext;
@end

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_COCOA_RENDER_VIEW_H_
