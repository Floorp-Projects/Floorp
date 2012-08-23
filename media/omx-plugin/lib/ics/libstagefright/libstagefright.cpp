/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Types.h"
#define STAGEFRIGHT_EXPORT __attribute__ ((visibility ("default")))
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
MOZ_EXPORT_API(void)
MediaBuffer::release()
{
}

MOZ_EXPORT_API(size_t)
MediaBuffer::range_offset() const
{
  return 0;
}

MOZ_EXPORT_API(size_t)
MediaBuffer::range_length() const
{
  return 0;
}

MOZ_EXPORT_API(sp<MetaData>)
MediaBuffer::meta_data()
{
  return 0;
}

MOZ_EXPORT_API(void*)
MediaBuffer::data() const
{
  return 0;
}

MOZ_EXPORT_API(size_t)
MediaBuffer::size() const
{
  return 0;
}

MOZ_EXPORT_API(bool)
MetaData::findInt32(uint32_t key, int32_t *value)
{
  return false;
}

MOZ_EXPORT_API(bool)
MetaData::findInt64(uint32_t key, int64_t *value)
{
  return false;
}

MOZ_EXPORT_API(bool)
MetaData::findPointer(uint32_t key, void **value)
{
  return false;
}

MOZ_EXPORT_API(bool)
MetaData::findCString(uint32_t key, const char **value)
{
  return false;
}
 
MOZ_EXPORT_API(MediaSource::ReadOptions)::ReadOptions()
{
}

MOZ_EXPORT_API(void)
MediaSource::ReadOptions::setSeekTo(int64_t time_us, SeekMode mode)
{
}

MOZ_EXPORT_API(bool)
DataSource::getUInt16(off64_t offset, uint16_t *x)
{
  return false;
}

MOZ_EXPORT_API(status_t)
DataSource::getSize(off64_t *size)
{
  return 0;
}

MOZ_EXPORT_API(String8)
DataSource::getMIMEType() const
{
  return String8();
}

MOZ_EXPORT_API(void)
DataSource::RegisterDefaultSniffers()
{
}

MOZ_EXPORT_API(sp<MediaExtractor>)
MediaExtractor::Create(const sp<DataSource> &source, const char *mime)
{
  return 0;
}

MOZ_EXPORT_API(sp<MediaSource>)
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

MOZ_EXPORT_API(OMXClient)::OMXClient()
{
}

MOZ_EXPORT_API(status_t) OMXClient::connect()
{
  return OK;
}

MOZ_EXPORT_API(void) OMXClient::disconnect()
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

MOZ_EXPORT_API(UnknownDataSource)::UnknownDataSource() { }
}
