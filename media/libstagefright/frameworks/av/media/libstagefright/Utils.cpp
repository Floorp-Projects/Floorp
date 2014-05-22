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
#define LOG_TAG "Utils"
#include <utils/Log.h>

#include "include/ESDS.h"

#include <arpa/inet.h>
#include <cutils/properties.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaDefs.h>
#include <media/AudioSystem.h>
#include <media/MediaPlayerInterface.h>
#include <hardware/audio.h>
#include <media/stagefright/Utils.h>
#include <media/AudioParameter.h>

namespace stagefright {

uint16_t U16_AT(const uint8_t *ptr) {
    return ptr[0] << 8 | ptr[1];
}

uint32_t U32_AT(const uint8_t *ptr) {
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}

uint64_t U64_AT(const uint8_t *ptr) {
    return ((uint64_t)U32_AT(ptr)) << 32 | U32_AT(ptr + 4);
}

uint16_t U16LE_AT(const uint8_t *ptr) {
    return ptr[0] | (ptr[1] << 8);
}

uint32_t U32LE_AT(const uint8_t *ptr) {
    return ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];
}

uint64_t U64LE_AT(const uint8_t *ptr) {
    return ((uint64_t)U32LE_AT(ptr + 4)) << 32 | U32LE_AT(ptr);
}

// XXX warning: these won't work on big-endian host.
uint64_t ntoh64(uint64_t x) {
    return ((uint64_t)ntohl(x & 0xffffffff) << 32) | ntohl(x >> 32);
}

uint64_t hton64(uint64_t x) {
    return ((uint64_t)htonl(x & 0xffffffff) << 32) | htonl(x >> 32);
}

