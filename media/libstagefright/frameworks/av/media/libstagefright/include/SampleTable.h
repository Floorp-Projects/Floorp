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

#ifndef SAMPLE_TABLE_H_

#define SAMPLE_TABLE_H_

#include <sys/types.h>
#include <stdint.h>

#include <media/stagefright/MediaErrors.h>
#include <utils/RefBase.h>
#include <utils/threads.h>

namespace stagefright {

class DataSource;
struct SampleIterator;

class SampleTable : public RefBase {
public:
    SampleTable(const sp<DataSource> &source);

    bool isValid() const;

    // type can be 'stco' or 'co64'.
    status_t setChunkOffsetParams(
            uint32_t type, off64_t data_offset, size_t data_size);

    status_t setSampleToChunkParams(off64_t data_offset, size_t data_size);

    // type can be 'stsz' or 'stz2'.
    status_t setSampleSizeParams(
            uint32_t type, off64_t data_offset, size_t data_size);

    status_t setTimeToSampleParams(off64_t data_offset, size_t data_size);

    status_t setCompositionTimeToSampleParams(
            off64_t data_offset, size_t data_size);

    status_t setSyncSampleParams(off64_t data_offset, size_t data_size);

    status_t setSampleAuxiliaryInformationSizeParams(off64_t aDataOffset,
                                                     size_t aDataSize,
                                                     uint32_t aDrmScheme);

    status_t setSampleAuxiliaryInformationOffsetParams(off64_t aDataOffset,
                                                       size_t aDataSize,
                                                       uint32_t aDrmScheme);

    ////////////////////////////////////////////////////////////////////////////

    uint32_t countChunkOffsets() const;

    uint32_t countSamples() const;

    status_t getMaxSampleSize(size_t *size);

    status_t getMetaDataForSample(
            uint32_t sampleIndex,
            off64_t *offset,
            size_t *size,
            uint32_t *compositionTime,
            uint32_t *duration = NULL,
            bool *isSyncSample = NULL,
            uint32_t *decodeTime = NULL);

    enum {
        kFlagBefore,
        kFlagAfter,
        kFlagClosest
    };
    status_t findSampleAtTime(
            uint32_t req_time, uint32_t *sample_index, uint32_t flags);

    status_t findSyncSampleNear(
            uint32_t start_sample_index, uint32_t *sample_index,
            uint32_t flags);

    status_t findThumbnailSample(uint32_t *sample_index);

    bool hasCencInfo() const { return !!mCencInfo; }

    status_t getSampleCencInfo(uint32_t aSampleIndex,
                               nsTArray<uint16_t>& aClearSizes,
                               nsTArray<uint32_t>& aCipherSizes,
                               uint8_t aIV[]);

protected:
    ~SampleTable();

private:
    struct CompositionDeltaLookup;

    static const uint32_t kChunkOffsetType32;
    static const uint32_t kChunkOffsetType64;
    static const uint32_t kSampleSizeType32;
    static const uint32_t kSampleSizeTypeCompact;

    sp<DataSource> mDataSource;
    Mutex mLock;

    off64_t mChunkOffsetOffset;
    uint32_t mChunkOffsetType;
    uint32_t mNumChunkOffsets;

    off64_t mSampleToChunkOffset;
    uint32_t mNumSampleToChunkOffsets;

    off64_t mSampleSizeOffset;
    uint32_t mSampleSizeFieldSize;
    uint32_t mDefaultSampleSize;
    uint32_t mNumSampleSizes;

    uint32_t mTimeToSampleCount;
    uint32_t *mTimeToSample;

    struct SampleTimeEntry {
        uint32_t mSampleIndex;
        uint32_t mCompositionTime;
    };
    SampleTimeEntry *mSampleTimeEntries;

    uint32_t *mCompositionTimeDeltaEntries;
    size_t mNumCompositionTimeDeltaEntries;
    CompositionDeltaLookup *mCompositionDeltaLookup;

    off64_t mSyncSampleOffset;
    uint32_t mNumSyncSamples;
    uint32_t *mSyncSamples;
    size_t mLastSyncSampleIndex;

    SampleIterator *mSampleIterator;

    struct SampleToChunkEntry {
        uint32_t startChunk;
        uint32_t samplesPerChunk;
        uint32_t chunkDesc;
    };
    SampleToChunkEntry *mSampleToChunkEntries;

    enum { IV_BYTES = 16 };
    struct SampleCencInfo {
        uint8_t mIV[IV_BYTES];
        uint16_t mSubsampleCount;

        struct SubsampleSizes {
            uint16_t mClearBytes;
            uint32_t mCipherBytes;
        } * mSubsamples;
    } * mCencInfo;
    uint32_t mCencInfoCount;

    uint8_t mCencDefaultSize;
    FallibleTArray<uint8_t> mCencSizes;
    FallibleTArray<uint64_t> mCencOffsets;

    friend struct SampleIterator;

    status_t getSampleSize_l(uint32_t sample_index, size_t *sample_size);
    uint32_t getCompositionTimeOffset(uint32_t sampleIndex);

    static int CompareIncreasingTime(const void *, const void *);

    status_t buildSampleEntriesTable();

    status_t parseSampleCencInfo();

    SampleTable(const SampleTable &);
    SampleTable &operator=(const SampleTable &);
};

}  // namespace stagefright

#endif  // SAMPLE_TABLE_H_
