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

#undef LOG_TAG
#define LOG_TAG "SampleTable"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "include/SampleTable.h"
#include "include/SampleIterator.h"

#include <arpa/inet.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/Utils.h>

#include <stdint.h>

namespace stagefright {

// static
const uint32_t SampleTable::kChunkOffsetType32 = FOURCC('s', 't', 'c', 'o');
// static
const uint32_t SampleTable::kChunkOffsetType64 = FOURCC('c', 'o', '6', '4');
// static
const uint32_t SampleTable::kSampleSizeType32 = FOURCC('s', 't', 's', 'z');
// static
const uint32_t SampleTable::kSampleSizeTypeCompact = FOURCC('s', 't', 'z', '2');

const uint32_t kAuxTypeCenc = FOURCC('c', 'e', 'n', 'c');

static const uint32_t kMAX_ALLOCATION =
    (SIZE_MAX < INT32_MAX ? SIZE_MAX : INT32_MAX) - 128;

////////////////////////////////////////////////////////////////////////////////

struct SampleTable::CompositionDeltaLookup {
    CompositionDeltaLookup();

    void setEntries(
            const uint32_t *deltaEntries, size_t numDeltaEntries);

    uint32_t getCompositionTimeOffset(uint32_t sampleIndex);

private:
    Mutex mLock;

    const uint32_t *mDeltaEntries;
    size_t mNumDeltaEntries;

    size_t mCurrentDeltaEntry;
    size_t mCurrentEntrySampleIndex;

