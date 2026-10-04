// SPDX-License-Identifier: MPL-2.0

#pragma once

#import <Cocoa/Cocoa.h>
#include "Protocol.h"
#include <mach/mach.h>

typedef void (^FPAShimEventSink)(floorp::shim::MessageType, NSDictionary*);

@interface FPAShimView : NSView <NSTextInputClient>
- (instancetype)initWithFrame:(NSRect)frame windowID:(uint32_t)windowID
                         sink:(FPAShimEventSink)sink;
- (BOOL)beginFrame:(NSDictionary*)payload;
- (BOOL)setLayer:(NSDictionary*)payload surfacePort:(mach_port_t)port;
- (BOOL)removeLayer:(NSDictionary*)payload;
- (BOOL)commitFrame:(NSDictionary*)payload;
- (BOOL)setEditorState:(NSDictionary*)payload;
- (BOOL)setCursorState:(NSDictionary*)payload;
- (void)disconnect;
@property(nonatomic, readonly) NSUInteger presentedLayerCount;
@property(nonatomic, readonly) uint32_t committedFrameID;
@property(nonatomic) BOOL inputEnabled;
@property(nonatomic, readonly) NSCursor* nativeCursor;
@end
