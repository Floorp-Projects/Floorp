/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testAPI.h"

#include <iostream>

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <QTKit/QTKit.h>
#include <sys/time.h>

#include "common_types.h"
#import "webrtc/modules/video_render//mac/cocoa_render_view.h"
#include "module_common_types.h"
#include "process_thread.h"
#include "tick_util.h"
#include "trace.h"
#include "video_render_defines.h"
#include "video_render.h"

using namespace webrtc;

int WebRtcCreateWindow(CocoaRenderView*& cocoaRenderer, int winNum, int width, int height)
{
    // In Cocoa, rendering is not done directly to a window like in Windows and Linux.
    // It is rendererd to a Subclass of NSOpenGLView

    // create cocoa container window
    NSRect outWindowFrame = NSMakeRect(200, 800, width + 20, height + 20);
    NSWindow* outWindow = [[NSWindow alloc] initWithContentRect:outWindowFrame 
                                                      styleMask:NSTitledWindowMask 
                                                        backing:NSBackingStoreBuffered 
                                                          defer:NO];
    [outWindow orderOut:nil];
    [outWindow setTitle:@"Cocoa Renderer"];
    [outWindow setBackgroundColor:[NSColor blueColor]];

    // create renderer and attach to window
    NSRect cocoaRendererFrame = NSMakeRect(10, 10, width, height);
    cocoaRenderer = [[CocoaRenderView alloc] initWithFrame:cocoaRendererFrame];
    [[outWindow contentView] addSubview:(NSView*)cocoaRenderer];

    [outWindow makeKeyAndOrderFront:NSApp];

    return 0;
}

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];

    CocoaRenderView* testWindow;
    WebRtcCreateWindow(testWindow, 0, 352, 288);
    VideoRenderType windowType = kRenderCocoa;
    void* window = (void*)testWindow;

    RunVideoRenderTests(window, windowType);

    [pool release];
}
