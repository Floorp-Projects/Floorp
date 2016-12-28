/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_IOS_RENDER_VIEW_H_
#define WEBRTC_MODULES_VIDEO_RENDER_IOS_RENDER_VIEW_H_

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

#include "webrtc/modules/video_render/ios/open_gles20.h"

@interface VideoRenderIosView : UIView

- (BOOL)createContext;
- (BOOL)presentFramebuffer;
- (BOOL)renderFrame:(webrtc::VideoFrame*)frameToRender;
- (BOOL)setCoordinatesForZOrder:(const float)zOrder
                           Left:(const float)left
                            Top:(const float)top
                          Right:(const float)right
                         Bottom:(const float)bottom;

@property(nonatomic, retain) EAGLContext* context;

@end

#endif  // WEBRTC_MODULES_VIDEO_RENDER_IOS_RENDER_VIEW_H_