#if 0
status_t convertMetaDataToMessage(
        const sp<MetaData> &meta, sp<AMessage> *format) {
    format->clear();

    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    sp<AMessage> msg = new AMessage;
    msg->setString("mime", mime);

    int64_t durationUs;
    if (meta->findInt64(kKeyDuration, &durationUs)) {
        msg->setInt64("durationUs", durationUs);
    }

    int32_t isSync;
    if (meta->findInt32(kKeyIsSyncFrame, &isSync) && isSync != 0) {
        msg->setInt32("is-sync-frame", 1);
    }

    if (!strncasecmp("video/", mime, 6)) {
        int32_t width, height;
        CHECK(meta->findInt32(kKeyWidth, &width));
        CHECK(meta->findInt32(kKeyHeight, &height));

        msg->setInt32("width", width);
        msg->setInt32("height", height);

        int32_t sarWidth, sarHeight;
        if (meta->findInt32(kKeySARWidth, &sarWidth)
                && meta->findInt32(kKeySARHeight, &sarHeight)) {
            msg->setInt32("sar-width", sarWidth);
            msg->setInt32("sar-height", sarHeight);
        }
    } else if (!strncasecmp("audio/", mime, 6)) {
        int32_t numChannels, sampleRate;
        CHECK(meta->findInt32(kKeyChannelCount, &numChannels));
        CHECK(meta->findInt32(kKeySampleRate, &sampleRate));

        msg->setInt32("channel-count", numChannels);
        msg->setInt32("sample-rate", sampleRate);

        int32_t channelMask;
        if (meta->findInt32(kKeyChannelMask, &channelMask)) {
            msg->setInt32("channel-mask", channelMask);
        }

        int32_t delay = 0;
        if (meta->findInt32(kKeyEncoderDelay, &delay)) {
            msg->setInt32("encoder-delay", delay);
        }
        int32_t padding = 0;
        if (meta->findInt32(kKeyEncoderPadding, &padding)) {
            msg->setInt32("encoder-padding", padding);
        }

        int32_t isADTS;
        if (meta->findInt32(kKeyIsADTS, &isADTS)) {
            msg->setInt32("is-adts", true);
        }
    }

    int32_t maxInputSize;
    if (meta->findInt32(kKeyMaxInputSize, &maxInputSize)) {
        msg->setInt32("max-input-size", maxInputSize);
    }

    uint32_t type;
    const void *data;
    size_t size;
    if (meta->findData(kKeyAVCC, &type, &data, &size)) {
        // Parse the AVCDecoderConfigurationRecord

        const uint8_t *ptr = (const uint8_t *)data;

        CHECK(size >= 7);
        CHECK_EQ((unsigned)ptr[0], 1u);  // configurationVersion == 1
        uint8_t profile = ptr[1];
        uint8_t level = ptr[3];

        // There is decodable content out there that fails the following
        // assertion, let's be lenient for now...
        // CHECK((ptr[4] >> 2) == 0x3f);  // reserved

        size_t lengthSize = 1 + (ptr[4] & 3);

        // commented out check below as H264_QVGA_500_NO_AUDIO.3gp
        // violates it...
        // CHECK((ptr[5] >> 5) == 7);  // reserved

        size_t numSeqParameterSets = ptr[5] & 31;

        ptr += 6;
        size -= 6;

        sp<ABuffer> buffer = new ABuffer(1024);
        buffer->setRange(0, 0);

        for (size_t i = 0; i < numSeqParameterSets; ++i) {
            CHECK(size >= 2);
            size_t length = U16_AT(ptr);

            ptr += 2;
            size -= 2;

            CHECK(size >= length);

            memcpy(buffer->data() + buffer->size(), "\x00\x00\x00\x01", 4);
            memcpy(buffer->data() + buffer->size() + 4, ptr, length);
            buffer->setRange(0, buffer->size() + 4 + length);

            ptr += length;
            size -= length;
        }

        buffer->meta()->setInt32("csd", true);
        buffer->meta()->setInt64("timeUs", 0);

        msg->setBuffer("csd-0", buffer);

        buffer = new ABuffer(1024);
        buffer->setRange(0, 0);

        CHECK(size >= 1);
        size_t numPictureParameterSets = *ptr;
        ++ptr;
        --size;

        for (size_t i = 0; i < numPictureParameterSets; ++i) {
            CHECK(size >= 2);
            size_t length = U16_AT(ptr);

            ptr += 2;
            size -= 2;

            CHECK(size >= length);

            memcpy(buffer->data() + buffer->size(), "\x00\x00\x00\x01", 4);
            memcpy(buffer->data() + buffer->size() + 4, ptr, length);
            buffer->setRange(0, buffer->size() + 4 + length);

            ptr += length;
            size -= length;
        }

        buffer->meta()->setInt32("csd", true);
        buffer->meta()->setInt64("timeUs", 0);
        msg->setBuffer("csd-1", buffer);
    } else if (meta->findData(kKeyESDS, &type, &data, &size)) {
        ESDS esds((const char *)data, size);
        CHECK_EQ(esds.InitCheck(), (status_t)OK);

        const void *codec_specific_data;
        size_t codec_specific_data_size;
        esds.getCodecSpecificInfo(
                &codec_specific_data, &codec_specific_data_size);

        sp<ABuffer> buffer = new ABuffer(codec_specific_data_size);

        memcpy(buffer->data(), codec_specific_data,
               codec_specific_data_size);

        buffer->meta()->setInt32("csd", true);
        buffer->meta()->setInt64("timeUs", 0);
        msg->setBuffer("csd-0", buffer);
    } else if (meta->findData(kKeyVorbisInfo, &type, &data, &size)) {
        sp<ABuffer> buffer = new ABuffer(size);
        memcpy(buffer->data(), data, size);

        buffer->meta()->setInt32("csd", true);
        buffer->meta()->setInt64("timeUs", 0);
        msg->setBuffer("csd-0", buffer);

        if (!meta->findData(kKeyVorbisBooks, &type, &data, &size)) {
            return -EINVAL;
        }

        buffer = new ABuffer(size);
        memcpy(buffer->data(), data, size);

        buffer->meta()->setInt32("csd", true);
        buffer->meta()->setInt64("timeUs", 0);
        msg->setBuffer("csd-1", buffer);
    }

    *format = msg;

    return OK;
}