    DISALLOW_EVIL_CONSTRUCTORS(CompositionDeltaLookup);
};

SampleTable::CompositionDeltaLookup::CompositionDeltaLookup()
    : mDeltaEntries(NULL),
      mNumDeltaEntries(0),
      mCurrentDeltaEntry(0),
      mCurrentEntrySampleIndex(0) {
}

void SampleTable::CompositionDeltaLookup::setEntries(
        const uint32_t *deltaEntries, size_t numDeltaEntries) {
    Mutex::Autolock autolock(mLock);

    mDeltaEntries = deltaEntries;
    mNumDeltaEntries = numDeltaEntries;
    mCurrentDeltaEntry = 0;
    mCurrentEntrySampleIndex = 0;
}

uint32_t SampleTable::CompositionDeltaLookup::getCompositionTimeOffset(
        uint32_t sampleIndex) {
    Mutex::Autolock autolock(mLock);

    if (mDeltaEntries == NULL) {
        return 0;
    }

    if (sampleIndex < mCurrentEntrySampleIndex) {
        mCurrentDeltaEntry = 0;
        mCurrentEntrySampleIndex = 0;
    }

    while (mCurrentDeltaEntry < mNumDeltaEntries) {
        uint32_t sampleCount = mDeltaEntries[2 * mCurrentDeltaEntry];
        if (sampleIndex < mCurrentEntrySampleIndex + sampleCount) {
            return mDeltaEntries[2 * mCurrentDeltaEntry + 1];
        }

        mCurrentEntrySampleIndex += sampleCount;
        ++mCurrentDeltaEntry;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

SampleTable::SampleTable(const sp<DataSource> &source)
    : mDataSource(source),
      mChunkOffsetOffset(-1),
      mChunkOffsetType(0),
      mNumChunkOffsets(0),
      mSampleToChunkOffset(-1),
      mNumSampleToChunkOffsets(0),
      mSampleSizeOffset(-1),
      mSampleSizeFieldSize(0),
      mDefaultSampleSize(0),
      mNumSampleSizes(0),
      mTimeToSampleCount(0),
      mTimeToSample(NULL),
      mSampleTimeEntries(NULL),
      mCompositionTimeDeltaEntries(NULL),
      mNumCompositionTimeDeltaEntries(0),
      mCompositionDeltaLookup(new CompositionDeltaLookup),
      mSyncSampleOffset(-1),
      mNumSyncSamples(0),
      mSyncSamples(NULL),
      mLastSyncSampleIndex(0),
      mSampleToChunkEntries(NULL),
      mCencInfo(NULL),
      mCencInfoCount(0),
      mCencDefaultSize(0)
{
    mSampleIterator = new SampleIterator(this);
}

SampleTable::~SampleTable() {
    delete[] mSampleToChunkEntries;
    mSampleToChunkEntries = NULL;

    delete[] mSyncSamples;
    mSyncSamples = NULL;

    delete mCompositionDeltaLookup;
    mCompositionDeltaLookup = NULL;

    delete[] mCompositionTimeDeltaEntries;
    mCompositionTimeDeltaEntries = NULL;

    delete[] mSampleTimeEntries;
    mSampleTimeEntries = NULL;

    delete[] mTimeToSample;
    mTimeToSample = NULL;

    if (mCencInfo) {
        for (uint32_t i = 0; i < mCencInfoCount; i++) {
            if (mCencInfo[i].mSubsamples) {
                delete[] mCencInfo[i].mSubsamples;
            }
        }
        delete[] mCencInfo;
    }

    delete mSampleIterator;
    mSampleIterator = NULL;
}

bool SampleTable::isValid() const {
    return mChunkOffsetOffset >= 0
        && mSampleToChunkOffset >= 0
        && mSampleSizeOffset >= 0
        && mTimeToSample != NULL;
}

status_t SampleTable::setChunkOffsetParams(
        uint32_t type, off64_t data_offset, size_t data_size) {
    if (mChunkOffsetOffset >= 0) {
        return ERROR_MALFORMED;
    }

    CHECK(type == kChunkOffsetType32 || type == kChunkOffsetType64);

    mChunkOffsetOffset = data_offset;
    mChunkOffsetType = type;

    if (data_size < 8) {
        return ERROR_MALFORMED;
    }

    uint8_t header[8];
    if (mDataSource->readAt(
                data_offset, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return ERROR_IO;
    }

    if (U32_AT(header) != 0) {
        // Expected version = 0, flags = 0.
        return ERROR_MALFORMED;
    }

    mNumChunkOffsets = U32_AT(&header[4]);

    if (mChunkOffsetType == kChunkOffsetType32) {
        if (data_size < 8 + (uint64_t)mNumChunkOffsets * 4) {
            return ERROR_MALFORMED;
        }
    } else {
        if (data_size < 8 + (uint64_t)mNumChunkOffsets * 8) {
            return ERROR_MALFORMED;
        }
    }

    return OK;
}

status_t SampleTable::setSampleToChunkParams(
        off64_t data_offset, size_t data_size) {
    if (mSampleToChunkOffset >= 0) {
        return ERROR_MALFORMED;
    }

    mSampleToChunkOffset = data_offset;

    if (data_size < 8) {
        return ERROR_MALFORMED;
    }

    uint8_t header[8];
    if (mDataSource->readAt(
                data_offset, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return ERROR_IO;
    }

    if (U32_AT(header) != 0) {
        // Expected version = 0, flags = 0.
        return ERROR_MALFORMED;
    }

    mNumSampleToChunkOffsets = U32_AT(&header[4]);

    if (data_size < 8 + (uint64_t)mNumSampleToChunkOffsets * 12) {
        return ERROR_MALFORMED;
    }

    mSampleToChunkEntries =
        new SampleToChunkEntry[mNumSampleToChunkOffsets];

    for (uint32_t i = 0; i < mNumSampleToChunkOffsets; ++i) {
        uint8_t buffer[12];
        if (mDataSource->readAt(
                    mSampleToChunkOffset + 8 + i * 12, buffer, sizeof(buffer))
                != (ssize_t)sizeof(buffer)) {
            return ERROR_IO;
        }

        if (!U32_AT(buffer)) {
          ALOGE("error reading sample to chunk table");
          return ERROR_MALFORMED;  // chunk index is 1 based in the spec.
        }

        // We want the chunk index to be 0-based.
        mSampleToChunkEntries[i].startChunk = U32_AT(buffer) - 1;
        mSampleToChunkEntries[i].samplesPerChunk = U32_AT(&buffer[4]);
        mSampleToChunkEntries[i].chunkDesc = U32_AT(&buffer[8]);
    }

    return OK;
}

status_t SampleTable::setSampleSizeParams(
        uint32_t type, off64_t data_offset, size_t data_size) {
    if (mSampleSizeOffset >= 0) {
        return ERROR_MALFORMED;
    }

    CHECK(type == kSampleSizeType32 || type == kSampleSizeTypeCompact);

    mSampleSizeOffset = data_offset;

    if (data_size < 12) {
        return ERROR_MALFORMED;
    }

    uint8_t header[12];
    if (mDataSource->readAt(
                data_offset, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return ERROR_IO;
    }

    if (U32_AT(header) != 0) {
        // Expected version = 0, flags = 0.
        return ERROR_MALFORMED;
    }

    mDefaultSampleSize = U32_AT(&header[4]);
    mNumSampleSizes = U32_AT(&header[8]);

    if (type == kSampleSizeType32) {
        mSampleSizeFieldSize = 32;

        if (mDefaultSampleSize != 0) {
            return OK;
        }

        if (data_size < 12 + (uint64_t)mNumSampleSizes * 4) {
            return ERROR_MALFORMED;
        }
    } else {
        if ((mDefaultSampleSize & 0xffffff00) != 0) {
            // The high 24 bits are reserved and must be 0.
            return ERROR_MALFORMED;
        }

        mSampleSizeFieldSize = mDefaultSampleSize & 0xff;
        mDefaultSampleSize = 0;

        if (mSampleSizeFieldSize != 4 && mSampleSizeFieldSize != 8
            && mSampleSizeFieldSize != 16) {
            return ERROR_MALFORMED;
        }

        if (data_size < 12 + ((uint64_t)mNumSampleSizes * mSampleSizeFieldSize + 4) / 8) {
            return ERROR_MALFORMED;
        }
    }

    return OK;
}

status_t SampleTable::setTimeToSampleParams(
        off64_t data_offset, size_t data_size) {
    if (mTimeToSample != NULL || data_size < 8) {
        return ERROR_MALFORMED;
    }

    uint8_t header[8];
    if (mDataSource->readAt(
                data_offset, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return ERROR_IO;
    }

    if (U32_AT(header) != 0) {
        // Expected version = 0, flags = 0.
        return ERROR_MALFORMED;
    }

    mTimeToSampleCount = U32_AT(&header[4]);
    if (mTimeToSampleCount > kMAX_ALLOCATION / 2 / sizeof(uint32_t)) {
        // Avoid later overflow.
        return ERROR_MALFORMED;
    }
    mTimeToSample = new uint32_t[mTimeToSampleCount * 2];

    size_t size = sizeof(uint32_t) * mTimeToSampleCount * 2;
    if (mDataSource->readAt(
                data_offset + 8, mTimeToSample, size) < (ssize_t)size) {
        return ERROR_IO;
    }

    for (uint32_t i = 0; i < mTimeToSampleCount * 2; ++i) {
        mTimeToSample[i] = ntohl(mTimeToSample[i]);
    }

    return OK;
}

status_t SampleTable::setCompositionTimeToSampleParams(
        off64_t data_offset, size_t data_size) {
    ALOGI("There are reordered frames present.");

    if (mCompositionTimeDeltaEntries != NULL || data_size < 8) {
        return ERROR_MALFORMED;
    }

    uint8_t header[8];
    if (mDataSource->readAt(
                data_offset, header, sizeof(header))
            < (ssize_t)sizeof(header)) {
        return ERROR_IO;
    }

    if (U32_AT(header) != 0) {
        // Expected version = 0, flags = 0.
        return ERROR_MALFORMED;
    }

    uint32_t numEntries = U32_AT(&header[4]);

    if (data_size != ((uint64_t)numEntries + 1) * 8) {
        return ERROR_MALFORMED;
    }

    mNumCompositionTimeDeltaEntries = numEntries;
    mCompositionTimeDeltaEntries = new uint32_t[2 * numEntries];

    if (mDataSource->readAt(
                data_offset + 8, mCompositionTimeDeltaEntries, numEntries * 8)
            < (ssize_t)numEntries * 8) {
        delete[] mCompositionTimeDeltaEntries;
        mCompositionTimeDeltaEntries = NULL;

        return ERROR_IO;
    }

    for (size_t i = 0; i < 2 * numEntries; ++i) {
        mCompositionTimeDeltaEntries[i] = ntohl(mCompositionTimeDeltaEntries[i]);
    }

    mCompositionDeltaLookup->setEntries(
            mCompositionTimeDeltaEntries, mNumCompositionTimeDeltaEntries);

    return OK;
}

status_t SampleTable::setSyncSampleParams(off64_t data_offset, size_t data_size) {
    if (mSyncSampleOffset >= 0 || data_size < 8) {
        return ERROR_MALFORMED;
    }

    mSyncSampleOffset = data_offset;

    uint8_t header[8];
    if (mDataSource->readAt(
                data_offset, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return ERROR_IO;
    }

    if (U32_AT(header) != 0) {
        // Expected version = 0, flags = 0.
        return ERROR_MALFORMED;
    }

    mNumSyncSamples = U32_AT(&header[4]);
    if (mNumSyncSamples > kMAX_ALLOCATION / sizeof(uint32_t)) {
        // Avoid later overflow.
        return ERROR_MALFORMED;
    }

    if (mNumSyncSamples < 2) {
        ALOGV("Table of sync samples is empty or has only a single entry!");
    }

    mSyncSamples = new uint32_t[mNumSyncSamples];
    size_t size = mNumSyncSamples * sizeof(uint32_t);
    if (mDataSource->readAt(mSyncSampleOffset + 8, mSyncSamples, size)
            != (ssize_t)size) {
        return ERROR_IO;
    }

    for (size_t i = 0; i < mNumSyncSamples; ++i) {
        mSyncSamples[i] = ntohl(mSyncSamples[i]) - 1;
    }

    return OK;
}

static status_t
validateCencBoxHeader(
        sp<DataSource>& data_source, off64_t& data_offset,
        uint8_t* out_version, uint32_t* out_aux_type) {
    *out_aux_type = 0;

    if (data_source->readAt(data_offset++, out_version, 1) < 1) {
        ALOGE("error reading sample aux info header");
        return ERROR_IO;
    }

    uint32_t flags;
    if (!data_source->getUInt24(data_offset, &flags)) {
        ALOGE("error reading sample aux info flags");
        return ERROR_IO;
    }
    data_offset += 3;

    if (flags & 1) {
        uint32_t aux_type;
        uint32_t aux_param;
        if (!data_source->getUInt32(data_offset, &aux_type) ||
            !data_source->getUInt32(data_offset + 4, &aux_param)) {
            ALOGE("error reading aux info type");
            return ERROR_IO;
        }
        data_offset += 8;
        *out_aux_type = aux_type;
    }

    return OK;
}

status_t
SampleTable::setSampleAuxiliaryInformationSizeParams(
        off64_t data_offset, size_t data_size, uint32_t drm_scheme) {
    off64_t data_end = data_offset + data_size;

    uint8_t version;
    uint32_t aux_type;
    status_t err = validateCencBoxHeader(
                mDataSource, data_offset, &version, &aux_type);
    if (err != OK) {
        return err;
    }

    if (aux_type && aux_type != kAuxTypeCenc && drm_scheme != kAuxTypeCenc) {
        // Quietly skip aux types we don't care about.
        return OK;
    }

    if (!mCencSizes.isEmpty() || mCencDefaultSize) {
        ALOGE("duplicate cenc saiz box");
        return ERROR_MALFORMED;
    }

    if (version) {
        ALOGV("unsupported cenc saiz version");
        return ERROR_UNSUPPORTED;
    }

    if (mDataSource->readAt(
                data_offset++, &mCencDefaultSize, sizeof(mCencDefaultSize))
                < sizeof(mCencDefaultSize)) {
        return ERROR_IO;
    }

    if (!mDataSource->getUInt32(data_offset, &mCencInfoCount)) {
        return ERROR_IO;
    }
    data_offset += 4;

    if (!mCencDefaultSize) {
        mCencSizes.insertAt(0, 0, mCencInfoCount);
        if (mDataSource->readAt(
                    data_offset, mCencSizes.editArray(), mCencInfoCount)
                    < mCencInfoCount) {
            return ERROR_IO;
        }
        data_offset += mCencInfoCount;
    }

    CHECK(data_offset == data_end);

    return parseSampleCencInfo();
}

status_t
SampleTable::setSampleAuxiliaryInformationOffsetParams(
        off64_t data_offset, size_t data_size, uint32_t drm_scheme) {
    off64_t data_end = data_offset + data_size;

    uint8_t version;
    uint32_t aux_type;
    status_t err = validateCencBoxHeader(mDataSource, data_offset,
                                         &version, &aux_type);
    if (err != OK) {
        return err;
    }

    if (aux_type && aux_type != kAuxTypeCenc && drm_scheme != kAuxTypeCenc) {
        // Quietly skip aux types we don't care about.
        return OK;
    }

    if (!mCencOffsets.isEmpty()) {
        ALOGE("duplicate cenc saio box");
        return ERROR_MALFORMED;
    }

    uint32_t cencOffsetCount;
    if (!mDataSource->getUInt32(data_offset, &cencOffsetCount)) {
        ALOGE("error reading cenc aux info offset count");
        return ERROR_IO;
    }
    data_offset += 4;

    mCencOffsets.setCapacity(cencOffsetCount);
    if (!version) {
        for (uint32_t i = 0; i < cencOffsetCount; i++) {
            uint32_t tmp;
            if (!mDataSource->getUInt32(data_offset, &tmp)) {
                ALOGE("error reading cenc aux info offsets");
                return ERROR_IO;
            }
            mCencOffsets.push(tmp);
            data_offset += 4;
        }
    } else {
        for (uint32_t i = 0; i < cencOffsetCount; i++) {
            if (!mDataSource->getUInt64(data_offset, &mCencOffsets.editItemAt(i))) {
                ALOGE("error reading cenc aux info offsets");
                return ERROR_IO;
            }
            data_offset += 8;
        }
    }

    CHECK(data_offset == data_end);

    return parseSampleCencInfo();
}

status_t
SampleTable::parseSampleCencInfo() {
    if ((!mCencDefaultSize && !mCencInfoCount) || mCencOffsets.isEmpty()) {
        // We don't have all the cenc information we need yet. Quietly fail and
        // hope we get the data we need later in the track header.
        ALOGV("Got half of cenc saio/saiz pair. Deferring parse until we get the other half.");
        return OK;
    }

    if (!mCencSizes.isEmpty() && mCencOffsets.size() > 1 &&
        mCencSizes.size() != mCencOffsets.size()) {
        return ERROR_MALFORMED;
    }

    if (mCencInfoCount > kMAX_ALLOCATION / sizeof(SampleCencInfo)) {
        // Avoid future OOM.
        return ERROR_MALFORMED;
    }

    mCencInfo = new SampleCencInfo[mCencInfoCount];
    for (uint32_t i = 0; i < mCencInfoCount; i++) {
        mCencInfo[i].mSubsamples = NULL;
    }

    uint64_t nextOffset = mCencOffsets[0];
    for (uint32_t i = 0; i < mCencInfoCount; i++) {
        uint8_t size = mCencDefaultSize ? mCencDefaultSize : mCencSizes[i];
        uint64_t offset = mCencOffsets.size() == 1 ? nextOffset : mCencOffsets[i];
        nextOffset = offset + size;

        auto& info = mCencInfo[i];

        if (size < IV_BYTES) {
            ALOGE("cenc aux info too small");
            return ERROR_MALFORMED;
        }

        if (mDataSource->readAt(offset, info.mIV, IV_BYTES) < IV_BYTES) {
            ALOGE("couldn't read init vector");
            return ERROR_IO;
        }
        offset += IV_BYTES;

        if (size == IV_BYTES) {
            info.mSubsampleCount = 0;
            continue;
        }

        if (size < IV_BYTES + sizeof(info.mSubsampleCount)) {
            ALOGE("subsample count overflows sample aux info buffer");
            return ERROR_MALFORMED;
        }

        if (!mDataSource->getUInt16(offset, &info.mSubsampleCount)) {
            ALOGE("error reading sample cenc info subsample count");
            return ERROR_IO;
        }
        offset += sizeof(info.mSubsampleCount);

        if (size < IV_BYTES + sizeof(info.mSubsampleCount) + info.mSubsampleCount * 6) {
            ALOGE("subsample descriptions overflow sample aux info buffer");
            return ERROR_MALFORMED;
        }

        info.mSubsamples = new SampleCencInfo::SubsampleSizes[info.mSubsampleCount];
        for (uint16_t j = 0; j < info.mSubsampleCount; j++) {
            auto& subsample = info.mSubsamples[j];
            if (!mDataSource->getUInt16(offset, &subsample.mClearBytes) ||
                !mDataSource->getUInt32(offset + sizeof(subsample.mClearBytes),
                                        &subsample.mCipherBytes)) {
                ALOGE("error reading cenc subsample aux info");
                return ERROR_IO;
            }
            offset += 6;
        }
    }

    return OK;
}

uint32_t SampleTable::countChunkOffsets() const {
    return mNumChunkOffsets;
}

uint32_t SampleTable::countSamples() const {
    return mNumSampleSizes;
}

status_t SampleTable::getMaxSampleSize(size_t *max_size) {
    Mutex::Autolock autoLock(mLock);

    *max_size = 0;

    for (uint32_t i = 0; i < mNumSampleSizes; ++i) {
        size_t sample_size;
        status_t err = getSampleSize_l(i, &sample_size);

        if (err != OK) {
            return err;
        }

        if (sample_size > *max_size) {
            *max_size = sample_size;
        }
    }

    return OK;
}

uint32_t abs_difference(uint32_t time1, uint32_t time2) {
    return time1 > time2 ? time1 - time2 : time2 - time1;
}

// static
int SampleTable::CompareIncreasingTime(const void *_a, const void *_b) {
    const SampleTimeEntry *a = (const SampleTimeEntry *)_a;
    const SampleTimeEntry *b = (const SampleTimeEntry *)_b;

    if (a->mCompositionTime < b->mCompositionTime) {
        return -1;
    } else if (a->mCompositionTime > b->mCompositionTime) {
        return 1;
    }

    return 0;
}

void SampleTable::buildSampleEntriesTable() {
    Mutex::Autolock autoLock(mLock);

    if (mSampleTimeEntries != NULL) {
        return;
    }

    mSampleTimeEntries = new SampleTimeEntry[mNumSampleSizes];

    uint32_t sampleIndex = 0;
    uint32_t sampleTime = 0;

    for (uint32_t i = 0; i < mTimeToSampleCount; ++i) {
        uint32_t n = mTimeToSample[2 * i];
        uint32_t delta = mTimeToSample[2 * i + 1];

        for (uint32_t j = 0; j < n; ++j) {
            if (sampleIndex < mNumSampleSizes) {
                // Technically this should always be the case if the file
                // is well-formed, but you know... there's (gasp) malformed
                // content out there.

                mSampleTimeEntries[sampleIndex].mSampleIndex = sampleIndex;

                uint32_t compTimeDelta =
                    mCompositionDeltaLookup->getCompositionTimeOffset(
                            sampleIndex);

                mSampleTimeEntries[sampleIndex].mCompositionTime =
                    sampleTime + compTimeDelta;
            }

            ++sampleIndex;
            sampleTime += delta;
        }
    }

    qsort(mSampleTimeEntries, mNumSampleSizes, sizeof(SampleTimeEntry),
          CompareIncreasingTime);
}

status_t SampleTable::findSampleAtTime(
        uint32_t req_time, uint32_t *sample_index, uint32_t flags) {
    buildSampleEntriesTable();

    uint32_t left = 0;
    uint32_t right = mNumSampleSizes;
    while (left < right) {
        uint32_t center = (left + right) / 2;
        uint32_t centerTime = mSampleTimeEntries[center].mCompositionTime;

        if (req_time < centerTime) {
            right = center;
        } else if (req_time > centerTime) {
            left = center + 1;
        } else {
            left = center;
            break;
        }
    }

    if (left == mNumSampleSizes) {
        if (flags == kFlagAfter) {
            return ERROR_OUT_OF_RANGE;
        }

        --left;
    }

    uint32_t closestIndex = left;

    switch (flags) {
        case kFlagBefore:
        {
            while (closestIndex > 0
                    && mSampleTimeEntries[closestIndex].mCompositionTime
                            > req_time) {
                --closestIndex;
            }
            break;
        }

        case kFlagAfter:
        {
            while (closestIndex + 1 < mNumSampleSizes
                    && mSampleTimeEntries[closestIndex].mCompositionTime
                            < req_time) {
                ++closestIndex;
            }
            break;
        }

        default:
        {
            CHECK(flags == kFlagClosest);

            if (closestIndex > 0) {
                // Check left neighbour and pick closest.
                uint32_t absdiff1 =
                    abs_difference(
                            mSampleTimeEntries[closestIndex].mCompositionTime,
                            req_time);

                uint32_t absdiff2 =
                    abs_difference(
                            mSampleTimeEntries[closestIndex - 1].mCompositionTime,
                            req_time);

                if (absdiff1 > absdiff2) {
                    closestIndex = closestIndex - 1;
                }
            }

            break;
        }
    }

    *sample_index = mSampleTimeEntries[closestIndex].mSampleIndex;

    return OK;
}

status_t SampleTable::findSyncSampleNear(
        uint32_t start_sample_index, uint32_t *sample_index, uint32_t flags) {
    Mutex::Autolock autoLock(mLock);

    *sample_index = 0;

    if (mSyncSampleOffset < 0) {
        // All samples are sync-samples.
        *sample_index = start_sample_index;
        return OK;
    }

    if (mNumSyncSamples == 0) {
        *sample_index = 0;
        return OK;
    }

    uint32_t left = 0;
    uint32_t right = mNumSyncSamples;
    while (left < right) {
        uint32_t center = left + (right - left) / 2;
        uint32_t x = mSyncSamples[center];

        if (start_sample_index < x) {
            right = center;
        } else if (start_sample_index > x) {
            left = center + 1;
        } else {
            left = center;
            break;
        }
    }
    if (left == mNumSyncSamples) {
        if (flags == kFlagAfter) {
            ALOGE("tried to find a sync frame after the last one: %d", left);
            return ERROR_OUT_OF_RANGE;
        }
        left = left - 1;
    }

    // Now ssi[left] is the sync sample index just before (or at)
    // start_sample_index.
    // Also start_sample_index < ssi[left + 1], if left + 1 < mNumSyncSamples.

    uint32_t x = mSyncSamples[left];

    if (left + 1 < mNumSyncSamples) {
        uint32_t y = mSyncSamples[left + 1];

        // our sample lies between sync samples x and y.

        status_t err = mSampleIterator->seekTo(start_sample_index);
        if (err != OK) {
            return err;
        }

        uint32_t sample_time = mSampleIterator->getSampleTime();

        err = mSampleIterator->seekTo(x);
        if (err != OK) {
            return err;
        }
        uint32_t x_time = mSampleIterator->getSampleTime();

        err = mSampleIterator->seekTo(y);
        if (err != OK) {
            return err;
        }

        uint32_t y_time = mSampleIterator->getSampleTime();

        if (abs_difference(x_time, sample_time)
                > abs_difference(y_time, sample_time)) {
            // Pick the sync sample closest (timewise) to the start-sample.
            x = y;
            ++left;
        }
    }

    switch (flags) {
        case kFlagBefore:
        {
            if (x > start_sample_index) {
                CHECK(left > 0);

                x = mSyncSamples[left - 1];

                if (x > start_sample_index) {
                    // The table of sync sample indices was not sorted
                    // properly.
                    return ERROR_MALFORMED;
                }
            }
            break;
        }

        case kFlagAfter:
        {
            if (x < start_sample_index) {
                if (left + 1 >= mNumSyncSamples) {
                    return ERROR_OUT_OF_RANGE;
                }

                x = mSyncSamples[left + 1];

                if (x < start_sample_index) {
                    // The table of sync sample indices was not sorted
                    // properly.
                    return ERROR_MALFORMED;
                }
            }

            break;
        }

        default:
            break;
    }

    *sample_index = x;

    return OK;
}

status_t SampleTable::findThumbnailSample(uint32_t *sample_index) {
    Mutex::Autolock autoLock(mLock);

    if (mSyncSampleOffset < 0) {
        // All samples are sync-samples.
        *sample_index = 0;
        return OK;
    }

    uint32_t bestSampleIndex = 0;
    size_t maxSampleSize = 0;

    static const size_t kMaxNumSyncSamplesToScan = 20;

    // Consider the first kMaxNumSyncSamplesToScan sync samples and
    // pick the one with the largest (compressed) size as the thumbnail.

    size_t numSamplesToScan = mNumSyncSamples;
    if (numSamplesToScan > kMaxNumSyncSamplesToScan) {
        numSamplesToScan = kMaxNumSyncSamplesToScan;
    }

    for (size_t i = 0; i < numSamplesToScan; ++i) {
        uint32_t x = mSyncSamples[i];

        // Now x is a sample index.
        size_t sampleSize;
        status_t err = getSampleSize_l(x, &sampleSize);
        if (err != OK) {
            return err;
        }

        if (i == 0 || sampleSize > maxSampleSize) {
            bestSampleIndex = x;
            maxSampleSize = sampleSize;
        }
    }

    *sample_index = bestSampleIndex;

    return OK;
}

status_t SampleTable::getSampleSize_l(
        uint32_t sampleIndex, size_t *sampleSize) {
    return mSampleIterator->getSampleSizeDirect(
            sampleIndex, sampleSize);
}

status_t SampleTable::getMetaDataForSample(
        uint32_t sampleIndex,
        off64_t *offset,
        size_t *size,
        uint32_t *compositionTime,
        uint32_t *duration,
        bool *isSyncSample,
        uint32_t *decodeTime) {
    Mutex::Autolock autoLock(mLock);

    status_t err;
    if ((err = mSampleIterator->seekTo(sampleIndex)) != OK) {
        return err;
    }

    if (offset) {
        *offset = mSampleIterator->getSampleOffset();
    }

    if (size) {
        *size = mSampleIterator->getSampleSize();
    }

    if (compositionTime) {
        *compositionTime = mSampleIterator->getSampleTime();
    }

    if (decodeTime) {
        *decodeTime = mSampleIterator->getSampleDecodeTime();
    }

    if (duration) {
        *duration = mSampleIterator->getSampleDuration();
    }

    if (isSyncSample) {
        *isSyncSample = false;
        if (mSyncSampleOffset < 0) {
            // Every sample is a sync sample.
            *isSyncSample = true;
        } else {
            size_t i = (mLastSyncSampleIndex < mNumSyncSamples)
                    && (mSyncSamples[mLastSyncSampleIndex] <= sampleIndex)
                ? mLastSyncSampleIndex : 0;

            while (i < mNumSyncSamples && mSyncSamples[i] < sampleIndex) {
                ++i;
            }

            if (i < mNumSyncSamples && mSyncSamples[i] == sampleIndex) {
                *isSyncSample = true;
            }

            mLastSyncSampleIndex = i;
        }
    }

    return OK;
}

uint32_t SampleTable::getCompositionTimeOffset(uint32_t sampleIndex) {
    return mCompositionDeltaLookup->getCompositionTimeOffset(sampleIndex);
}

status_t
SampleTable::getSampleCencInfo(
        uint32_t sample_index, Vector<uint16_t>& clear_sizes,
        Vector<uint32_t>& cipher_sizes, uint8_t iv[]) {
    CHECK(clear_sizes.isEmpty() && cipher_sizes.isEmpty());

    if (sample_index >= mCencInfoCount) {
        ALOGE("cenc info requested for out of range sample index");
        return ERROR_MALFORMED;
    }

    auto& info = mCencInfo[sample_index];
    clear_sizes.setCapacity(info.mSubsampleCount);
    cipher_sizes.setCapacity(info.mSubsampleCount);

    for (uint32_t i = 0; i < info.mSubsampleCount; i++) {
        clear_sizes.push(info.mSubsamples[i].mClearBytes);
        cipher_sizes.push(info.mSubsamples[i].mCipherBytes);
    }

    memcpy(iv, info.mIV, IV_BYTES);

    return OK;
}

}  // namespace stagefright

#undef LOG_TAG
