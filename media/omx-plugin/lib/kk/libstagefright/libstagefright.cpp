/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Types.h"
#define STAGEFRIGHT_EXPORT __attribute__ ((visibility ("default")))

#include "media/stagefright/MediaCodec.h"
#include "media/stagefright/foundation/ALooper.h"
#include "media/stagefright/foundation/AMessage.h"
#include "media/stagefright/foundation/ABuffer.h"
#include "media/ICrypto.h"
#include "gui/Surface.h"


namespace android {
	
// stub for MediaCodec
MOZ_EXPORT sp<MediaCodec> MediaCodec::CreateByType(
        const sp<ALooper> &looper, const char *mime, bool encoder)
{
	return 0;
}

MOZ_EXPORT status_t
MediaCodec::start()
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::release()
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::stop()
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::configure(
        const sp<AMessage> &format,
        const sp<Surface> &nativeWindow,
        const sp<ICrypto> &crypto,
        uint32_t flags)
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::getInputBuffers(Vector<sp<ABuffer> > *buffers) const
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::getOutputBuffers(Vector<sp<ABuffer> > *buffers) const
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::dequeueInputBuffer(size_t *index, int64_t timeoutUs)
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::queueInputBuffer(size_t index,
							 size_t offset,
							 size_t size,
							 int64_t presentationTimeUs,
							 uint32_t flags,
							 AString *errorDetailMsg)
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::dequeueOutputBuffer(size_t *index,
										 size_t *offset,
										 size_t *size,
										 int64_t *presentationTimeUs,
										 uint32_t *flags,
										 int64_t timeoutUs)
{
	return 0;
}

MOZ_EXPORT status_t 
MediaCodec::releaseOutputBuffer(size_t index)
{
	return 0;
}

MOZ_EXPORT status_t
MediaCodec::setParameters(const sp<AMessage> &params)
{
    return 0;
}

}

