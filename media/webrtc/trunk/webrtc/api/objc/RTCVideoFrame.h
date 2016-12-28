/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RTCVideoFrame : NSObject

/** Width without rotation applied. */
@property(nonatomic, readonly) size_t width;

/** Height without rotation applied. */
@property(nonatomic, readonly) size_t height;
@property(nonatomic, readonly) size_t chromaWidth;
@property(nonatomic, readonly) size_t chromaHeight;
@property(nonatomic, readonly) size_t chromaSize;
// These can return NULL if the object is not backed by a buffer.
@property(nonatomic, readonly, nullable) const uint8_t *yPlane;
@property(nonatomic, readonly, nullable) const uint8_t *uPlane;
@property(nonatomic, readonly, nullable) const uint8_t *vPlane;
@property(nonatomic, readonly) int32_t yPitch;
@property(nonatomic, readonly) int32_t uPitch;
@property(nonatomic, readonly) int32_t vPitch;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