static size_t reassembleAVCC(const sp<ABuffer> &csd0, const sp<ABuffer> csd1, char *avcc) {

    avcc[0] = 1;        // version
    avcc[1] = 0x64;     // profile
    avcc[2] = 0;        // unused (?)
    avcc[3] = 0xd;      // level
    avcc[4] = 0xff;     // reserved+size

    size_t i = 0;
    int numparams = 0;
    int lastparamoffset = 0;
    int avccidx = 6;
    do {
        if (i >= csd0->size() - 4 ||
                memcmp(csd0->data() + i, "\x00\x00\x00\x01", 4) == 0) {
            if (i >= csd0->size() - 4) {
                // there can't be another param here, so use all the rest
                i = csd0->size();
            }
            ALOGV("block at %d, last was %d", i, lastparamoffset);
            if (lastparamoffset > 0) {
                int size = i - lastparamoffset;
                avcc[avccidx++] = size >> 8;
                avcc[avccidx++] = size & 0xff;
                memcpy(avcc+avccidx, csd0->data() + lastparamoffset, size);
                avccidx += size;
                numparams++;
            }
            i += 4;
            lastparamoffset = i;
        } else {
            i++;
        }
    } while(i < csd0->size());
    ALOGV("csd0 contains %d params", numparams);

    avcc[5] = 0xe0 | numparams;
    //and now csd-1
    i = 0;
    numparams = 0;
    lastparamoffset = 0;
    int numpicparamsoffset = avccidx;
    avccidx++;
    do {
        if (i >= csd1->size() - 4 ||
                memcmp(csd1->data() + i, "\x00\x00\x00\x01", 4) == 0) {
            if (i >= csd1->size() - 4) {
                // there can't be another param here, so use all the rest
                i = csd1->size();
            }
            ALOGV("block at %d, last was %d", i, lastparamoffset);
            if (lastparamoffset > 0) {
                int size = i - lastparamoffset;
                avcc[avccidx++] = size >> 8;
                avcc[avccidx++] = size & 0xff;
                memcpy(avcc+avccidx, csd1->data() + lastparamoffset, size);
                avccidx += size;
                numparams++;
            }
            i += 4;
            lastparamoffset = i;
        } else {
            i++;
        }
    } while(i < csd1->size());
    avcc[numpicparamsoffset] = numparams;
    return avccidx;
}

static void reassembleESDS(const sp<ABuffer> &csd0, char *esds) {
    int csd0size = csd0->size();
    esds[0] = 3; // kTag_ESDescriptor;
    int esdescriptorsize = 26 + csd0size;
    CHECK(esdescriptorsize < 268435456); // 7 bits per byte, so max is 2^28-1
    esds[1] = 0x80 | (esdescriptorsize >> 21);
    esds[2] = 0x80 | ((esdescriptorsize >> 14) & 0x7f);
    esds[3] = 0x80 | ((esdescriptorsize >> 7) & 0x7f);
    esds[4] = (esdescriptorsize & 0x7f);
    esds[5] = esds[6] = 0; // es id
    esds[7] = 0; // flags
    esds[8] = 4; // kTag_DecoderConfigDescriptor
    int configdescriptorsize = 18 + csd0size;
    esds[9] = 0x80 | (configdescriptorsize >> 21);
    esds[10] = 0x80 | ((configdescriptorsize >> 14) & 0x7f);
    esds[11] = 0x80 | ((configdescriptorsize >> 7) & 0x7f);
    esds[12] = (configdescriptorsize & 0x7f);
    esds[13] = 0x40; // objectTypeIndication
    esds[14] = 0x15; // not sure what 14-25 mean, they are ignored by ESDS.cpp,
    esds[15] = 0x00; // but the actual values here were taken from a real file.
    esds[16] = 0x18;
    esds[17] = 0x00;
    esds[18] = 0x00;
    esds[19] = 0x00;
    esds[20] = 0xfa;
    esds[21] = 0x00;
    esds[22] = 0x00;
    esds[23] = 0x00;
    esds[24] = 0xfa;
    esds[25] = 0x00;
    esds[26] = 5; // kTag_DecoderSpecificInfo;
    esds[27] = 0x80 | (csd0size >> 21);
    esds[28] = 0x80 | ((csd0size >> 14) & 0x7f);
    esds[29] = 0x80 | ((csd0size >> 7) & 0x7f);
    esds[30] = (csd0size & 0x7f);
    memcpy((void*)&esds[31], csd0->data(), csd0size);
    // data following this is ignored, so don't bother appending it

}

