/*
 * Copyright (C) 2009 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "MPEG4Extractor"
#include <utils/Log.h>

#include "include/MPEG4Extractor.h"
#include "include/SampleTable.h"
#include "include/ESDS.h"

#include <algorithm>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>

static const uint32_t kMAX_ALLOCATION =
    (SIZE_MAX < INT32_MAX ? SIZE_MAX : INT32_MAX) - 128;

namespace stagefright {

class MPEG4Source : public MediaSource {
public:
    MPEG4Source(const sp<MetaData> &format,
                uint32_t timeScale,
                const sp<SampleTable> &sampleTable);

    sp<MetaData> getFormat() override;

    nsTArray<Indice> exportIndex() override;

protected:
    virtual ~MPEG4Source();

private:
    sp<MetaData> mFormat;
    uint32_t mTimescale;
    sp<SampleTable> mSampleTable;

    MPEG4Source(const MPEG4Source &) = delete;
    MPEG4Source &operator=(const MPEG4Source &) = delete;
};

// This custom data source wraps an existing one and satisfies requests
// falling entirely within a cached range from the cache while forwarding
// all remaining requests to the wrapped datasource.
// This is used to cache the full sampletable metadata for a single track,
// possibly wrapping multiple times to cover all tracks, i.e.
// Each MPEG4DataSource caches the sampletable metadata for a single track.

struct MPEG4DataSource : public DataSource {
    MPEG4DataSource(const sp<DataSource> &source);

    status_t initCheck() const override;
    ssize_t readAt(off64_t offset, void *data, size_t size) override;
    status_t getSize(off64_t *size) override;
    uint32_t flags() override;

    status_t setCachedRange(off64_t offset, size_t size);

protected:
    virtual ~MPEG4DataSource();

private:
    Mutex mLock;

    sp<DataSource> mSource;
    off64_t mCachedOffset;
    size_t mCachedSize;
    uint8_t *mCache;

    void clearCache();

    MPEG4DataSource(const MPEG4DataSource &) = delete;
    MPEG4DataSource &operator=(const MPEG4DataSource &) = delete;
};

MPEG4DataSource::MPEG4DataSource(const sp<DataSource> &source)
    : mSource(source),
      mCachedOffset(0),
      mCachedSize(0),
      mCache(NULL) {
}

MPEG4DataSource::~MPEG4DataSource() {
    clearCache();
}

void MPEG4DataSource::clearCache() {
    if (mCache) {
        free(mCache);
        mCache = NULL;
    }

    mCachedOffset = 0;
    mCachedSize = 0;
}

status_t MPEG4DataSource::initCheck() const {
    return mSource->initCheck();
}

ssize_t MPEG4DataSource::readAt(off64_t offset, void *data, size_t size) {
    Mutex::Autolock autoLock(mLock);

    if (offset >= mCachedOffset
            && offset + size <= mCachedOffset + mCachedSize) {
        memcpy(data, &mCache[offset - mCachedOffset], size);
        return size;
    }

    return mSource->readAt(offset, data, size);
}

status_t MPEG4DataSource::getSize(off64_t *size) {
    return mSource->getSize(size);
}

uint32_t MPEG4DataSource::flags() {
    return mSource->flags();
}

status_t MPEG4DataSource::setCachedRange(off64_t offset, size_t size) {
    Mutex::Autolock autoLock(mLock);

    clearCache();

    mCache = (uint8_t *)malloc(size);

    if (mCache == NULL) {
        return -ENOMEM;
    }

    mCachedOffset = offset;
    mCachedSize = size;

    ssize_t err = mSource->readAt(mCachedOffset, mCache, mCachedSize);

    if (err < (ssize_t)size) {
        clearCache();

        return ERROR_IO;
    }

    return OK;
}

////////////////////////////////////////////////////////////////////////////////

static void hexdump(const void *_data, size_t size) {
    const uint8_t *data = (const uint8_t *)_data;
    size_t offset = 0;
    while (offset < size) {
        printf("0x%04x  ", offset);

        size_t n = size - offset;
        if (n > 16) {
            n = 16;
        }

        for (size_t i = 0; i < 16; ++i) {
            if (i == 8) {
                printf(" ");
            }

            if (offset + i < size) {
                printf("%02x ", data[offset + i]);
            } else {
                printf("   ");
            }
        }

        printf(" ");

        for (size_t i = 0; i < n; ++i) {
            if (isprint(data[offset + i])) {
                printf("%c", data[offset + i]);
            } else {
                printf(".");
            }
        }

        printf("\n");

        offset += 16;
    }
}

static const char *FourCC2MIME(uint32_t fourcc) {
    switch (fourcc) {
        case FOURCC('m', 'p', '4', 'a'):
            return MEDIA_MIMETYPE_AUDIO_AAC;

        case FOURCC('s', 'a', 'm', 'r'):
            return MEDIA_MIMETYPE_AUDIO_AMR_NB;

        case FOURCC('s', 'a', 'w', 'b'):
            return MEDIA_MIMETYPE_AUDIO_AMR_WB;

        case FOURCC('.', 'm', 'p', '3'):
            return MEDIA_MIMETYPE_AUDIO_MPEG;

        case FOURCC('m', 'p', '4', 'v'):
            return MEDIA_MIMETYPE_VIDEO_MPEG4;

        case FOURCC('s', '2', '6', '3'):
        case FOURCC('h', '2', '6', '3'):
        case FOURCC('H', '2', '6', '3'):
            return MEDIA_MIMETYPE_VIDEO_H263;

        case FOURCC('a', 'v', 'c', '1'):
        case FOURCC('a', 'v', 'c', '3'):
            return MEDIA_MIMETYPE_VIDEO_AVC;

        case FOURCC('V', 'P', '6', 'F'):
            return MEDIA_MIMETYPE_VIDEO_VP6;

        default:
            ALOGE("Unknown MIME type %08x", fourcc);
            return NULL;
    }
}

static bool AdjustChannelsAndRate(uint32_t fourcc, uint32_t *channels, uint32_t *rate) {
    const char* mime = FourCC2MIME(fourcc);
    if (!mime) {
        return false;
    }
    if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_AMR_NB, mime)) {
        // AMR NB audio is always mono, 8kHz
        *channels = 1;
        *rate = 8000;
        return true;
    } else if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_AMR_WB, mime)) {
        // AMR WB audio is always mono, 16kHz
        *channels = 1;
        *rate = 16000;
        return true;
    }
    return false;
}

MPEG4Extractor::MPEG4Extractor(const sp<DataSource> &source)
    : mSidxDuration(0),
      mDataSource(source),
      mInitCheck(NO_INIT),
      mHasVideo(false),
      mHeaderTimescale(0),
      mFirstTrack(NULL),
      mLastTrack(NULL),
      mFileMetaData(new MetaData),
      mFirstSINF(NULL),
      mIsDrm(false),
      mDrmScheme(0)
{
}

MPEG4Extractor::~MPEG4Extractor() {
    Track *track = mFirstTrack;
    while (track) {
        Track *next = track->next;

        delete track;
        track = next;
    }
    mFirstTrack = mLastTrack = NULL;

    SINF *sinf = mFirstSINF;
    while (sinf) {
        SINF *next = sinf->next;
        delete[] sinf->IPMPData;
        delete sinf;
        sinf = next;
    }
    mFirstSINF = NULL;

    for (size_t i = 0; i < mPssh.Length(); i++) {
        delete [] mPssh[i].data;
    }
}

uint32_t MPEG4Extractor::flags() const {
    return CAN_PAUSE | CAN_SEEK_BACKWARD | CAN_SEEK_FORWARD | CAN_SEEK;
}

sp<MetaData> MPEG4Extractor::getMetaData() {
    status_t err;
    if ((err = readMetaData()) != OK) {
        return NULL;
    }

    return mFileMetaData;
}

size_t MPEG4Extractor::countTracks() {
    status_t err;
    if ((err = readMetaData()) != OK) {
        ALOGV("MPEG4Extractor::countTracks: no tracks");
        return 0;
    }

    size_t n = 0;
    Track *track = mFirstTrack;
    while (track) {
        ++n;
        track = track->next;
    }

    ALOGV("MPEG4Extractor::countTracks: %d tracks", n);
    return n;
}

sp<MetaData> MPEG4Extractor::getTrackMetaData(
        size_t index, uint32_t flags) {
    status_t err;
    if ((err = readMetaData()) != OK) {
        return NULL;
    }

    Track *track = mFirstTrack;
    while (index > 0) {
        if (track == NULL) {
            return NULL;
        }

        track = track->next;
        --index;
    }

    if (track == NULL) {
        return NULL;
    }

    return track->meta;
}

static void MakeFourCCString(uint32_t x, char *s) {
    s[0] = x >> 24;
    s[1] = (x >> 16) & 0xff;
    s[2] = (x >> 8) & 0xff;
    s[3] = x & 0xff;
    s[4] = '\0';
}

status_t MPEG4Extractor::readMetaData() {
    if (mInitCheck != NO_INIT) {
        return mInitCheck;
    }

    off64_t offset = 0;
    status_t err = NO_INIT;
    while (!mFirstTrack) {
        err = parseChunk(&offset, 0);
        // The parseChunk function returns UNKNOWN_ERROR to skip
        // some boxes we don't want to handle. Filter that error
        // code but return others so e.g. I/O errors propagate.
        if (err != OK && err != (status_t) UNKNOWN_ERROR) {
          ALOGW("Error %d parsing chunck at offset %lld looking for first track",
              err, (long long)offset);
          break;
        }
    }

    if (mInitCheck == OK) {
        if (mHasVideo) {
            mFileMetaData->setCString(
                    kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_MPEG4);
        } else {
            mFileMetaData->setCString(kKeyMIMEType, "audio/mp4");
        }

        mInitCheck = OK;
    } else {
        mInitCheck = err;
    }

    CHECK_NE(err, (status_t)NO_INIT);

    // copy pssh data into file metadata
    uint64_t psshsize = 0;
    for (size_t i = 0; i < mPssh.Length(); i++) {
        psshsize += 20 + mPssh[i].datalen;
        if (mPssh[i].datalen > kMAX_ALLOCATION - 20 ||
            psshsize > kMAX_ALLOCATION) {
            return ERROR_MALFORMED;
        }
    }
    if (psshsize) {
        char *buf = (char*)malloc(psshsize);
        if (!buf) {
            return -ENOMEM;
        }
        char *ptr = buf;
        for (size_t i = 0; i < mPssh.Length(); i++) {
            memcpy(ptr, mPssh[i].uuid, 20); // uuid + length
            memcpy(ptr + 20, mPssh[i].data, mPssh[i].datalen);
            ptr += (20 + mPssh[i].datalen);
        }
        mFileMetaData->setData(kKeyPssh, 'pssh', buf, psshsize);
        free(buf);
    }
    return mInitCheck;
}

char* MPEG4Extractor::getDrmTrackInfo(size_t trackID, int *len) {
    if (mFirstSINF == NULL) {
        return NULL;
    }

    SINF *sinf = mFirstSINF;
    while (sinf && (trackID != sinf->trackID)) {
        sinf = sinf->next;
    }

    if (sinf == NULL) {
        return NULL;
    }

    *len = sinf->len;
    return sinf->IPMPData;
}

// Reads an encoded integer 7 bits at a time until it encounters the high bit clear.
static int32_t readSize(off64_t offset,
        const sp<DataSource> DataSource, uint8_t *numOfBytes) {
    uint32_t size = 0;
    uint8_t data;
    bool moreData = true;
    *numOfBytes = 0;

    while (moreData) {
        if (DataSource->readAt(offset, &data, 1) < 1) {
            return -1;
        }
        offset ++;
        moreData = (data >= 128) ? true : false;
        size = (size << 7) | (data & 0x7f); // Take last 7 bits
        (*numOfBytes) ++;
    }

    return size;
}

status_t MPEG4Extractor::parseDrmSINF(off64_t *offset, off64_t data_offset) {
    uint8_t updateIdTag;
    if (mDataSource->readAt(data_offset, &updateIdTag, 1) < 1) {
        return ERROR_IO;
    }
    data_offset ++;

    if (0x01/*OBJECT_DESCRIPTOR_UPDATE_ID_TAG*/ != updateIdTag) {
        return ERROR_MALFORMED;
    }

    uint8_t numOfBytes;
    int32_t size = readSize(data_offset, mDataSource, &numOfBytes);
    if (size < 0) {
        return ERROR_IO;
    }
    int32_t classSize = size;
    data_offset += numOfBytes;

    while(size >= 11 ) {
        uint8_t descriptorTag;
        if (mDataSource->readAt(data_offset, &descriptorTag, 1) < 1) {
            return ERROR_IO;
        }
        data_offset ++;

        if (0x11/*OBJECT_DESCRIPTOR_ID_TAG*/ != descriptorTag) {
            return ERROR_MALFORMED;
        }

        uint8_t buffer[8];
        //ObjectDescriptorID and ObjectDescriptor url flag
        if (mDataSource->readAt(data_offset, buffer, 2) < 2) {
            return ERROR_IO;
        }
        data_offset += 2;

        if ((buffer[1] >> 5) & 0x0001) { //url flag is set
            return ERROR_MALFORMED;
        }

        if (mDataSource->readAt(data_offset, buffer, 8) < 8) {
            return ERROR_IO;
        }
        data_offset += 8;

        if ((0x0F/*ES_ID_REF_TAG*/ != buffer[1])
                || ( 0x0A/*IPMP_DESCRIPTOR_POINTER_ID_TAG*/ != buffer[5])) {
            return ERROR_MALFORMED;
        }

        SINF *sinf = new SINF;
        sinf->trackID = U16_AT(&buffer[3]);
        sinf->IPMPDescriptorID = buffer[7];
        sinf->next = mFirstSINF;
        mFirstSINF = sinf;

        size -= (8 + 2 + 1);
    }

    if (size != 0) {
        return ERROR_MALFORMED;
    }

    if (mDataSource->readAt(data_offset, &updateIdTag, 1) < 1) {
        return ERROR_IO;
    }
    data_offset ++;

    if(0x05/*IPMP_DESCRIPTOR_UPDATE_ID_TAG*/ != updateIdTag) {
        return ERROR_MALFORMED;
    }

    size = readSize(data_offset, mDataSource, &numOfBytes);
    if (size < 0) {
        return ERROR_IO;
    }
    classSize = size;
    data_offset += numOfBytes;

    while (size > 0) {
        uint8_t tag;
        int32_t dataLen;
        if (mDataSource->readAt(data_offset, &tag, 1) < 1) {
            return ERROR_IO;
        }
        data_offset ++;

        if (0x0B/*IPMP_DESCRIPTOR_ID_TAG*/ == tag) {
            uint8_t id;
            dataLen = readSize(data_offset, mDataSource, &numOfBytes);
            if (dataLen < 0) {
                return ERROR_IO;
            } else if (dataLen < 4) {
                return ERROR_MALFORMED;
            }
            data_offset += numOfBytes;

            if (mDataSource->readAt(data_offset, &id, 1) < 1) {
                return ERROR_IO;
            }
            data_offset ++;

            SINF *sinf = mFirstSINF;
            while (sinf && (sinf->IPMPDescriptorID != id)) {
                sinf = sinf->next;
            }
            if (sinf == NULL) {
                return ERROR_MALFORMED;
            }
            sinf->len = dataLen - 3;
            sinf->IPMPData = new (fallible) char[sinf->len];
            if (!sinf->IPMPData) {
                return -ENOMEM;
            }

            if (mDataSource->readAt(data_offset + 2, sinf->IPMPData, sinf->len) < sinf->len) {
                return ERROR_IO;
            }
            data_offset += sinf->len;

            size -= (dataLen + numOfBytes + 1);
        }
    }

    if (size != 0) {
        return ERROR_MALFORMED;
    }

    return UNKNOWN_ERROR;  // Return a dummy error.
}

