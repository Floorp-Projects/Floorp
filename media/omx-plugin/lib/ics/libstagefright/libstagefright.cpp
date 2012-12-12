/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Types.h"
#define STAGEFRIGHT_EXPORT __attribute__ ((visibility ("default")))
#include "stagefright/ColorConverter.h"
#include "stagefright/DataSource.h"
#include "media/stagefright/MediaBuffer.h"
#include "stagefright/MediaExtractor.h"
#include "media/stagefright/MediaSource.h"
#include "stagefright/MetaData.h"
#include "media/stagefright/openmax/OMX_Types.h"
#include "media/stagefright/openmax/OMX_Index.h"
#include "media/stagefright/openmax/OMX_IVCommon.h"
#include "media/stagefright/openmax/OMX_Video.h"
#include "media/stagefright/openmax/OMX_Core.h"
#include "stagefright/OMXCodec.h"
#include "stagefright/OMXClient.h"

namespace android {
MOZ_EXPORT void
MediaBuffer::release()
{
}

MOZ_EXPORT size_t
MediaBuffer::range_offset() const
{
  return 0;
}

MOZ_EXPORT size_t
MediaBuffer::range_length() const
{
  return 0;
}

MOZ_EXPORT sp<MetaData>
MediaBuffer::meta_data()
{
  return 0;
}

MOZ_EXPORT void*
MediaBuffer::data() const
{
  return 0;
}

MOZ_EXPORT size_t
MediaBuffer::size() const
{
  return 0;
}

MOZ_EXPORT bool
MetaData::findInt32(uint32_t key, int32_t *value)
{
  return false;
}

MOZ_EXPORT bool
MetaData::findInt64(uint32_t key, int64_t *value)
{
  return false;
}

MOZ_EXPORT bool
MetaData::findPointer(uint32_t key, void **value)
{
  return false;
}

MOZ_EXPORT bool
MetaData::findCString(uint32_t key, const char **value)
{
  return false;
}

MOZ_EXPORT bool
MetaData::findRect(unsigned int key, int *cropLeft, int *cropTop,
                   int *cropRight, int *cropBottom)
{
  abort();
}

MOZ_EXPORT MediaSource::ReadOptions::ReadOptions()
{
}

MOZ_EXPORT void
MediaSource::ReadOptions::setSeekTo(int64_t time_us, SeekMode mode)
{
}

MOZ_EXPORT bool
DataSource::getUInt16(off64_t offset, uint16_t *x)
{
  return false;
}

MOZ_EXPORT status_t
DataSource::getSize(off64_t *size)
{
  return 0;
}

MOZ_EXPORT String8
DataSource::getMIMEType() const
{
  return String8();
}

MOZ_EXPORT void
DataSource::RegisterDefaultSniffers()
{
}

MOZ_EXPORT sp<MediaExtractor>
MediaExtractor::Create(const sp<DataSource> &source, const char *mime)
{
  return 0;
}

MOZ_EXPORT sp<MediaSource>
OMXCodec::Create(
            const sp<IOMX> &omx,
            const sp<MetaData> &meta, bool createEncoder,
            const sp<MediaSource> &source,
            const char *matchComponentName,
            uint32_t flags,
            const sp<ANativeWindow> &nativeWindow)
{
  return 0;
}

MOZ_EXPORT OMXClient::OMXClient()
{
}

MOZ_EXPORT status_t OMXClient::connect()
{
  return OK;
}

MOZ_EXPORT void OMXClient::disconnect()
{
}

class __attribute__ ((visibility ("default"))) UnknownDataSource : public DataSource {
public:
UnknownDataSource();

virtual status_t initCheck() const { return 0; }
virtual ssize_t readAt(off64_t offset, void *data, size_t size) { return 0; }
virtual status_t getSize(off64_t *size) { return 0; }

virtual ~UnknownDataSource() { }
};

UnknownDataSource foo;

MOZ_EXPORT UnknownDataSource::UnknownDataSource() { }

MOZ_EXPORT
ColorConverter::ColorConverter(OMX_COLOR_FORMATTYPE, OMX_COLOR_FORMATTYPE) { }

MOZ_EXPORT
ColorConverter::~ColorConverter() { }

MOZ_EXPORT bool
ColorConverter::isValid() const { return false; }

MOZ_EXPORT status_t
ColorConverter::convert(const void *srcBits,
                        size_t srcWidth, size_t srcHeight,
                        size_t srcCropLeft, size_t srcCropTop,
                        size_t srcCropRight, size_t srcCropBottom,
                        void *dstBits,
                        size_t dstWidth, size_t dstHeight,
                        size_t dstCropLeft, size_t dstCropTop,
                        size_t dstCropRight, size_t dstCropBottom)
{
  return 0;
}
}