void convertMessageToMetaData(const sp<AMessage> &msg, sp<MetaData> &meta) {
    AString mime;
    if (msg->findString("mime", &mime)) {
        meta->setCString(kKeyMIMEType, mime.c_str());
    } else {
        ALOGW("did not find mime type");
    }

    int64_t durationUs;
    if (msg->findInt64("durationUs", &durationUs)) {
        meta->setInt64(kKeyDuration, durationUs);
    }

    int32_t isSync;
    if (msg->findInt32("is-sync-frame", &isSync) && isSync != 0) {
        meta->setInt32(kKeyIsSyncFrame, 1);
    }

    if (mime.startsWith("video/")) {
        int32_t width;
        int32_t height;
        if (msg->findInt32("width", &width) && msg->findInt32("height", &height)) {
            meta->setInt32(kKeyWidth, width);
            meta->setInt32(kKeyHeight, height);
        } else {
            ALOGW("did not find width and/or height");
        }

        int32_t sarWidth, sarHeight;
        if (msg->findInt32("sar-width", &sarWidth)
                && msg->findInt32("sar-height", &sarHeight)) {
            meta->setInt32(kKeySARWidth, sarWidth);
            meta->setInt32(kKeySARHeight, sarHeight);
        }
    } else if (mime.startsWith("audio/")) {
        int32_t numChannels;
        if (msg->findInt32("channel-count", &numChannels)) {
            meta->setInt32(kKeyChannelCount, numChannels);
        }
        int32_t sampleRate;
        if (msg->findInt32("sample-rate", &sampleRate)) {
            meta->setInt32(kKeySampleRate, sampleRate);
        }
        int32_t channelMask;
        if (msg->findInt32("channel-mask", &channelMask)) {
            meta->setInt32(kKeyChannelMask, channelMask);
        }
        int32_t delay = 0;
        if (msg->findInt32("encoder-delay", &delay)) {
            meta->setInt32(kKeyEncoderDelay, delay);
        }
        int32_t padding = 0;
        if (msg->findInt32("encoder-padding", &padding)) {
            meta->setInt32(kKeyEncoderPadding, padding);
        }

        int32_t isADTS;
        if (msg->findInt32("is-adts", &isADTS)) {
            meta->setInt32(kKeyIsADTS, isADTS);
        }
    }

    int32_t maxInputSize;
    if (msg->findInt32("max-input-size", &maxInputSize)) {
        meta->setInt32(kKeyMaxInputSize, maxInputSize);
    }

    // reassemble the csd data into its original form
    sp<ABuffer> csd0;
    if (msg->findBuffer("csd-0", &csd0)) {
        if (mime.startsWith("video/")) { // do we need to be stricter than this?
            sp<ABuffer> csd1;
            if (msg->findBuffer("csd-1", &csd1)) {
                char avcc[1024]; // that oughta be enough, right?
                size_t outsize = reassembleAVCC(csd0, csd1, avcc);
                meta->setData(kKeyAVCC, kKeyAVCC, avcc, outsize);
            }
        } else if (mime.startsWith("audio/")) {
            int csd0size = csd0->size();
            char esds[csd0size + 31];
            reassembleESDS(csd0, esds);
            meta->setData(kKeyESDS, kKeyESDS, esds, sizeof(esds));
        }
    }

    // XXX TODO add whatever other keys there are

#if 0
    ALOGI("converted %s to:", msg->debugString(0).c_str());
    meta->dumpToLog();
#endif
}

AString MakeUserAgent() {
    AString ua;
    ua.append("stagefright/1.2 (Linux;Android ");

#if (PROPERTY_VALUE_MAX < 8)
#error "PROPERTY_VALUE_MAX must be at least 8"
#endif

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.build.version.release", value, "Unknown");
    ua.append(value);
    ua.append(")");

    return ua;
}