struct PathAdder {
    PathAdder(nsTArray<uint32_t> *path, uint32_t chunkType)
        : mPath(path) {
        mPath->AppendElement(chunkType);
    }

    ~PathAdder() {
        mPath->RemoveElementAt(mPath->Length() - 1);
    }

private:
    nsTArray<uint32_t> *mPath;

    PathAdder(const PathAdder &);
    PathAdder &operator=(const PathAdder &);
};

static bool underMetaDataPath(const nsTArray<uint32_t> &path) {
    return path.Length() >= 5
        && path[0] == FOURCC('m', 'o', 'o', 'v')
        && path[1] == FOURCC('u', 'd', 't', 'a')
        && path[2] == FOURCC('m', 'e', 't', 'a')
        && path[3] == FOURCC('i', 'l', 's', 't');
}

static bool ValidInputSize(int32_t size) {
  // Reject compressed samples larger than an uncompressed UHD
  // frame. This is a reasonable cut-off for a lossy codec,
  // combined with the current Firefox limit to 5k video.
  return (size > 0 && size <= 4 * (1920 * 1080) * 3 / 2);
}

status_t MPEG4Extractor::parseChunk(off64_t *offset, int depth) {
    ALOGV("entering parseChunk %lld/%d", *offset, depth);
    uint32_t hdr[2];
    if (mDataSource->readAt(*offset, hdr, 4) < 4) {
        return ERROR_IO;
    }
    if (!hdr[0]) {
        *offset += 4;
        return OK;
    }
    if (mDataSource->readAt(*offset + 4, hdr + 1, 4) < 4) {
        return ERROR_IO;
    }
    uint64_t chunk_size = ntohl(hdr[0]);
    uint32_t chunk_type = ntohl(hdr[1]);
    off64_t data_offset = *offset + 8;

    if (chunk_size == 1) {
        if (mDataSource->readAt(*offset + 8, &chunk_size, 8) < 8) {
            return ERROR_IO;
        }
        chunk_size = ntoh64(chunk_size);
        data_offset += 8;

        if (chunk_size < 16) {
            // The smallest valid chunk is 16 bytes long in this case.
            return ERROR_MALFORMED;
        }
    } else if (chunk_size < 8) {
        // The smallest valid chunk is 8 bytes long.
        return ERROR_MALFORMED;
    }

    if (chunk_size >= kMAX_ALLOCATION) {
        // Could cause an overflow later. Abort.
        return ERROR_MALFORMED;
    }

    char chunk[5];
    MakeFourCCString(chunk_type, chunk);
    ALOGV("chunk: %s @ %lld, %d", chunk, *offset, depth);

#if 0
    static const char kWhitespace[] = "                                        ";
    const char *indent = &kWhitespace[sizeof(kWhitespace) - 1 - 2 * depth];
    printf("%sfound chunk '%s' of size %lld\n", indent, chunk, chunk_size);

    char buffer[256];
    size_t n = chunk_size;
    if (n > sizeof(buffer)) {
        n = sizeof(buffer);
    }
    if (mDataSource->readAt(*offset, buffer, n)
            < (ssize_t)n) {
        return ERROR_IO;
    }

    hexdump(buffer, n);
#endif

    PathAdder autoAdder(&mPath, chunk_type);

    off64_t chunk_data_size = *offset + chunk_size - data_offset;

    if (chunk_type != FOURCC('c', 'p', 'r', 't')
            && chunk_type != FOURCC('c', 'o', 'v', 'r')
            && mPath.Length() == 5 && underMetaDataPath(mPath)) {
        off64_t stop_offset = *offset + chunk_size;
        *offset = data_offset;
        while (*offset < stop_offset) {
            status_t err = parseChunk(offset, depth + 1);
            if (err != OK) {
                return err;
            }
        }

        if (*offset != stop_offset) {
            return ERROR_MALFORMED;
        }

        return OK;
    }

    switch(chunk_type) {
        case FOURCC('m', 'o', 'o', 'v'):
        case FOURCC('t', 'r', 'a', 'k'):
        case FOURCC('m', 'd', 'i', 'a'):
        case FOURCC('m', 'i', 'n', 'f'):
        case FOURCC('d', 'i', 'n', 'f'):
        case FOURCC('s', 't', 'b', 'l'):
        case FOURCC('m', 'v', 'e', 'x'):
        case FOURCC('m', 'o', 'o', 'f'):
        case FOURCC('t', 'r', 'a', 'f'):
        case FOURCC('m', 'f', 'r', 'a'):
        case FOURCC('u', 'd', 't', 'a'):
        case FOURCC('i', 'l', 's', 't'):
        case FOURCC('s', 'i', 'n', 'f'):
        case FOURCC('s', 'c', 'h', 'i'):
        case FOURCC('e', 'd', 't', 's'):
        {
            if (chunk_type == FOURCC('s', 't', 'b', 'l')) {
                ALOGV("sampleTable chunk is %d bytes long.", (size_t)chunk_size);

                if (mDataSource->flags()
                        & (DataSource::kWantsPrefetching
                            | DataSource::kIsCachingDataSource)) {
                    sp<MPEG4DataSource> cachedSource =
                        new MPEG4DataSource(mDataSource);

                    if (cachedSource->setCachedRange(*offset, chunk_size) == OK) {
                        mDataSource = cachedSource;
                    }
                }

                if (!mLastTrack) {
                  return ERROR_MALFORMED;
                }
                mLastTrack->sampleTable = new SampleTable(mDataSource);
            }

            bool isTrack = false;
            if (chunk_type == FOURCC('t', 'r', 'a', 'k')) {
                isTrack = true;

                Track *track = new Track;
                track->next = NULL;
                if (mLastTrack) {
                    mLastTrack->next = track;
                } else {
                    mFirstTrack = track;
                }
                mLastTrack = track;

                track->meta = new MetaData;
                track->includes_expensive_metadata = false;
                track->skipTrack = false;
                track->timescale = 0;
                track->empty_duration = 0;
                track->segment_duration = 0;
                track->media_time = 0;
                track->meta->setCString(kKeyMIMEType, "application/octet-stream");
            }

            off64_t stop_offset = *offset + chunk_size;
            *offset = data_offset;
            while (*offset < stop_offset) {
                status_t err = parseChunk(offset, depth + 1);
                if (err != OK) {
                    return err;
                }
            }

            if (*offset != stop_offset) {
                return ERROR_MALFORMED;
            }

            if (isTrack) {
                if (mLastTrack->skipTrack) {
                    Track *cur = mFirstTrack;

                    if (cur == mLastTrack) {
                        delete cur;
                        mFirstTrack = mLastTrack = NULL;
                    } else {
                        while (cur && cur->next != mLastTrack) {
                            cur = cur->next;
                        }
                        cur->next = NULL;
                        delete mLastTrack;
                        mLastTrack = cur;
                    }

                    return OK;
                }

                status_t err = verifyTrack(mLastTrack);

                if (err != OK) {
                    return err;
                }
            } else if (chunk_type == FOURCC('m', 'o', 'o', 'v')) {
                mInitCheck = OK;

                if (!mIsDrm) {
                    return UNKNOWN_ERROR;  // Return a dummy error.
                } else {
                    return OK;
                }
            }
            break;
        }

        case FOURCC('e', 'l', 's', 't'):
        {
            // See 14496-12 8.6.6
            uint8_t version;
            if (mDataSource->readAt(data_offset, &version, 1) < 1) {
                return ERROR_IO;
            }

            uint32_t entry_count;
            if (!mDataSource->getUInt32(data_offset + 4, &entry_count)) {
                return ERROR_IO;
            }

            off64_t entriesoffset = data_offset + 8;
            bool nonEmptyCount = false;
            for (uint32_t i = 0; i < entry_count; i++) {
                if (mHeaderTimescale == 0) {
                    ALOGW("ignoring edit list because timescale is 0");
                    break;
                }
                if (entriesoffset - data_offset > chunk_size) {
                    ALOGW("invalid edit list size");
                    break;
                }
                uint64_t segment_duration;
                int64_t media_time;
                if (version == 1) {
                    if (!mDataSource->getUInt64(entriesoffset, &segment_duration) ||
                        !mDataSource->getUInt64(entriesoffset + 8, (uint64_t*)&media_time)) {
                        return ERROR_IO;
                    }
                    entriesoffset += 16;
                } else if (version == 0) {
                    uint32_t sd;
                    int32_t mt;
                    if (!mDataSource->getUInt32(entriesoffset, &sd) ||
                        !mDataSource->getUInt32(entriesoffset + 4, (uint32_t*)&mt)) {
                        return ERROR_IO;
                    }
                    entriesoffset += 8;
                    segment_duration = sd;
                    media_time = mt;
                } else {
                    return ERROR_IO;
                }
                entriesoffset += 4; // ignore media_rate_integer and media_rate_fraction.
                if (media_time == -1 && i) {
                    ALOGW("ignoring invalid empty edit", i);
                    break;
                } else if (media_time == -1) {
                    // Starting offsets for tracks (streams) are represented by an initial empty edit.
                    if (!mLastTrack) {
                      return ERROR_MALFORMED;
                    }
                    mLastTrack->empty_duration = segment_duration;
                    continue;
                } else if (nonEmptyCount) {
                    // we only support a single non-empty entry at the moment, for gapless playback
                    ALOGW("multiple edit list entries, A/V sync will be wrong");
                    break;
                } else {
                    nonEmptyCount = true;
                }

                if (!mLastTrack) {
                  return ERROR_MALFORMED;
                }
                mLastTrack->segment_duration = segment_duration;
                mLastTrack->media_time = media_time;
            }
            storeEditList();
            *offset += chunk_size;
            break;
        }

        case FOURCC('f', 'r', 'm', 'a'):
        {
            uint32_t original_fourcc;
            if (mDataSource->readAt(data_offset, &original_fourcc, 4) < 4) {
                return ERROR_IO;
            }
            original_fourcc = ntohl(original_fourcc);
            ALOGV("read original format: %d", original_fourcc);
            if (!mLastTrack) {
                return ERROR_MALFORMED;
            }
            const char* mime = FourCC2MIME(original_fourcc);
            if (!mime) {
                return ERROR_UNSUPPORTED;
            }
            mLastTrack->meta->setCString(kKeyMIMEType, mime);
            uint32_t num_channels = 0;
            uint32_t sample_rate = 0;
            if (AdjustChannelsAndRate(original_fourcc, &num_channels, &sample_rate)) {
                mLastTrack->meta->setInt32(kKeyChannelCount, num_channels);
                mLastTrack->meta->setInt32(kKeySampleRate, sample_rate);
            }
            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 'c', 'h', 'm'):
        {
            if (!mDataSource->getUInt32(data_offset, &mDrmScheme)) {
                return ERROR_IO;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('t', 'e', 'n', 'c'):
        {
            if (chunk_size < 32) {
                return ERROR_MALFORMED;
            }

            // tenc box contains 1 byte version, 3 byte flags, 3 byte default algorithm id, one byte
            // default IV size, 16 bytes default KeyID
            // (ISO 23001-7)
            char buf[4];
            memset(buf, 0, 4);
            if (mDataSource->readAt(data_offset + 4, buf + 1, 3) < 3) {
                return ERROR_IO;
            }
            uint32_t defaultAlgorithmId = ntohl(*((int32_t*)buf));
            if (defaultAlgorithmId > 1) {
                // only 0 (clear) and 1 (AES-128) are valid
                return ERROR_MALFORMED;
            }

            memset(buf, 0, 4);
            if (mDataSource->readAt(data_offset + 7, buf + 3, 1) < 1) {
                return ERROR_IO;
            }
            uint32_t defaultIVSize = ntohl(*((int32_t*)buf));

            if ((defaultAlgorithmId == 0 && defaultIVSize != 0) ||
                    (defaultAlgorithmId != 0 && defaultIVSize == 0)) {
                // only unencrypted data must have 0 IV size
                return ERROR_MALFORMED;
            } else if (defaultIVSize != 0 &&
                    defaultIVSize != 8 &&
                    defaultIVSize != 16) {
                // only supported sizes are 0, 8 and 16
                return ERROR_MALFORMED;
            }

            uint8_t defaultKeyId[16];

            if (mDataSource->readAt(data_offset + 8, &defaultKeyId, 16) < 16) {
                return ERROR_IO;
            }

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            mLastTrack->meta->setInt32(kKeyCryptoMode, defaultAlgorithmId);
            mLastTrack->meta->setInt32(kKeyCryptoDefaultIVSize, defaultIVSize);
            mLastTrack->meta->setData(kKeyCryptoKey, 'tenc', defaultKeyId, 16);
            *offset += chunk_size;
            break;
        }

        case FOURCC('t', 'k', 'h', 'd'):
        {
            status_t err;
            if ((err = parseTrackHeader(data_offset, chunk_data_size)) != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('p', 's', 's', 'h'):
        {
            PsshInfo pssh;

            // We need the contents of the box header before data_offset. Make
            // sure we don't underflow somehow.
            CHECK(data_offset >= 8);

            uint32_t version = 0;
            if (mDataSource->readAt(data_offset, &version, 4) < 4) {
                return ERROR_IO;
            }

            if (mDataSource->readAt(data_offset + 4, &pssh.uuid, 16) < 16) {
                return ERROR_IO;
            }

            // Copy the contents of the box (including header) verbatim.
            pssh.datalen = chunk_data_size + 8;
            pssh.data = new (fallible) uint8_t[pssh.datalen];
            if (!pssh.data) {
                return -ENOMEM;
            }
            if (mDataSource->readAt(data_offset - 8, pssh.data, pssh.datalen) < pssh.datalen) {
                return ERROR_IO;
            }

            mPssh.AppendElement(pssh);

            *offset += chunk_size;
            break;
        }

        case FOURCC('m', 'd', 'h', 'd'):
        {
            if (chunk_data_size < 4) {
                return ERROR_MALFORMED;
            }

            uint8_t version;
            if (mDataSource->readAt(
                        data_offset, &version, sizeof(version))
                    < (ssize_t)sizeof(version)) {
                return ERROR_IO;
            }

            off64_t timescale_offset;

            if (version == 1) {
                timescale_offset = data_offset + 4 + 16;
            } else if (version == 0) {
                timescale_offset = data_offset + 4 + 8;
            } else {
                return ERROR_IO;
            }

            uint32_t timescale;
            if (mDataSource->readAt(
                        timescale_offset, &timescale, sizeof(timescale))
                    < (ssize_t)sizeof(timescale)) {
                return ERROR_IO;
            }

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            mLastTrack->timescale = ntohl(timescale);

            // Now that we've parsed the media timescale, we can interpret
            // the edit list data.
            storeEditList();

            int64_t duration = 0;
            if (version == 1) {
                if (mDataSource->readAt(
                            timescale_offset + 4, &duration, sizeof(duration))
                        < (ssize_t)sizeof(duration)) {
                    return ERROR_IO;
                }
                duration = ntoh64(duration);
            } else {
                uint32_t duration32;
                if (mDataSource->readAt(
                            timescale_offset + 4, &duration32, sizeof(duration32))
                        < (ssize_t)sizeof(duration32)) {
                    return ERROR_IO;
                }
                // ffmpeg sets duration to -1, which is incorrect.
                if (duration32 != 0xffffffff) {
                    duration = ntohl(duration32);
                }
            }
            if (!mLastTrack->timescale) {
                return ERROR_MALFORMED;
            }
            mLastTrack->meta->setInt64(
                    kKeyDuration, (duration * 1000000) / mLastTrack->timescale);

            uint8_t lang[2];
            off64_t lang_offset;
            if (version == 1) {
                lang_offset = timescale_offset + 4 + 8;
            } else if (version == 0) {
                lang_offset = timescale_offset + 4 + 4;
            } else {
                return ERROR_IO;
            }

            if (mDataSource->readAt(lang_offset, &lang, sizeof(lang))
                    < (ssize_t)sizeof(lang)) {
                return ERROR_IO;
            }

            // To get the ISO-639-2/T three character language code
            // 1 bit pad followed by 3 5-bits characters. Each character
            // is packed as the difference between its ASCII value and 0x60.
            char lang_code[4];
            lang_code[0] = ((lang[0] >> 2) & 0x1f) + 0x60;
            lang_code[1] = ((lang[0] & 0x3) << 3 | (lang[1] >> 5)) + 0x60;
            lang_code[2] = (lang[1] & 0x1f) + 0x60;
            lang_code[3] = '\0';

            mLastTrack->meta->setCString(
                    kKeyMediaLanguage, lang_code);

            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 't', 's', 'd'):
        {
            if (chunk_data_size < 8) {
                return ERROR_MALFORMED;
            }

            uint8_t buffer[8];
            if (chunk_data_size < (off64_t)sizeof(buffer)) {
                return ERROR_MALFORMED;
            }

            if (mDataSource->readAt(
                        data_offset, buffer, 8) < 8) {
                return ERROR_IO;
            }

            if (U32_AT(buffer) != 0) {
                // Should be version 0, flags 0.
                return ERROR_MALFORMED;
            }

            uint32_t entry_count = U32_AT(&buffer[4]);

            if (entry_count > 1) {
                // For 3GPP timed text, there could be multiple tx3g boxes contain
                // multiple text display formats. These formats will be used to
                // display the timed text.
                // For encrypted files, there may also be more than one entry.
                const char *mime;
                if (!mLastTrack) {
                  return ERROR_MALFORMED;
                }
                CHECK(mLastTrack->meta->findCString(kKeyMIMEType, &mime));
                if (strcasecmp(mime, MEDIA_MIMETYPE_TEXT_3GPP) &&
                        strcasecmp(mime, "application/octet-stream")) {
                    // For now we only support a single type of media per track.
                    mLastTrack->skipTrack = true;
                    *offset += chunk_size;
                    break;
                }
            }
            off64_t stop_offset = *offset + chunk_size;
            *offset = data_offset + 8;
            for (uint32_t i = 0; i < entry_count; ++i) {
                status_t err = parseChunk(offset, depth + 1);
                if (err != OK) {
                    return err;
                }
            }

            // Some muxers add some padding after the stsd content. Skip it.
            *offset = stop_offset;
            break;
        }

        case FOURCC('m', 'p', '4', 'a'):
        case FOURCC('.', 'm', 'p', '3'):
        case FOURCC('e', 'n', 'c', 'a'):
        case FOURCC('s', 'a', 'm', 'r'):
        case FOURCC('s', 'a', 'w', 'b'):
        {
            // QT's MP4 may have an empty MP4A atom within a MP4A atom.
            // Ignore it.
            if (chunk_data_size == 4) {
                *offset += chunk_size;
                break;
            }
            uint8_t buffer[8 + 20];
            if (chunk_data_size < (ssize_t)sizeof(buffer)) {
                // Basic AudioSampleEntry size.
                return ERROR_MALFORMED;
            }

            if (mDataSource->readAt(
                        data_offset, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer)) {
                return ERROR_IO;
            }

            uint16_t data_ref_index = U16_AT(&buffer[6]);
            uint16_t qt_version = U16_AT(&buffer[8]);
            uint32_t num_channels = U16_AT(&buffer[16]);

            uint16_t sample_size = U16_AT(&buffer[18]);
            uint32_t sample_rate = U32_AT(&buffer[24]) >> 16;

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            if (chunk_type != FOURCC('e', 'n', 'c', 'a')) {
                // if the chunk type is enca, we'll get the type from the sinf/frma box later
                const char* mime = FourCC2MIME(chunk_type);
                if (!mime) {
                    return ERROR_UNSUPPORTED;
                }
                mLastTrack->meta->setCString(kKeyMIMEType, mime);
                AdjustChannelsAndRate(chunk_type, &num_channels, &sample_rate);
            }

            uint64_t skip = 0;
            if (qt_version == 1) {
                // Skip QTv1 extension
                // uint32_t SamplesPerPacket
                // uint32_t BytesPerPacket
                // uint32_t BytesPerFrame
                // uint32_t BytesPerSample
                skip = 16;
            } else if (qt_version == 2) {
                // Skip QTv2 extension
                // uint32_t Qt V2 StructSize
                // double SampleRate
                // uint32_t ChannelCount
                // uint32_t Reserved
                // uint32_t BitsPerChannel
                // uint32_t LPCMFormatSpecificFlags
                // uint32_t BytesPerAudioPacket
                // uint32_t LPCMFramesPerAudioPacket
                // if (Qt V2 StructSize > 72) {
                //     StructSize-72: Qt V2 extension
                // }
                uint32_t structSize32;
                if (mDataSource->readAt(
                            data_offset + 28, &structSize32, sizeof(structSize32))
                        < (ssize_t)sizeof(structSize32)) {
                    return ERROR_IO;
                }
                uint32_t structSize = ntohl(structSize32);
                // Read SampleRate.
                uint64_t sample_rate64;
                if (mDataSource->readAt(
                            data_offset + 32, &sample_rate64, sizeof(sample_rate64))
                        < (ssize_t)sizeof(sample_rate64)) {
                    return ERROR_IO;
                }
                uint64_t i_value = ntoh64(sample_rate64);
                void* v_value = reinterpret_cast<void*>(&i_value);
                sample_rate = uint32_t(*reinterpret_cast<double*>(v_value));
                // Read ChannelCount.
                uint32_t channel_count32;
                if (mDataSource->readAt(
                            data_offset + 40, &channel_count32, sizeof(channel_count32))
                        < (ssize_t)sizeof(channel_count32)) {
                    return ERROR_IO;
                }
                num_channels = ntohl(channel_count32);

                skip += 36;
                if (structSize > 72) {
                    skip += structSize - 72;
                }
            }
            ALOGV("*** coding='%s' %d channels, size %d, rate %d\n",
                   chunk, num_channels, sample_size, sample_rate);
            mLastTrack->meta->setInt32(kKeyChannelCount, num_channels);
            mLastTrack->meta->setInt32(kKeySampleSize, sample_size);
            mLastTrack->meta->setInt32(kKeySampleRate, sample_rate);

            off64_t stop_offset = *offset + chunk_size;
            *offset = data_offset + sizeof(buffer) + skip;
            while (*offset < stop_offset) {
                status_t err = parseChunk(offset, depth + 1);
                if (err != OK) {
                    return err;
                }
            }

            if (*offset != stop_offset) {
                return ERROR_MALFORMED;
            }
            break;
        }

        case FOURCC('m', 'p', '4', 'v'):
        case FOURCC('e', 'n', 'c', 'v'):
        case FOURCC('s', '2', '6', '3'):
        case FOURCC('H', '2', '6', '3'):
        case FOURCC('h', '2', '6', '3'):
        case FOURCC('a', 'v', 'c', '1'):
        case FOURCC('a', 'v', 'c', '3'):
        case FOURCC('V', 'P', '6', 'F'):
        {
            mHasVideo = true;

            uint8_t buffer[78];
            if (chunk_data_size < (ssize_t)sizeof(buffer)) {
                // Basic VideoSampleEntry size.
                return ERROR_MALFORMED;
            }

            if (mDataSource->readAt(
                        data_offset, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer)) {
                return ERROR_IO;
            }

            uint16_t data_ref_index = U16_AT(&buffer[6]);
            uint16_t width = U16_AT(&buffer[6 + 18]);
            uint16_t height = U16_AT(&buffer[6 + 20]);

            // The video sample is not standard-compliant if it has invalid dimension.
            // Use some default width and height value, and
            // let the decoder figure out the actual width and height (and thus
            // be prepared for INFO_FOMRAT_CHANGED event).
            if (width == 0)  width  = 352;
            if (height == 0) height = 288;

            // printf("*** coding='%s' width=%d height=%d\n",
            //        chunk, width, height);

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            if (chunk_type != FOURCC('e', 'n', 'c', 'v')) {
                // if the chunk type is encv, we'll get the type from the sinf/frma box later
                const char* mime = FourCC2MIME(chunk_type);
                if (!mime) {
                    return ERROR_UNSUPPORTED;
                }
                mLastTrack->meta->setCString(kKeyMIMEType, mime);
            }
            mLastTrack->meta->setInt32(kKeyWidth, width);
            mLastTrack->meta->setInt32(kKeyHeight, height);

            off64_t stop_offset = *offset + chunk_size;
            *offset = data_offset + sizeof(buffer);
            while (*offset < stop_offset) {
                status_t err = parseChunk(offset, depth + 1);
                if (err != OK) {
                    return err;
                }
                // Some Apple QuickTime muxed videos appear to have some padding.
                // Ignore it and assume we've reached the end.
                if (stop_offset - *offset < 8) {
                    *offset = stop_offset;
                }
            }

            if (*offset != stop_offset) {
                return ERROR_MALFORMED;
            }
            break;
        }

        case FOURCC('s', 't', 'c', 'o'):
        case FOURCC('c', 'o', '6', '4'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setChunkOffsetParams(
                        chunk_type, data_offset, chunk_data_size);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 't', 's', 'c'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setSampleToChunkParams(
                        data_offset, chunk_data_size);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 't', 's', 'z'):
        case FOURCC('s', 't', 'z', '2'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setSampleSizeParams(
                        chunk_type, data_offset, chunk_data_size);

            if (err != OK) {
                return err;
            }

            size_t max_size;
            err = mLastTrack->sampleTable->getMaxSampleSize(&max_size);

            if (err != OK) {
                return err;
            }

            if (max_size != 0) {
                // Assume that a given buffer only contains at most 10 chunks,
                // each chunk originally prefixed with a 2 byte length will
                // have a 4 byte header (0x00 0x00 0x00 0x01) after conversion,
                // and thus will grow by 2 bytes per chunk.
                mLastTrack->meta->setInt32(kKeyMaxInputSize, max_size + 10 * 2);
            } else {
                // No size was specified. Pick a conservatively large size.
                int32_t width, height;
                if (mLastTrack->meta->findInt32(kKeyWidth, &width) &&
                        mLastTrack->meta->findInt32(kKeyHeight, &height)) {
                    mLastTrack->meta->setInt32(kKeyMaxInputSize, width * height * 3 / 2);
                } else {
                    ALOGV("No width or height, assuming worst case 1080p");
                    mLastTrack->meta->setInt32(kKeyMaxInputSize, 3110400);
                }
            }
            *offset += chunk_size;

            // Calculate average frame rate.
            const char *mime;
            CHECK(mLastTrack->meta->findCString(kKeyMIMEType, &mime));
            if (!strncasecmp("video/", mime, 6)) {
                size_t nSamples = mLastTrack->sampleTable->countSamples();
                int64_t durationUs;
                if (mLastTrack->meta->findInt64(kKeyDuration, &durationUs)) {
                    if (durationUs > 0) {
                        int32_t frameRate = (nSamples * 1000000LL +
                                    (durationUs >> 1)) / durationUs;
                        mLastTrack->meta->setInt32(kKeyFrameRate, frameRate);
                    }
                }
            }

            break;
        }

        case FOURCC('s', 't', 't', 's'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setTimeToSampleParams(
                        data_offset, chunk_data_size);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('c', 't', 't', 's'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setCompositionTimeToSampleParams(
                        data_offset, chunk_data_size);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 't', 's', 's'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setSyncSampleParams(
                        data_offset, chunk_data_size);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 'a', 'i', 'z'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setSampleAuxiliaryInformationSizeParams(
                        data_offset, chunk_data_size, mDrmScheme);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 'a', 'i', 'o'):
        {
            if (!mLastTrack || !mLastTrack->sampleTable.get()) {
              return ERROR_MALFORMED;
            }
            status_t err =
                mLastTrack->sampleTable->setSampleAuxiliaryInformationOffsetParams(
                        data_offset, chunk_data_size, mDrmScheme);

            if (err != OK) {
                return err;
            }

            *offset += chunk_size;
            break;
        }

        // @xyz
        case FOURCC('\xA9', 'x', 'y', 'z'):
        {
            // Best case the total data length inside "@xyz" box
            // would be 8, for instance "@xyz" + "\x00\x04\x15\xc7" + "0+0/",
            // where "\x00\x04" is the text string length with value = 4,
            // "\0x15\xc7" is the language code = en, and "0+0" is a
            // location (string) value with longitude = 0 and latitude = 0.
            if (chunk_data_size < 8) {
                return ERROR_MALFORMED;
            }

            // Worst case the location string length would be 18,
            // for instance +90.0000-180.0000, without the trailing "/" and
            // the string length + language code.
            char buffer[18];

            // Substracting 5 from the data size is because the text string length +
            // language code takes 4 bytes, and the trailing slash "/" takes 1 byte.
            off64_t location_length = chunk_data_size - 5;
            if (location_length >= (off64_t) sizeof(buffer)) {
                return ERROR_MALFORMED;
            }

            if (mDataSource->readAt(
                        data_offset + 4, buffer, location_length) < location_length) {
                return ERROR_IO;
            }

            buffer[location_length] = '\0';
            mFileMetaData->setCString(kKeyLocation, buffer);
            *offset += chunk_size;
            break;
        }

        case FOURCC('e', 's', 'd', 's'):
        {
            if (chunk_data_size < 4) {
                return ERROR_MALFORMED;
            }

            uint8_t buffer[256];
            if (chunk_data_size > (off64_t)sizeof(buffer)) {
                return ERROR_BUFFER_TOO_SMALL;
            }

            if (mDataSource->readAt(
                        data_offset, buffer, chunk_data_size) < chunk_data_size) {
                return ERROR_IO;
            }

            if (U32_AT(buffer) != 0) {
                // Should be version 0, flags 0.
                return ERROR_MALFORMED;
            }

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            mLastTrack->meta->setData(
                    kKeyESDS, kTypeESDS, &buffer[4], chunk_data_size - 4);

            if (mPath.Length() >= 2
                    && (mPath[mPath.Length() - 2] == FOURCC('m', 'p', '4', 'a') ||
                       (mPath[mPath.Length() - 2] == FOURCC('e', 'n', 'c', 'a')))) {
                // Information from the ESDS must be relied on for proper
                // setup of sample rate and channel count for MPEG4 Audio.
                // The generic header appears to only contain generic
                // information...

                status_t err = updateAudioTrackInfoFromESDS_MPEG4Audio(
                        &buffer[4], chunk_data_size - 4);

                if (err != OK) {
                    return err;
                }
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('a', 'v', 'c', 'C'):
        {
            if (chunk_data_size < 7) {
              ALOGE("short avcC chunk (%d bytes)", chunk_data_size);
              return ERROR_MALFORMED;
            }

            sp<ABuffer> buffer = new (fallible) ABuffer(chunk_data_size);
            if (!buffer.get() || !buffer->data()) {
                return -ENOMEM;
            }

            if (mDataSource->readAt(
                        data_offset, buffer->data(), chunk_data_size) < chunk_data_size) {
                return ERROR_IO;
            }

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            mLastTrack->meta->setData(
                    kKeyAVCC, kTypeAVCC, buffer->data(), chunk_data_size);

            *offset += chunk_size;
            break;
        }

        case FOURCC('d', '2', '6', '3'):
        {
            /*
             * d263 contains a fixed 7 bytes part:
             *   vendor - 4 bytes
             *   version - 1 byte
             *   level - 1 byte
             *   profile - 1 byte
             * optionally, "d263" box itself may contain a 16-byte
             * bit rate box (bitr)
             *   average bit rate - 4 bytes
             *   max bit rate - 4 bytes
             */
            char buffer[23];
            if (chunk_data_size != 7 &&
                chunk_data_size != 23) {
                ALOGE("Incorrect D263 box size %lld", chunk_data_size);
                return ERROR_MALFORMED;
            }

            if (mDataSource->readAt(
                    data_offset, buffer, chunk_data_size) < chunk_data_size) {
                return ERROR_IO;
            }

            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            mLastTrack->meta->setData(kKeyD263, kTypeD263, buffer, chunk_data_size);

            *offset += chunk_size;
            break;
        }

        case FOURCC('m', 'e', 't', 'a'):
        {
            uint8_t buffer[4];
            if (chunk_data_size < (off64_t)sizeof(buffer)) {
                return ERROR_MALFORMED;
            }

            if (mDataSource->readAt(
                        data_offset, buffer, 4) < 4) {
                return ERROR_IO;
            }

            if (U32_AT(buffer) != 0) {
                // Should be version 0, flags 0.

                // If it's not, let's assume this is one of those
                // apparently malformed chunks that don't have flags
                // and completely different semantics than what's
                // in the MPEG4 specs and skip it.
                *offset += chunk_size;
                return OK;
            }

            off64_t stop_offset = *offset + chunk_size;
            *offset = data_offset + sizeof(buffer);
            while (*offset < stop_offset) {
                status_t err = parseChunk(offset, depth + 1);
                if (err != OK) {
                    return err;
                }
            }

            if (*offset != stop_offset) {
                return ERROR_MALFORMED;
            }
            break;
        }

        case FOURCC('m', 'e', 'a', 'n'):
        case FOURCC('n', 'a', 'm', 'e'):
        case FOURCC('d', 'a', 't', 'a'):
        {
            if (mPath.Length() == 6 && underMetaDataPath(mPath)) {
                status_t err = parseMetaData(data_offset, chunk_data_size);

                if (err != OK) {
                    return err;
                }
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('m', 'v', 'h', 'd'):
        {
            if (chunk_data_size < 24) {
                return ERROR_MALFORMED;
            }

            uint8_t header[24];
            if (mDataSource->readAt(
                        data_offset, header, sizeof(header))
                    < (ssize_t)sizeof(header)) {
                return ERROR_IO;
            }

            if (header[0] == 1) {
                mHeaderTimescale = U32_AT(&header[20]);
            } else if (header[0] != 0) {
                return ERROR_MALFORMED;
            } else {
                mHeaderTimescale = U32_AT(&header[12]);
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('m', 'e', 'h', 'd'):
        {
            if (chunk_data_size < 8) {
                return ERROR_MALFORMED;
            }

            uint8_t version;
            if (mDataSource->readAt(
                        data_offset, &version, sizeof(version))
                    < (ssize_t)sizeof(version)) {
                return ERROR_IO;
            }
            if (version > 1) {
                break;
            }
            int64_t duration = 0;
            if (version == 1) {
                if (mDataSource->readAt(
                            data_offset + 4, &duration, sizeof(duration))
                        < (ssize_t)sizeof(duration)) {
                    return ERROR_IO;
                }
                duration = ntoh64(duration);
            } else {
                uint32_t duration32;
                if (mDataSource->readAt(
                            data_offset + 4, &duration32, sizeof(duration32))
                        < (ssize_t)sizeof(duration32)) {
                    return ERROR_IO;
                }
                duration = ntohl(duration32);
            }
            if (duration && mHeaderTimescale) {
                mFileMetaData->setInt64(
                        kKeyMovieDuration, (duration * 1000000) / mHeaderTimescale);
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('m', 'd', 'a', 't'):
        {
            ALOGV("mdat chunk, drm: %d", mIsDrm);
            if (!mIsDrm) {
                *offset += chunk_size;
                break;
            }

            if (chunk_size < 8) {
                return ERROR_MALFORMED;
            }

            return parseDrmSINF(offset, data_offset);
        }

        case FOURCC('h', 'd', 'l', 'r'):
        {
            uint32_t buffer;
            if (mDataSource->readAt(
                        data_offset + 8, &buffer, 4) < 4) {
                return ERROR_IO;
            }

            uint32_t type = ntohl(buffer);
            // For the 3GPP file format, the handler-type within the 'hdlr' box
            // shall be 'text'. We also want to support 'sbtl' handler type
            // for a practical reason as various MPEG4 containers use it.
            if (type == FOURCC('t', 'e', 'x', 't') || type == FOURCC('s', 'b', 't', 'l')) {
                if (!mLastTrack) {
                  return ERROR_MALFORMED;
                }
                mLastTrack->meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_TEXT_3GPP);
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('t', 'x', '3', 'g'):
        {
            if (!mLastTrack) {
              return ERROR_MALFORMED;
            }
            uint32_t type;
            const void *data;
            size_t size = 0;
            if (!mLastTrack->meta->findData(
                    kKeyTextFormatData, &type, &data, &size)) {
                size = 0;
            }

            // Make sure (size + chunk_size) isn't going to overflow.
            if (size >= kMAX_ALLOCATION - chunk_size) {
                return ERROR_MALFORMED;
            }
            uint8_t *buffer = new (fallible) uint8_t[size + chunk_size];
            if (!buffer) {
                return -ENOMEM;
            }

            if (size > 0) {
                memcpy(buffer, data, size);
            }

            if ((size_t)(mDataSource->readAt(*offset, buffer + size, chunk_size))
                    < chunk_size) {
                delete[] buffer;
                buffer = NULL;

                return ERROR_IO;
            }

            mLastTrack->meta->setData(
                    kKeyTextFormatData, 0, buffer, size + chunk_size);

            delete[] buffer;

            *offset += chunk_size;
            break;
        }

        case FOURCC('c', 'o', 'v', 'r'):
        {
            if (mFileMetaData != NULL) {
                ALOGV("chunk_data_size = %lld and data_offset = %lld",
                        chunk_data_size, data_offset);
                const int kSkipBytesOfDataBox = 16;
                if (chunk_data_size <= kSkipBytesOfDataBox) {
                  return ERROR_MALFORMED;
                }
                sp<ABuffer> buffer = new (fallible) ABuffer(chunk_data_size + 1);
                if (!buffer.get() || !buffer->data()) {
                    return -ENOMEM;
                }
                if (mDataSource->readAt(
                    data_offset, buffer->data(), chunk_data_size) != (ssize_t)chunk_data_size) {
                    return ERROR_IO;
                }
                mFileMetaData->setData(
                    kKeyAlbumArt, MetaData::TYPE_NONE,
                    buffer->data() + kSkipBytesOfDataBox, chunk_data_size - kSkipBytesOfDataBox);
            }

            *offset += chunk_size;
            break;
        }

        case FOURCC('-', '-', '-', '-'):
        {
            mLastCommentMean.clear();
            mLastCommentName.clear();
            mLastCommentData.clear();
            *offset += chunk_size;
            break;
        }

        case FOURCC('s', 'i', 'd', 'x'):
        {
            parseSegmentIndex(data_offset, chunk_data_size);
            *offset += chunk_size;
            return UNKNOWN_ERROR; // stop parsing after sidx
        }

        case FOURCC('w', 'a', 'v', 'e'):
        {
            off64_t stop_offset = *offset + chunk_size;
            *offset = data_offset;
            while (*offset < stop_offset) {
                status_t err = parseChunk(offset, depth + 1);
                if (err != OK) {
                    return err;
                }
            }

            if (*offset != stop_offset) {
                return ERROR_MALFORMED;
            }
            break;
        }

        default:
        {
            *offset += chunk_size;
            break;
        }
    }

    return OK;
}

void MPEG4Extractor::storeEditList()
{
  if (mHeaderTimescale == 0 ||
      !mLastTrack ||
      mLastTrack->timescale == 0) {
    return;
  }

  uint64_t segment_duration = (mLastTrack->segment_duration * 1000000) / mHeaderTimescale;
  // media_time is measured in media time scale units.
  int64_t media_time = (mLastTrack->media_time * 1000000) / mLastTrack->timescale;
  // empty_duration is in the Movie Header Box's timescale.
  int64_t empty_duration = (mLastTrack->empty_duration * 1000000) / mHeaderTimescale;
  media_time -= empty_duration;
  mLastTrack->meta->setInt64(kKeyMediaTime, media_time);

  int64_t duration;
  int32_t samplerate;
  if (mLastTrack->meta->findInt64(kKeyDuration, &duration) &&
      mLastTrack->meta->findInt32(kKeySampleRate, &samplerate)) {

    int64_t delay = (media_time  * samplerate + 500000) / 1000000;
    mLastTrack->meta->setInt32(kKeyEncoderDelay, delay);

    int64_t paddingus = duration - (segment_duration + media_time);
    int64_t paddingsamples = (paddingus * samplerate + 500000) / 1000000;
    mLastTrack->meta->setInt32(kKeyEncoderPadding, paddingsamples);
  }
}

status_t MPEG4Extractor::parseSegmentIndex(off64_t offset, size_t size) {
  ALOGV("MPEG4Extractor::parseSegmentIndex");

    if (size < 12) {
      return -EINVAL;
    }

    uint32_t flags;
    if (!mDataSource->getUInt32(offset, &flags)) {
        return ERROR_MALFORMED;
    }

    uint32_t version = flags >> 24;
    flags &= 0xffffff;

    ALOGV("sidx version %d", version);

    uint32_t referenceId;
    if (!mDataSource->getUInt32(offset + 4, &referenceId)) {
        return ERROR_MALFORMED;
    }

    uint32_t timeScale;
    if (!mDataSource->getUInt32(offset + 8, &timeScale)) {
        return ERROR_MALFORMED;
    }
    if (!timeScale) {
        return ERROR_MALFORMED;
    }
    ALOGV("sidx refid/timescale: %d/%d", referenceId, timeScale);

    uint64_t earliestPresentationTime;
    uint64_t firstOffset;

    offset += 12;
    size -= 12;

    if (version == 0) {
        if (size < 8) {
            return -EINVAL;
        }
        uint32_t tmp;
        if (!mDataSource->getUInt32(offset, &tmp)) {
            return ERROR_MALFORMED;
        }
        earliestPresentationTime = tmp;
        if (!mDataSource->getUInt32(offset + 4, &tmp)) {
            return ERROR_MALFORMED;
        }
        firstOffset = tmp;
        offset += 8;
        size -= 8;
    } else {
        if (size < 16) {
            return -EINVAL;
        }
        if (!mDataSource->getUInt64(offset, &earliestPresentationTime)) {
            return ERROR_MALFORMED;
        }
        if (!mDataSource->getUInt64(offset + 8, &firstOffset)) {
            return ERROR_MALFORMED;
        }
        offset += 16;
        size -= 16;
    }
    ALOGV("sidx pres/off: %Ld/%Ld", earliestPresentationTime, firstOffset);

    if (size < 4) {
        return -EINVAL;
    }

    uint16_t referenceCount;
    if (!mDataSource->getUInt16(offset + 2, &referenceCount)) {
        return ERROR_MALFORMED;
    }
    offset += 4;
    size -= 4;
    ALOGV("refcount: %d", referenceCount);

    if (size < referenceCount * 12) {
        return -EINVAL;
    }

    uint64_t total_duration = 0;
    for (unsigned int i = 0; i < referenceCount; i++) {
        uint32_t d1, d2, d3;

        if (!mDataSource->getUInt32(offset, &d1) ||     // size
            !mDataSource->getUInt32(offset + 4, &d2) || // duration
            !mDataSource->getUInt32(offset + 8, &d3)) { // flags
            return ERROR_MALFORMED;
        }

        if (d1 & 0x80000000) {
            ALOGW("sub-sidx boxes not supported yet");
        }
        bool sap = d3 & 0x80000000;
        uint32_t saptype = (d3 >> 28) & 0x3;
        if (!sap || saptype > 2) {
            ALOGW("not a stream access point, or unsupported type");
        }
        total_duration += d2;
        offset += 12;
        ALOGV(" item %d, %08x %08x %08x", i, d1, d2, d3);
        SidxEntry se;
        se.mSize = d1 & 0x7fffffff;
        se.mDurationUs = 1000000LL * d2 / timeScale;
        mSidxEntries.AppendElement(se);
    }

    mSidxDuration = total_duration * 1000000 / timeScale;
    ALOGV("duration: %lld", mSidxDuration);

    if (!mLastTrack) {
      return ERROR_MALFORMED;
    }
    int64_t metaDuration;
    if (!mLastTrack->meta->findInt64(kKeyDuration, &metaDuration) || metaDuration == 0) {
        mLastTrack->meta->setInt64(kKeyDuration, mSidxDuration);
    }
    return OK;
}

status_t MPEG4Extractor::parseTrackHeader(
        off64_t data_offset, off64_t data_size) {
    if (data_size < 4) {
        return ERROR_MALFORMED;
    }

    uint8_t version;
    if (mDataSource->readAt(data_offset, &version, 1) < 1) {
        return ERROR_IO;
    }

    size_t dynSize = (version == 1) ? 36 : 24;

    uint8_t buffer[36 + 60];

    if (data_size != (off64_t)dynSize + 60) {
        return ERROR_MALFORMED;
    }

    if (mDataSource->readAt(
                data_offset, buffer, data_size) < (ssize_t)data_size) {
        return ERROR_IO;
    }

    uint64_t ctime, mtime, duration;
    int32_t id;

    if (version == 1) {
        ctime = U64_AT(&buffer[4]);
        mtime = U64_AT(&buffer[12]);
        id = U32_AT(&buffer[20]);
        duration = U64_AT(&buffer[28]);
    } else if (version == 0) {
        ctime = U32_AT(&buffer[4]);
        mtime = U32_AT(&buffer[8]);
        id = U32_AT(&buffer[12]);
        duration = U32_AT(&buffer[20]);
    } else {
        return ERROR_UNSUPPORTED;
    }

    if (!mLastTrack) {
      return ERROR_MALFORMED;
    }
    mLastTrack->meta->setInt32(kKeyTrackID, id);

    size_t matrixOffset = dynSize + 16;
    int32_t a00 = U32_AT(&buffer[matrixOffset]);
    int32_t a01 = U32_AT(&buffer[matrixOffset + 4]);
    int32_t dx = U32_AT(&buffer[matrixOffset + 8]);
    int32_t a10 = U32_AT(&buffer[matrixOffset + 12]);
    int32_t a11 = U32_AT(&buffer[matrixOffset + 16]);
    int32_t dy = U32_AT(&buffer[matrixOffset + 20]);

#if 0
    ALOGI("x' = %.2f * x + %.2f * y + %.2f",
         a00 / 65536.0f, a01 / 65536.0f, dx / 65536.0f);
    ALOGI("y' = %.2f * x + %.2f * y + %.2f",
         a10 / 65536.0f, a11 / 65536.0f, dy / 65536.0f);
#endif

    uint32_t rotationDegrees;

    static const int32_t kFixedOne = 0x10000;
    if (a00 == kFixedOne && a01 == 0 && a10 == 0 && a11 == kFixedOne) {
        // Identity, no rotation
        rotationDegrees = 0;
    } else if (a00 == 0 && a01 == kFixedOne && a10 == -kFixedOne && a11 == 0) {
        rotationDegrees = 90;
    } else if (a00 == 0 && a01 == -kFixedOne && a10 == kFixedOne && a11 == 0) {
        rotationDegrees = 270;
    } else if (a00 == -kFixedOne && a01 == 0 && a10 == 0 && a11 == -kFixedOne) {
        rotationDegrees = 180;
    } else {
        ALOGW("We only support 0,90,180,270 degree rotation matrices");
        rotationDegrees = 0;
    }

    if (rotationDegrees != 0) {
        mLastTrack->meta->setInt32(kKeyRotation, rotationDegrees);
    }

    // Handle presentation display size, which could be different
    // from the image size indicated by kKeyWidth and kKeyHeight.
    uint32_t width = U32_AT(&buffer[dynSize + 52]);
    uint32_t height = U32_AT(&buffer[dynSize + 56]);
    mLastTrack->meta->setInt32(kKeyDisplayWidth, width >> 16);
    mLastTrack->meta->setInt32(kKeyDisplayHeight, height >> 16);

    return OK;
}

status_t MPEG4Extractor::parseMetaData(off64_t offset, size_t size) {
    if (size < 4) {
        return ERROR_MALFORMED;
    }

    FallibleTArray<uint8_t> bufferBackend;
    if (!bufferBackend.SetLength(size + 1, mozilla::fallible)) {
        // OOM ignore metadata.
        return OK;
    }

    uint8_t *buffer = bufferBackend.Elements();
    if (mDataSource->readAt(
                offset, buffer, size) != (ssize_t)size) {
        return ERROR_IO;
    }

    uint32_t flags = U32_AT(buffer);

    uint32_t metadataKey = 0;
    char chunk[5];
    MakeFourCCString(mPath[4], chunk);
    ALOGV("meta: %s @ %lld", chunk, offset);
    switch (mPath[4]) {
        case FOURCC(0xa9, 'a', 'l', 'b'):
        {
            metadataKey = kKeyAlbum;
            break;
        }
        case FOURCC(0xa9, 'A', 'R', 'T'):
        {
            metadataKey = kKeyArtist;
            break;
        }
        case FOURCC('a', 'A', 'R', 'T'):
        {
            metadataKey = kKeyAlbumArtist;
            break;
        }
        case FOURCC(0xa9, 'd', 'a', 'y'):
        {
            metadataKey = kKeyYear;
            break;
        }
        case FOURCC(0xa9, 'n', 'a', 'm'):
        {
            metadataKey = kKeyTitle;
            break;
        }
        case FOURCC(0xa9, 'w', 'r', 't'):
        {
            metadataKey = kKeyWriter;
            break;
        }
        case FOURCC('c', 'o', 'v', 'r'):
        {
            metadataKey = kKeyAlbumArt;
            break;
        }
        case FOURCC('g', 'n', 'r', 'e'):
        {
            metadataKey = kKeyGenre;
            break;
        }
        case FOURCC(0xa9, 'g', 'e', 'n'):
        {
            metadataKey = kKeyGenre;
            break;
        }
        case FOURCC('c', 'p', 'i', 'l'):
        {
            if (size == 9 && flags == 21) {
                char tmp[16];
                sprintf(tmp, "%d",
                        (int)buffer[size - 1]);

                mFileMetaData->setCString(kKeyCompilation, tmp);
            }
            break;
        }
        case FOURCC('t', 'r', 'k', 'n'):
        {
            if (size == 16 && flags == 0) {
                char tmp[16];
                uint16_t* pTrack = (uint16_t*)&buffer[10];
                uint16_t* pTotalTracks = (uint16_t*)&buffer[12];
                sprintf(tmp, "%d/%d", ntohs(*pTrack), ntohs(*pTotalTracks));

                mFileMetaData->setCString(kKeyCDTrackNumber, tmp);
            }
            break;
        }
        case FOURCC('d', 'i', 's', 'k'):
        {
            if ((size == 14 || size == 16) && flags == 0) {
                char tmp[16];
                uint16_t* pDisc = (uint16_t*)&buffer[10];
                uint16_t* pTotalDiscs = (uint16_t*)&buffer[12];
                sprintf(tmp, "%d/%d", ntohs(*pDisc), ntohs(*pTotalDiscs));

                mFileMetaData->setCString(kKeyDiscNumber, tmp);
            }
            break;
        }
        case FOURCC('-', '-', '-', '-'):
        {
            buffer[size] = '\0';
            switch (mPath[5]) {
                case FOURCC('m', 'e', 'a', 'n'):
                    mLastCommentMean.setTo((const char *)buffer + 4);
                    break;
                case FOURCC('n', 'a', 'm', 'e'):
                    mLastCommentName.setTo((const char *)buffer + 4);
                    break;
                case FOURCC('d', 'a', 't', 'a'):
                    mLastCommentData.setTo((const char *)buffer + 8);
                    break;
            }

            // Once we have a set of mean/name/data info, go ahead and process
            // it to see if its something we are interested in.  Whether or not
            // were are interested in the specific tag, make sure to clear out
            // the set so we can be ready to process another tuple should one
            // show up later in the file.
            if ((mLastCommentMean.length() != 0) &&
                (mLastCommentName.length() != 0) &&
                (mLastCommentData.length() != 0)) {

                if (mLastCommentMean == "com.apple.iTunes"
                        && mLastCommentName == "iTunSMPB") {
                    int32_t delay, padding;
                    if (sscanf(mLastCommentData,
                               " %*x %x %x %*x", &delay, &padding) == 2) {
                        if (!mLastTrack) {
                          return ERROR_MALFORMED;
                        }
                        mLastTrack->meta->setInt32(kKeyEncoderDelay, delay);
                        mLastTrack->meta->setInt32(kKeyEncoderPadding, padding);
                    }
                }

                mLastCommentMean.clear();
                mLastCommentName.clear();
                mLastCommentData.clear();
            }
            break;
        }

        default:
            break;
    }

    if (size >= 8 && metadataKey) {
        if (metadataKey == kKeyAlbumArt) {
            mFileMetaData->setData(
                    kKeyAlbumArt, MetaData::TYPE_NONE,
                    buffer + 8, size - 8);
        } else if (metadataKey == kKeyGenre) {
            if (flags == 0) {
                // uint8_t genre code, iTunes genre codes are
                // the standard id3 codes, except they start
                // at 1 instead of 0 (e.g. Pop is 14, not 13)
                // We use standard id3 numbering, so subtract 1.
                int genrecode = (int)buffer[size - 1];
                genrecode--;
                if (genrecode < 0) {
                    genrecode = 255; // reserved for 'unknown genre'
                }
                char genre[10];
                sprintf(genre, "%d", genrecode);

                mFileMetaData->setCString(metadataKey, genre);
            } else if (flags == 1) {
                // custom genre string
                buffer[size] = '\0';

                mFileMetaData->setCString(
                        metadataKey, (const char *)buffer + 8);
            }
        } else {
            buffer[size] = '\0';

            mFileMetaData->setCString(
                    metadataKey, (const char *)buffer + 8);
        }
    }

    return OK;
}

sp<MediaSource> MPEG4Extractor::getTrack(size_t index) {
    status_t err;
    if ((err = readMetaData()) != OK) {
        return NULL;
    }

    Track *track = mFirstTrack;
    while (index > 0) {
        if (track == NULL) {
            return NULL;
        }

        track = track->next;
        --index;
    }

    if (track == NULL) {
        return NULL;
    }

    ALOGV("getTrack called, pssh: %d", mPssh.Length());

    return new MPEG4Source(track->meta, track->timescale, track->sampleTable);
}

// static
status_t MPEG4Extractor::verifyTrack(Track *track) {
    int32_t trackId;
    if (!track->meta->findInt32(kKeyTrackID, &trackId)) {
        return ERROR_MALFORMED;
    }

    const char *mime;
    if (!track->meta->findCString(kKeyMIMEType, &mime)) {
        return ERROR_MALFORMED;
    }

    uint32_t type;
    const void *data;
    size_t size;
    if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
        if (!track->meta->findData(kKeyAVCC, &type, &data, &size)
                || type != kTypeAVCC
                || size < 7
                // configurationVersion == 1?
                || reinterpret_cast<const uint8_t*>(data)[0] != 1) {
            return ERROR_MALFORMED;
        }
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_MPEG4)
            || !strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        if (!track->meta->findData(kKeyESDS, &type, &data, &size)
                || type != kTypeESDS) {
            return ERROR_MALFORMED;
        }
    }

    if (!track->sampleTable.get() || !track->sampleTable->isValid()) {
        // Make sure we have all the metadata we need.
        return ERROR_MALFORMED;
    }

    uint32_t keytype;
    const void *key;
    size_t keysize;
    if (track->meta->findData(kKeyCryptoKey, &keytype, &key, &keysize)) {
        if (keysize > 16) {
            return ERROR_MALFORMED;
        }
    }

    return OK;
}

typedef enum {
    //AOT_NONE             = -1,
    //AOT_NULL_OBJECT      = 0,
    //AOT_AAC_MAIN         = 1, /**< Main profile                              */
    AOT_AAC_LC           = 2,   /**< Low Complexity object                     */
    //AOT_AAC_SSR          = 3,
    //AOT_AAC_LTP          = 4,
    AOT_SBR              = 5,
    //AOT_AAC_SCAL         = 6,
    //AOT_TWIN_VQ          = 7,
    //AOT_CELP             = 8,
    //AOT_HVXC             = 9,
    //AOT_RSVD_10          = 10, /**< (reserved)                                */
    //AOT_RSVD_11          = 11, /**< (reserved)                                */
    //AOT_TTSI             = 12, /**< TTSI Object                               */
    //AOT_MAIN_SYNTH       = 13, /**< Main Synthetic object                     */
    //AOT_WAV_TAB_SYNTH    = 14, /**< Wavetable Synthesis object                */
    //AOT_GEN_MIDI         = 15, /**< General MIDI object                       */
    //AOT_ALG_SYNTH_AUD_FX = 16, /**< Algorithmic Synthesis and Audio FX object */
    AOT_ER_AAC_LC        = 17,   /**< Error Resilient(ER) AAC Low Complexity    */
    //AOT_RSVD_18          = 18, /**< (reserved)                                */
    //AOT_ER_AAC_LTP       = 19, /**< Error Resilient(ER) AAC LTP object        */
    AOT_ER_AAC_SCAL      = 20,   /**< Error Resilient(ER) AAC Scalable object   */
    //AOT_ER_TWIN_VQ       = 21, /**< Error Resilient(ER) TwinVQ object         */
    AOT_ER_BSAC          = 22,   /**< Error Resilient(ER) BSAC object           */
    AOT_ER_AAC_LD        = 23,   /**< Error Resilient(ER) AAC LowDelay object   */
    //AOT_ER_CELP          = 24, /**< Error Resilient(ER) CELP object           */
    //AOT_ER_HVXC          = 25, /**< Error Resilient(ER) HVXC object           */
    //AOT_ER_HILN          = 26, /**< Error Resilient(ER) HILN object           */
    //AOT_ER_PARA          = 27, /**< Error Resilient(ER) Parametric object     */
    //AOT_RSVD_28          = 28, /**< might become SSC                          */
    AOT_PS               = 29,   /**< PS, Parametric Stereo (includes SBR)      */
    //AOT_MPEGS            = 30, /**< MPEG Surround                             */

    AOT_ESCAPE           = 31,   /**< Signal AOT uses more than 5 bits          */

    //AOT_MP3ONMP4_L1      = 32, /**< MPEG-Layer1 in mp4                        */
    //AOT_MP3ONMP4_L2      = 33, /**< MPEG-Layer2 in mp4                        */
    //AOT_MP3ONMP4_L3      = 34, /**< MPEG-Layer3 in mp4                        */
    //AOT_RSVD_35          = 35, /**< might become DST                          */
    //AOT_RSVD_36          = 36, /**< might become ALS                          */
    //AOT_AAC_SLS          = 37, /**< AAC + SLS                                 */
    //AOT_SLS              = 38, /**< SLS                                       */
    //AOT_ER_AAC_ELD       = 39, /**< AAC Enhanced Low Delay                    */

    //AOT_USAC             = 42, /**< USAC                                      */
    //AOT_SAOC             = 43, /**< SAOC                                      */
    //AOT_LD_MPEGS         = 44, /**< Low Delay MPEG Surround                   */

    //AOT_RSVD50           = 50,  /**< Interim AOT for Rsvd50                   */
} AUDIO_OBJECT_TYPE;

status_t MPEG4Extractor::updateAudioTrackInfoFromESDS_MPEG4Audio(
        const void *esds_data, size_t esds_size) {
    ESDS esds(esds_data, esds_size);

    uint8_t objectTypeIndication;
    if (esds.getObjectTypeIndication(&objectTypeIndication) != OK) {
        return ERROR_MALFORMED;
    }

    if (objectTypeIndication == 0xe1) {
        // This isn't MPEG4 audio at all, it's QCELP 14k...
        if (mLastTrack == NULL)
            return ERROR_MALFORMED;

        mLastTrack->meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_QCELP);
        return OK;
    }

    if (objectTypeIndication  == 0x6b || objectTypeIndication  == 0x69) {
        // The media subtype is MP3 audio
        if (!mLastTrack) {
          return ERROR_MALFORMED;
        }
        mLastTrack->meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_MPEG);
    }

    const uint8_t *csd;
    size_t csd_size;
    if (esds.getCodecSpecificInfo(
                (const void **)&csd, &csd_size) != OK) {
        return ERROR_MALFORMED;
    }

#if 0
    if (kUseHexDump) {
        printf("ESD of size %zu\n", csd_size);
        hexdump(csd, csd_size);
    }
#endif

    if (csd_size == 0) {
        // There's no further information, i.e. no codec specific data
        // Let's assume that the information provided in the mpeg4 headers
        // is accurate and hope for the best.

        return OK;
    }

    if (csd_size < 2) {
        return ERROR_MALFORMED;
    }

    static uint32_t kSamplingRate[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000, 7350
    };

    ABitReader br(csd, csd_size);
    if (br.numBitsLeft() < 5) {
        return ERROR_MALFORMED;
    }
    uint32_t objectType = br.getBits(5);

    if (objectType == 31) {  // AAC-ELD => additional 6 bits
        if (br.numBitsLeft() < 6) {
            return ERROR_MALFORMED;
        }
        objectType = 32 + br.getBits(6);
    }

    if (mLastTrack == NULL)
        return ERROR_MALFORMED;

    if (objectType >= 1 && objectType <= 4) {
        mLastTrack->meta->setInt32(kKeyAACProfile, objectType);
    }

    //keep AOT type
    mLastTrack->meta->setInt32(kKeyAACAOT, objectType);

    if (br.numBitsLeft() < 4) {
        return ERROR_MALFORMED;
    }
    uint32_t freqIndex = br.getBits(4);

    int32_t sampleRate = 0;
    int32_t numChannels = 0;
    if (freqIndex == 15) {
        if (br.numBitsLeft() < 28) return ERROR_MALFORMED;
        sampleRate = br.getBits(24);
        numChannels = br.getBits(4);
    } else {
        if (br.numBitsLeft() < 4) return ERROR_MALFORMED;
        numChannels = br.getBits(4);

        if (freqIndex == 13 || freqIndex == 14) {
            return ERROR_MALFORMED;
        }

        sampleRate = kSamplingRate[freqIndex];
    }

    if (objectType == AOT_SBR || objectType == AOT_PS) {//SBR specific config per 14496-3 table 1.13
        if (br.numBitsLeft() < 4) return ERROR_MALFORMED;
        uint32_t extFreqIndex = br.getBits(4);
        int32_t extSampleRate;
        if (extFreqIndex == 15) {
            if (csd_size < 8) {
                return ERROR_MALFORMED;
            }
            if (br.numBitsLeft() < 24) return ERROR_MALFORMED;
            extSampleRate = br.getBits(24);
        } else {
            if (extFreqIndex == 13 || extFreqIndex == 14) {
                return ERROR_MALFORMED;
            }
            extSampleRate = kSamplingRate[extFreqIndex];
        }
        //TODO: save the extension sampling rate value in meta data =>
        //      mLastTrack->meta->setInt32(kKeyExtSampleRate, extSampleRate);
    }

    switch (numChannels) {
        // values defined in 14496-3_2009 amendment-4 Table 1.19 - Channel Configuration
        case 0:
        case 1:// FC
        case 2:// FL FR
        case 3:// FC, FL FR
        case 4:// FC, FL FR, RC
        case 5:// FC, FL FR, SL SR
        case 6:// FC, FL FR, SL SR, LFE
            //numChannels already contains the right value
            break;
        case 11:// FC, FL FR, SL SR, RC, LFE
            numChannels = 7;
            break;
        case 7: // FC, FCL FCR, FL FR, SL SR, LFE
        case 12:// FC, FL  FR,  SL SR, RL RR, LFE
        case 14:// FC, FL  FR,  SL SR, LFE, FHL FHR
            numChannels = 8;
            break;
        default:
            return ERROR_UNSUPPORTED;
    }

    {
        if (objectType == AOT_SBR || objectType == AOT_PS) {
            if (br.numBitsLeft() < 5) return ERROR_MALFORMED;
            objectType = br.getBits(5);

            if (objectType == AOT_ESCAPE) {
                if (br.numBitsLeft() < 6) return ERROR_MALFORMED;
                objectType = 32 + br.getBits(6);
            }
        }
        if (objectType == AOT_AAC_LC || objectType == AOT_ER_AAC_LC ||
                objectType == AOT_ER_AAC_LD || objectType == AOT_ER_AAC_SCAL ||
                objectType == AOT_ER_BSAC) {
            if (br.numBitsLeft() < 2) return ERROR_MALFORMED;
            const int32_t frameLengthFlag = br.getBits(1);

            const int32_t dependsOnCoreCoder = br.getBits(1);

            if (dependsOnCoreCoder ) {
                if (br.numBitsLeft() < 14) return ERROR_MALFORMED;
                const int32_t coreCoderDelay = br.getBits(14);
            }

            int32_t extensionFlag = -1;
            if (br.numBitsLeft() > 0) {
                extensionFlag = br.getBits(1);
            } else {
                switch (objectType) {
                // 14496-3 4.5.1.1 extensionFlag
                case AOT_AAC_LC:
                    extensionFlag = 0;
                    break;
                case AOT_ER_AAC_LC:
                case AOT_ER_AAC_SCAL:
                case AOT_ER_BSAC:
                case AOT_ER_AAC_LD:
                    extensionFlag = 1;
                    break;
                default:
                    return ERROR_MALFORMED;
                    break;
                }
                ALOGW("csd missing extension flag; assuming %d for object type %u.",
                        extensionFlag, objectType);
            }

            if (numChannels == 0) {
                int32_t channelsEffectiveNum = 0;
                int32_t channelsNum = 0;
                if (br.numBitsLeft() < 32) {
                    return ERROR_MALFORMED;
                }
                const int32_t ElementInstanceTag = br.getBits(4);
                const int32_t Profile = br.getBits(2);
                const int32_t SamplingFrequencyIndex = br.getBits(4);
                const int32_t NumFrontChannelElements = br.getBits(4);
                const int32_t NumSideChannelElements = br.getBits(4);
                const int32_t NumBackChannelElements = br.getBits(4);
                const int32_t NumLfeChannelElements = br.getBits(2);
                const int32_t NumAssocDataElements = br.getBits(3);
                const int32_t NumValidCcElements = br.getBits(4);

                const int32_t MonoMixdownPresent = br.getBits(1);

                if (MonoMixdownPresent != 0) {
                    if (br.numBitsLeft() < 4) return ERROR_MALFORMED;
                    const int32_t MonoMixdownElementNumber = br.getBits(4);
                }

                if (br.numBitsLeft() < 1) return ERROR_MALFORMED;
                const int32_t StereoMixdownPresent = br.getBits(1);
                if (StereoMixdownPresent != 0) {
                    if (br.numBitsLeft() < 4) return ERROR_MALFORMED;
                    const int32_t StereoMixdownElementNumber = br.getBits(4);
                }

                if (br.numBitsLeft() < 1) return ERROR_MALFORMED;
                const int32_t MatrixMixdownIndexPresent = br.getBits(1);
                if (MatrixMixdownIndexPresent != 0) {
                    if (br.numBitsLeft() < 3) return ERROR_MALFORMED;
                    const int32_t MatrixMixdownIndex = br.getBits(2);
                    const int32_t PseudoSurroundEnable = br.getBits(1);
                }

                int i;
                for (i=0; i < NumFrontChannelElements; i++) {
                    if (br.numBitsLeft() < 5) return ERROR_MALFORMED;
                    const int32_t FrontElementIsCpe = br.getBits(1);
                    const int32_t FrontElementTagSelect = br.getBits(4);
                    channelsNum += FrontElementIsCpe ? 2 : 1;
                }

                for (i=0; i < NumSideChannelElements; i++) {
                    if (br.numBitsLeft() < 5) return ERROR_MALFORMED;
                    const int32_t SideElementIsCpe = br.getBits(1);
                    const int32_t SideElementTagSelect = br.getBits(4);
                    channelsNum += SideElementIsCpe ? 2 : 1;
                }

                for (i=0; i < NumBackChannelElements; i++) {
                    if (br.numBitsLeft() < 5) return ERROR_MALFORMED;
                    const int32_t BackElementIsCpe = br.getBits(1);
                    const int32_t BackElementTagSelect = br.getBits(4);
                    channelsNum += BackElementIsCpe ? 2 : 1;
                }
                channelsEffectiveNum = channelsNum;

                for (i=0; i < NumLfeChannelElements; i++) {
                    if (br.numBitsLeft() < 4) return ERROR_MALFORMED;
                    const int32_t LfeElementTagSelect = br.getBits(4);
                    channelsNum += 1;
                }
                ALOGV("mpeg4 audio channelsNum = %d", channelsNum);
                ALOGV("mpeg4 audio channelsEffectiveNum = %d", channelsEffectiveNum);
                numChannels = channelsNum;
            }
        }
    }

    if (numChannels == 0) {
        return ERROR_UNSUPPORTED;
    }

    if (mLastTrack == NULL)
        return ERROR_MALFORMED;

    int32_t prevSampleRate;
    CHECK(mLastTrack->meta->findInt32(kKeySampleRate, &prevSampleRate));

    if (prevSampleRate != sampleRate) {
        ALOGV("mpeg4 audio sample rate different from previous setting. "
             "was: %d, now: %d", prevSampleRate, sampleRate);
    }

    mLastTrack->meta->setInt32(kKeySampleRate, sampleRate);

    int32_t prevChannelCount;
    CHECK(mLastTrack->meta->findInt32(kKeyChannelCount, &prevChannelCount));

    if (prevChannelCount != numChannels) {
        ALOGV("mpeg4 audio channel count different from previous setting. "
             "was: %d, now: %d", prevChannelCount, numChannels);
    }

    mLastTrack->meta->setInt32(kKeyChannelCount, numChannels);

    return OK;
}

////////////////////////////////////////////////////////////////////////////////

MPEG4Source::MPEG4Source(
        const sp<MetaData> &format,
        uint32_t timeScale,
        const sp<SampleTable> &sampleTable)
    : mFormat(format),
      mTimescale(timeScale),
      mSampleTable(sampleTable) {
}

MPEG4Source::~MPEG4Source() {
}

sp<MetaData> MPEG4Source::getFormat() {
    return mFormat;
}

class CompositionSorter
{
public:
  bool LessThan(MediaSource::Indice* aFirst, MediaSource::Indice* aSecond) const
  {
    return aFirst->start_composition < aSecond->start_composition;
  }

  bool Equals(MediaSource::Indice* aFirst, MediaSource::Indice* aSecond) const
  {
    return aFirst->start_composition == aSecond->start_composition;
  }
};

nsTArray<MediaSource::Indice> MPEG4Source::exportIndex()
{
  nsTArray<MediaSource::Indice> index;
  if (!mTimescale || !mSampleTable.get()) {
    return index;
  }

  if (!index.SetCapacity(mSampleTable->countSamples(), mozilla::fallible)) {
    return index;
  }
  for (uint32_t sampleIndex = 0; sampleIndex < mSampleTable->countSamples();
          sampleIndex++) {
      off64_t offset;
      size_t size;
      uint32_t compositionTime;
      uint32_t duration;
      bool isSyncSample;
      uint32_t decodeTime;
      if (mSampleTable->getMetaDataForSample(sampleIndex, &offset, &size,
                                             &compositionTime, &duration,
                                             &isSyncSample, &decodeTime) != OK) {
          ALOGE("Unexpected sample table problem");
          continue;
      }

      Indice indice;
      indice.start_offset = offset;
      indice.end_offset = offset + size;
      indice.start_composition = (compositionTime * 1000000ll) / mTimescale;
      // end_composition is overwritten everywhere except the last frame, where
      // the presentation duration is equal to the sample duration.
      indice.end_composition =
          (compositionTime * 1000000ll + duration * 1000000ll) / mTimescale;
      indice.sync = isSyncSample;
      indice.start_decode = (decodeTime * 1000000ll) / mTimescale;
      index.AppendElement(indice);
  }

  // Fix up composition durations so we don't end up with any unsightly gaps.
  if (index.Length() != 0) {
      nsTArray<Indice*> composition_order;
      if (!composition_order.SetCapacity(index.Length(), mozilla::fallible)) {
        return index;
      }
      for (uint32_t i = 0; i < index.Length(); i++) {
        composition_order.AppendElement(&index[i]);
      }

      composition_order.Sort(CompositionSorter());
      for (uint32_t i = 0; i + 1 < composition_order.Length(); i++) {
        composition_order[i]->end_composition =
                composition_order[i + 1]->start_composition;
      }
  }

  return index;
}

}  // namespace stagefright

#undef LOG_TAG
