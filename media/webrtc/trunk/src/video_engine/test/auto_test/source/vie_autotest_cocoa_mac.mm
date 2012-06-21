/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"

#if defined(COCOA_RENDERING)
#import "cocoa_render_view.h"
#include "vie_autotest_mac_cocoa.h"
#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "vie_autotest_main.h"

ViEAutoTestWindowManager::ViEAutoTestWindowManager()
    : _cocoaRenderView1(nil), _cocoaRenderView2(nil) {
}

int ViEAutoTestWindowManager::CreateWindows(AutoTestRect window1Size,
                                            AutoTestRect window2Size,
                                            void* window1Title,
                                            void* window2Title) {
    NSRect outWindow1Frame = NSMakeRect(window1Size.origin.x,
                                        window1Size.origin.y,
                                        window1Size.size.width,
                                        window1Size.size.height);
    outWindow1_ = [[NSWindow alloc] initWithContentRect:outWindow1Frame
                                    styleMask:NSTitledWindowMask
                                    backing:NSBackingStoreBuffered defer:NO];
    [outWindow1_ orderOut:nil];
    NSRect cocoaRenderView1Frame = NSMakeRect(0, 0, window1Size.size.width,
                                              window1Size.size.height);
    _cocoaRenderView1 = [[CocoaRenderView alloc]
                          initWithFrame:cocoaRenderView1Frame];
    [[outWindow1_ contentView] addSubview:(NSView*)_cocoaRenderView1];
    [outWindow1_ setTitle:[NSString stringWithFormat:@"%s", window1Title]];
    [outWindow1_ makeKeyAndOrderFront:NSApp];

    NSRect outWindow2Frame = NSMakeRect(window2Size.origin.x,
                                        window2Size.origin.y,
                                        window2Size.size.width,
                                        window2Size.size.height);
    outWindow2_ = [[NSWindow alloc] initWithContentRect:outWindow2Frame
                                    styleMask:NSTitledWindowMask
                                    backing:NSBackingStoreBuffered defer:NO];
    [outWindow2_ orderOut:nil];
    NSRect cocoaRenderView2Frame = NSMakeRect(0, 0, window2Size.size.width,
                                              window2Size.size.height);
    _cocoaRenderView2 = [[CocoaRenderView alloc]
                          initWithFrame:cocoaRenderView2Frame];
    [[outWindow2_ contentView] addSubview:(NSView*)_cocoaRenderView2];
    [outWindow2_ setTitle:[NSString stringWithFormat:@"%s", window2Title]];
    [outWindow2_ makeKeyAndOrderFront:NSApp];

    return 0;
}

int ViEAutoTestWindowManager::TerminateWindows() {
    [outWindow1_ close];
    [outWindow2_ close];
    return 0;
}

void* ViEAutoTestWindowManager::GetWindow1() {
    return _cocoaRenderView1;
}

void* ViEAutoTestWindowManager::GetWindow2() {
    return _cocoaRenderView2;
}

bool ViEAutoTestWindowManager::SetTopmostWindow() {
    return true;
}

int main(int argc, char * argv[]) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [NSApplication sharedApplication];

    int result = 0;
#if defined(MAC_COCOA_USE_NSRUNLOOP)
    AutoTestClass* tests = [[AutoTestClass alloc] init];

    [tests setArgc:argc argv:argv];
    [NSThread detachNewThreadSelector:@selector(autoTestWithArg:)
      toTarget:tests withObject:nil];
    // Process OS events. Blocking call.
    [[NSRunLoop mainRunLoop]run];

    result = [tests result];

#else
    ViEAutoTestMain autoTest;
    result = autoTest.RunTests(argc, argv);

#endif
    [pool release];
    return result;
}

@implementation AutoTestClass

- (void)setArgc:(int)argc argv:(char**)argv {
  argc_ = argc;
  argv_ = argv;
}

- (void)autoTestWithArg:(NSObject*)ignored {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    ViEAutoTestMain auto_test;

    result_ = auto_test.RunTests(argc_, argv_);

    [pool release];
    return;
}

- (int)result {
  return result_;
}

@end

#endif

