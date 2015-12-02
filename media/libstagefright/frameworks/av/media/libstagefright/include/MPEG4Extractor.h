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

#ifndef MPEG4_EXTRACTOR_H_

#define MPEG4_EXTRACTOR_H_

#include <arpa/inet.h>

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/Utils.h>
#include <utils/List.h>
#include <utils/String8.h>
#include "nsTArray.h"

namespace stagefright {

struct AMessage;
class DataSource;
class SampleTable;
class String8;

struct SidxEntry {
    size_t mSize;
    uint32_t mDurationUs;
};

class MPEG4Extractor : public MediaExtractor {
public:
    MPEG4Extractor(const sp<DataSource> &source);

    size_t countTracks() override;
    sp<MediaSource> getTrack(size_t index) override;
    sp<MetaData> getTrackMetaData(size_t index, uint32_t flags) override;

    sp<MetaData> getMetaData() override;
    uint32_t flags() const override;

    // for DRM
    char* getDrmTrackInfo(size_t trackID, int *len) override;

protected:
    virtual ~MPEG4Extractor();

private:

    struct PsshInfo {
        uint8_t uuid[16];
        uint32_t datalen;
        uint8_t *data;
    };
    struct Track {
        Track *next;
        sp<MetaData> meta;
        uint32_t timescale;
        // Temporary storage for elst until we've
        // parsed mdhd and can interpret them.
        uint64_t empty_duration;
        uint64_t segment_duration;
        int64_t media_time;

        sp<SampleTable> sampleTable;
        bool includes_expensive_metadata;
        bool skipTrack;
    };

    nsTArray<SidxEntry> mSidxEntries;
    uint64_t mSidxDuration;

    nsTArray<PsshInfo> mPssh;

    sp<DataSource> mDataSource;
    status_t mInitCheck;
    bool mHasVideo;
    uint32_t mHeaderTimescale;

    Track *mFirstTrack, *mLastTrack;

    sp<MetaData> mFileMetaData;

    nsTArray<uint32_t> mPath;
    String8 mLastCommentMean;
    String8 mLastCommentName;
    String8 mLastCommentData;

    status_t readMetaData();
    status_t parseChunk(off64_t *offset, int depth);
    status_t parseMetaData(off64_t offset, size_t size);

    status_t updateAudioTrackInfoFromESDS_MPEG4Audio(
            const void *esds_data, size_t esds_size);

    static status_t verifyTrack(Track *track);

    struct SINF {
        SINF *next;
        uint16_t trackID;
        uint8_t IPMPDescriptorID;
        ssize_t len;
        char *IPMPData;
    };

    SINF *mFirstSINF;

    bool mIsDrm;
    uint32_t mDrmScheme;

    status_t parseDrmSINF(off64_t *offset, off64_t data_offset);

    status_t parseTrackHeader(off64_t data_offset, off64_t data_size);

    status_t parseSegmentIndex(off64_t data_offset, size_t data_size);

    void storeEditList();

    MPEG4Extractor(const MPEG4Extractor &);
    MPEG4Extractor &operator=(const MPEG4Extractor &);
};

}  // namespace stagefright

#endif  // MPEG4_EXTRACTOR_H_
