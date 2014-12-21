/*
 * Copyright 2014 The Android Open Source Project
 * Copyright (C) 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GonkBufferItem.h"

#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>

#include <system/window.h>

namespace android {

GonkBufferItem::GonkBufferItem() :
    mTransform(0),
    mScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
    mTimestamp(0),
    mIsAutoTimestamp(false),
    mFrameNumber(0),
    mSlot(INVALID_BUFFER_SLOT),
    mIsDroppable(false),
    mAcquireCalled(false),
    mTransformToDisplayInverse(false) {
    mCrop.makeInvalid();
}

GonkBufferItem::operator IGonkGraphicBufferConsumer::BufferItem() const {
    IGonkGraphicBufferConsumer::BufferItem bufferItem;
    bufferItem.mGraphicBuffer = mGraphicBuffer;
    bufferItem.mFence = mFence;
    bufferItem.mCrop = mCrop;
    bufferItem.mTransform = mTransform;
    bufferItem.mScalingMode = mScalingMode;
    bufferItem.mTimestamp = mTimestamp;
    bufferItem.mIsAutoTimestamp = mIsAutoTimestamp;
    bufferItem.mFrameNumber = mFrameNumber;
    bufferItem.mBuf = mSlot;
    bufferItem.mIsDroppable = mIsDroppable;
    bufferItem.mAcquireCalled = mAcquireCalled;
    bufferItem.mTransformToDisplayInverse = mTransformToDisplayInverse;
    return bufferItem;
}

size_t GonkBufferItem::getPodSize() const {
    size_t c =  sizeof(mCrop) +
            sizeof(mTransform) +
            sizeof(mScalingMode) +
            sizeof(mTimestamp) +
            sizeof(mIsAutoTimestamp) +
            sizeof(mFrameNumber) +
            sizeof(mSlot) +
            sizeof(mIsDroppable) +
            sizeof(mAcquireCalled) +
            sizeof(mTransformToDisplayInverse);
    return c;
}

size_t GonkBufferItem::getFlattenedSize() const {
    size_t c = 0;
    if (mGraphicBuffer != 0) {
        c += mGraphicBuffer->getFlattenedSize();
        FlattenableUtils::align<4>(c);
    }
    if (mFence != 0) {
        c += mFence->getFlattenedSize();
        FlattenableUtils::align<4>(c);
    }
    return sizeof(int32_t) + c + getPodSize();
}

size_t GonkBufferItem::getFdCount() const {
    size_t c = 0;
    if (mGraphicBuffer != 0) {
        c += mGraphicBuffer->getFdCount();
    }
    if (mFence != 0) {
        c += mFence->getFdCount();
    }
    return c;
}

status_t GonkBufferItem::flatten(
        void*& buffer, size_t& size, int*& fds, size_t& count) const {

    // make sure we have enough space
    if (count < GonkBufferItem::getFlattenedSize()) {
        return NO_MEMORY;
    }

    // content flags are stored first
    uint32_t& flags = *static_cast<uint32_t*>(buffer);

    // advance the pointer
    FlattenableUtils::advance(buffer, size, sizeof(uint32_t));

    flags = 0;
    if (mGraphicBuffer != 0) {
        status_t err = mGraphicBuffer->flatten(buffer, size, fds, count);
        if (err) return err;
        size -= FlattenableUtils::align<4>(buffer);
        flags |= 1;
    }
    if (mFence != 0) {
        status_t err = mFence->flatten(buffer, size, fds, count);
        if (err) return err;
        size -= FlattenableUtils::align<4>(buffer);
        flags |= 2;
    }

    // check we have enough space (in case flattening the fence/graphicbuffer lied to us)
    if (size < getPodSize()) {
        return NO_MEMORY;
    }

    FlattenableUtils::write(buffer, size, mCrop);
    FlattenableUtils::write(buffer, size, mTransform);
    FlattenableUtils::write(buffer, size, mScalingMode);
    FlattenableUtils::write(buffer, size, mTimestamp);
    FlattenableUtils::write(buffer, size, mIsAutoTimestamp);
    FlattenableUtils::write(buffer, size, mFrameNumber);
    FlattenableUtils::write(buffer, size, mSlot);
    FlattenableUtils::write(buffer, size, mIsDroppable);
    FlattenableUtils::write(buffer, size, mAcquireCalled);
    FlattenableUtils::write(buffer, size, mTransformToDisplayInverse);

    return NO_ERROR;
}

status_t GonkBufferItem::unflatten(
        void const*& buffer, size_t& size, int const*& fds, size_t& count) {

    if (size < sizeof(uint32_t))
        return NO_MEMORY;

    uint32_t flags = 0;
    FlattenableUtils::read(buffer, size, flags);

    if (flags & 1) {
        mGraphicBuffer = new GraphicBuffer();
        status_t err = mGraphicBuffer->unflatten(buffer, size, fds, count);
        if (err) return err;
        size -= FlattenableUtils::align<4>(buffer);
    }

    if (flags & 2) {
        mFence = new Fence();
        status_t err = mFence->unflatten(buffer, size, fds, count);
        if (err) return err;
        size -= FlattenableUtils::align<4>(buffer);
    }

    // check we have enough space
    if (size < getPodSize()) {
        return NO_MEMORY;
    }

    FlattenableUtils::read(buffer, size, mCrop);
    FlattenableUtils::read(buffer, size, mTransform);
    FlattenableUtils::read(buffer, size, mScalingMode);
    FlattenableUtils::read(buffer, size, mTimestamp);
    FlattenableUtils::read(buffer, size, mIsAutoTimestamp);
    FlattenableUtils::read(buffer, size, mFrameNumber);
    FlattenableUtils::read(buffer, size, mSlot);
    FlattenableUtils::read(buffer, size, mIsDroppable);
    FlattenableUtils::read(buffer, size, mAcquireCalled);
    FlattenableUtils::read(buffer, size, mTransformToDisplayInverse);

    return NO_ERROR;
}

const char* GonkBufferItem::scalingModeName(uint32_t scalingMode) {
    switch (scalingMode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE: return "FREEZE";
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW: return "SCALE_TO_WINDOW";
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP: return "SCALE_CROP";
        default: return "Unknown";
    }
}

} // namespace android
