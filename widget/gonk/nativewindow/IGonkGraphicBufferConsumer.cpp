/*
 * Copyright (C) 2013 The Android Open Source Project
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

#define EGL_EGLEXT_PROTOTYPES

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>

#include <binder/Parcel.h>
#include <binder/IInterface.h>

#include <gui/IConsumerListener.h>
#include "IGonkGraphicBufferConsumer.h"

#include <ui/GraphicBuffer.h>
#include <ui/Fence.h>

#include <system/window.h>

namespace android {
// ---------------------------------------------------------------------------

IGonkGraphicBufferConsumer::BufferItem::BufferItem() :
    mTransform(0),
    mScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
    mTimestamp(0),
    mIsAutoTimestamp(false),
    mFrameNumber(0),
    mBuf(INVALID_BUFFER_SLOT),
    mIsDroppable(false),
    mAcquireCalled(false),
    mTransformToDisplayInverse(false),
    mSurfaceDescriptor(SurfaceDescriptor()) {
    mCrop.makeInvalid();
}

size_t IGonkGraphicBufferConsumer::BufferItem::getPodSize() const {
    size_t c =  sizeof(mCrop) +
            sizeof(mTransform) +
            sizeof(mScalingMode) +
            sizeof(mTimestamp) +
            sizeof(mIsAutoTimestamp) +
            sizeof(mFrameNumber) +
            sizeof(mBuf) +
            sizeof(mIsDroppable) +
            sizeof(mAcquireCalled) +
            sizeof(mTransformToDisplayInverse);
    return c;
}

size_t IGonkGraphicBufferConsumer::BufferItem::getFlattenedSize() const {
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

size_t IGonkGraphicBufferConsumer::BufferItem::getFdCount() const {
    size_t c = 0;
    if (mGraphicBuffer != 0) {
        c += mGraphicBuffer->getFdCount();
    }
    if (mFence != 0) {
        c += mFence->getFdCount();
    }
    return c;
}

status_t IGonkGraphicBufferConsumer::BufferItem::flatten(
        void*& buffer, size_t& size, int*& fds, size_t& count) const {

    // make sure we have enough space
    if (count < BufferItem::getFlattenedSize()) {
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
    FlattenableUtils::write(buffer, size, mBuf);
    FlattenableUtils::write(buffer, size, mIsDroppable);
    FlattenableUtils::write(buffer, size, mAcquireCalled);
    FlattenableUtils::write(buffer, size, mTransformToDisplayInverse);

    return NO_ERROR;
}

status_t IGonkGraphicBufferConsumer::BufferItem::unflatten(
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
    FlattenableUtils::read(buffer, size, mBuf);
    FlattenableUtils::read(buffer, size, mIsDroppable);
    FlattenableUtils::read(buffer, size, mAcquireCalled);
    FlattenableUtils::read(buffer, size, mTransformToDisplayInverse);

    return NO_ERROR;
}

// ---------------------------------------------------------------------------

enum {
    ACQUIRE_BUFFER = IBinder::FIRST_CALL_TRANSACTION,
    RELEASE_BUFFER,
    CONSUMER_CONNECT,
    CONSUMER_DISCONNECT,
    GET_RELEASED_BUFFERS,
    SET_DEFAULT_BUFFER_SIZE,
    SET_DEFAULT_MAX_BUFFER_COUNT,
    DISABLE_ASYNC_BUFFER,
    SET_MAX_ACQUIRED_BUFFER_COUNT,
    SET_CONSUMER_NAME,
    SET_DEFAULT_BUFFER_FORMAT,
    SET_CONSUMER_USAGE_BITS,
    SET_TRANSFORM_HINT,
    DUMP,
};

class BpGonkGraphicBufferConsumer : public BpInterface<IGonkGraphicBufferConsumer>
{
public:
    BpGonkGraphicBufferConsumer(const sp<IBinder>& impl)
        : BpInterface<IGonkGraphicBufferConsumer>(impl)
    {
    }

    virtual status_t acquireBuffer(BufferItem *buffer, nsecs_t presentWhen) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt64(presentWhen);
        status_t result = remote()->transact(ACQUIRE_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.read(*buffer);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t releaseBuffer(int buf, uint64_t frameNumber,
            const sp<Fence>& releaseFence) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(buf);
        data.writeInt64(frameNumber);
        data.write(*releaseFence);
        status_t result = remote()->transact(RELEASE_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer, bool controlledByApp) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeStrongBinder(consumer->asBinder());
        data.writeInt32(controlledByApp);
        status_t result = remote()->transact(CONSUMER_CONNECT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t consumerDisconnect() {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        status_t result = remote()->transact(CONSUMER_DISCONNECT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t getReleasedBuffers(uint32_t* slotMask) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        status_t result = remote()->transact(GET_RELEASED_BUFFERS, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        *slotMask = reply.readInt32();
        return reply.readInt32();
    }

    virtual status_t setDefaultBufferSize(uint32_t w, uint32_t h) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(w);
        data.writeInt32(h);
        status_t result = remote()->transact(SET_DEFAULT_BUFFER_SIZE, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setDefaultMaxBufferCount(int bufferCount) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(bufferCount);
        status_t result = remote()->transact(SET_DEFAULT_MAX_BUFFER_COUNT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t disableAsyncBuffer() {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        status_t result = remote()->transact(DISABLE_ASYNC_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(maxAcquiredBuffers);
        status_t result = remote()->transact(SET_MAX_ACQUIRED_BUFFER_COUNT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual void setConsumerName(const String8& name) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeString8(name);
        remote()->transact(SET_CONSUMER_NAME, data, &reply);
    }

    virtual status_t setDefaultBufferFormat(uint32_t defaultFormat) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(defaultFormat);
        status_t result = remote()->transact(SET_DEFAULT_BUFFER_FORMAT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setConsumerUsageBits(uint32_t usage) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(usage);
        status_t result = remote()->transact(SET_CONSUMER_USAGE_BITS, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setTransformHint(uint32_t hint) {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(hint);
        status_t result = remote()->transact(SET_TRANSFORM_HINT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual void dump(String8& result, const char* prefix) const {
        Parcel data, reply;
        data.writeInterfaceToken(IGonkGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeString8(result);
        data.writeString8(String8(prefix ? prefix : ""));
        remote()->transact(DUMP, data, &reply);
        reply.readString8();
    }
};

IMPLEMENT_META_INTERFACE(GonkGraphicBufferConsumer, "android.gui.IGonkGraphicBufferConsumer");
// ----------------------------------------------------------------------

status_t BnGonkGraphicBufferConsumer::onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case ACQUIRE_BUFFER: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            BufferItem item;
            int64_t presentWhen = data.readInt64();
            status_t result = acquireBuffer(&item, presentWhen);
            status_t err = reply->write(item);
            if (err) return err;
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case RELEASE_BUFFER: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            int buf = data.readInt32();
            uint64_t frameNumber = data.readInt64();
            sp<Fence> releaseFence = new Fence();
            status_t err = data.read(*releaseFence);
            if (err) return err;
            status_t result = releaseBuffer(buf, frameNumber, releaseFence);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;

        case CONSUMER_CONNECT: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            sp<IConsumerListener> consumer = IConsumerListener::asInterface( data.readStrongBinder() );
            bool controlledByApp = data.readInt32();
            status_t result = consumerConnect(consumer, controlledByApp);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;

        case CONSUMER_DISCONNECT: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            status_t result = consumerDisconnect();
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case GET_RELEASED_BUFFERS: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t slotMask;
            status_t result = getReleasedBuffers(&slotMask);
            reply->writeInt32(slotMask);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case SET_DEFAULT_BUFFER_SIZE: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t w = data.readInt32();
            uint32_t h = data.readInt32();
            status_t result = setDefaultBufferSize(w, h);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case SET_DEFAULT_MAX_BUFFER_COUNT: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t bufferCount = data.readInt32();
            status_t result = setDefaultMaxBufferCount(bufferCount);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case DISABLE_ASYNC_BUFFER: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            status_t result = disableAsyncBuffer();
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case SET_MAX_ACQUIRED_BUFFER_COUNT: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t maxAcquiredBuffers = data.readInt32();
            status_t result = setMaxAcquiredBufferCount(maxAcquiredBuffers);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case SET_CONSUMER_NAME: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            setConsumerName( data.readString8() );
            return NO_ERROR;
        } break;
        case SET_DEFAULT_BUFFER_FORMAT: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t defaultFormat = data.readInt32();
            status_t result = setDefaultBufferFormat(defaultFormat);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case SET_CONSUMER_USAGE_BITS: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t usage = data.readInt32();
            status_t result = setConsumerUsageBits(usage);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
        case SET_TRANSFORM_HINT: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            uint32_t hint = data.readInt32();
            status_t result = setTransformHint(hint);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;

        case DUMP: {
            CHECK_INTERFACE(IGonkGraphicBufferConsumer, data, reply);
            String8 result = data.readString8();
            String8 prefix = data.readString8();
            static_cast<IGonkGraphicBufferConsumer*>(this)->dump(result, prefix);
            reply->writeString8(result);
            return NO_ERROR;
        }
    }
    return BBinder::onTransact(code, data, reply, flags);
}

}; // namespace android