status_t sendMetaDataToHal(sp<MediaPlayerBase::AudioSink>& sink,
                           const sp<MetaData>& meta)
{
    int32_t sampleRate = 0;
    int32_t bitRate = 0;
    int32_t channelMask = 0;
    int32_t delaySamples = 0;
    int32_t paddingSamples = 0;

    AudioParameter param = AudioParameter();

    if (meta->findInt32(kKeySampleRate, &sampleRate)) {
        param.addInt(String8(AUDIO_OFFLOAD_CODEC_SAMPLE_RATE), sampleRate);
    }
    if (meta->findInt32(kKeyChannelMask, &channelMask)) {
        param.addInt(String8(AUDIO_OFFLOAD_CODEC_NUM_CHANNEL), channelMask);
    }
    if (meta->findInt32(kKeyBitRate, &bitRate)) {
        param.addInt(String8(AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE), bitRate);
    }
    if (meta->findInt32(kKeyEncoderDelay, &delaySamples)) {
        param.addInt(String8(AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES), delaySamples);
    }
    if (meta->findInt32(kKeyEncoderPadding, &paddingSamples)) {
        param.addInt(String8(AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES), paddingSamples);
    }

    ALOGV("sendMetaDataToHal: bitRate %d, sampleRate %d, chanMask %d,"
          "delaySample %d, paddingSample %d", bitRate, sampleRate,
          channelMask, delaySamples, paddingSamples);

    sink->setParameters(param.toString());
    return OK;
}

struct mime_conv_t {
    const char* mime;
    audio_format_t format;
};

static const struct mime_conv_t mimeLookup[] = {
    { MEDIA_MIMETYPE_AUDIO_MPEG,        AUDIO_FORMAT_MP3 },
    { MEDIA_MIMETYPE_AUDIO_RAW,         AUDIO_FORMAT_PCM_16_BIT },
    { MEDIA_MIMETYPE_AUDIO_AMR_NB,      AUDIO_FORMAT_AMR_NB },
    { MEDIA_MIMETYPE_AUDIO_AMR_WB,      AUDIO_FORMAT_AMR_WB },
    { MEDIA_MIMETYPE_AUDIO_AAC,         AUDIO_FORMAT_AAC },
    { MEDIA_MIMETYPE_AUDIO_VORBIS,      AUDIO_FORMAT_VORBIS },
    { 0, AUDIO_FORMAT_INVALID }
};

status_t mapMimeToAudioFormat( audio_format_t& format, const char* mime )
{
const struct mime_conv_t* p = &mimeLookup[0];
    while (p->mime != NULL) {
        if (0 == strcasecmp(mime, p->mime)) {
            format = p->format;
            return OK;
        }
        ++p;
    }

    return BAD_VALUE;
}

bool canOffloadStream(const sp<MetaData>& meta, bool hasVideo, bool isStreaming)
{
    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    audio_offload_info_t info = AUDIO_INFO_INITIALIZER;

    info.format = AUDIO_FORMAT_INVALID;
    if (mapMimeToAudioFormat(info.format, mime) != OK) {
        ALOGE(" Couldn't map mime type \"%s\" to a valid AudioSystem::audio_format !", mime);
        return false;
    } else {
        ALOGV("Mime type \"%s\" mapped to audio_format %d", mime, info.format);
    }

    if (AUDIO_FORMAT_INVALID == info.format) {
        // can't offload if we don't know what the source format is
        ALOGE("mime type \"%s\" not a known audio format", mime);
        return false;
    }

    int32_t srate = -1;
    if (!meta->findInt32(kKeySampleRate, &srate)) {
        ALOGV("track of type '%s' does not publish sample rate", mime);
    }
    info.sample_rate = srate;

    int32_t cmask = 0;
    if (!meta->findInt32(kKeyChannelMask, &cmask)) {
        ALOGV("track of type '%s' does not publish channel mask", mime);

        // Try a channel count instead
        int32_t channelCount;
        if (!meta->findInt32(kKeyChannelCount, &channelCount)) {
            ALOGV("track of type '%s' does not publish channel count", mime);
        } else {
            cmask = audio_channel_out_mask_from_count(channelCount);
        }
    }
    info.channel_mask = cmask;

    int64_t duration = 0;
    if (!meta->findInt64(kKeyDuration, &duration)) {
        ALOGV("track of type '%s' does not publish duration", mime);
    }
    info.duration_us = duration;

    int32_t brate = -1;
    if (!meta->findInt32(kKeyBitRate, &brate)) {
        ALOGV("track of type '%s' does not publish bitrate", mime);
     }
    info.bit_rate = brate;


    info.stream_type = AUDIO_STREAM_MUSIC;
    info.has_video = hasVideo;
    info.is_streaming = isStreaming;

    // Check if offload is possible for given format, stream type, sample rate,
    // bit rate, duration, video and streaming
    return AudioSystem::isOffloadSupported(info);
}

#endif

}  // namespace stagefright

#undef LOG_TAG
