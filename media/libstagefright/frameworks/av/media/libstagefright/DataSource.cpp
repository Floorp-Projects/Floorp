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

#include "include/AMRExtractor.h"

#if CHROMIUM_AVAILABLE
#include "include/chromium_http_stub.h"
#endif

#include "include/MPEG4Extractor.h"

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaErrors.h>

namespace stagefright {

bool DataSource::getUInt16(off64_t offset, uint16_t *x) {
    *x = 0;

    uint8_t byte[2];
    if (readAt(offset, byte, 2) != 2) {
        return false;
    }

    *x = (byte[0] << 8) | byte[1];

    return true;
}

bool DataSource::getUInt24(off64_t offset, uint32_t *x) {
    *x = 0;

    uint8_t byte[3];
    if (readAt(offset, byte, 3) != 3) {
        return false;
    }

    *x = (byte[0] << 16) | (byte[1] << 8) | byte[2];

    return true;
}

bool DataSource::getUInt32(off64_t offset, uint32_t *x) {
    *x = 0;

    uint32_t tmp;
    if (readAt(offset, &tmp, 4) != 4) {
        return false;
    }

    *x = ntohl(tmp);

    return true;
}

bool DataSource::getUInt64(off64_t offset, uint64_t *x) {
    *x = 0;

    uint64_t tmp;
    if (readAt(offset, &tmp, 8) != 8) {
        return false;
    }

    *x = ntoh64(tmp);

    return true;
}

status_t DataSource::getSize(off64_t *size) {
    *size = 0;

    return ERROR_UNSUPPORTED;
}

////////////////////////////////////////////////////////////////////////////////

#if 0

Mutex DataSource::gSnifferMutex;
List<DataSource::SnifferFunc> DataSource::gSniffers;

bool DataSource::sniff(
        String8 *mimeType, float *confidence, sp<AMessage> *meta) {
    *mimeType = "";
    *confidence = 0.0f;
    meta->clear();

    Mutex::Autolock autoLock(gSnifferMutex);
    for (List<SnifferFunc>::iterator it = gSniffers.begin();
         it != gSniffers.end(); ++it) {
        String8 newMimeType;
        float newConfidence;
        sp<AMessage> newMeta;
        if ((*it)(this, &newMimeType, &newConfidence, &newMeta)) {
            if (newConfidence > *confidence) {
                *mimeType = newMimeType;
                *confidence = newConfidence;
                *meta = newMeta;
            }
        }
    }

    return *confidence > 0.0;
}

// static
void DataSource::RegisterSniffer(SnifferFunc func) {
    Mutex::Autolock autoLock(gSnifferMutex);

    for (List<SnifferFunc>::iterator it = gSniffers.begin();
         it != gSniffers.end(); ++it) {
        if (*it == func) {
            return;
        }
    }

    gSniffers.push_back(func);
}

// static
void DataSource::RegisterDefaultSniffers() {
    RegisterSniffer(SniffMPEG4);
    RegisterSniffer(SniffMatroska);
    RegisterSniffer(SniffOgg);
    RegisterSniffer(SniffWAV);
    RegisterSniffer(SniffFLAC);
    RegisterSniffer(SniffAMR);
    RegisterSniffer(SniffMPEG2TS);
    RegisterSniffer(SniffMP3);
    RegisterSniffer(SniffAAC);
    RegisterSniffer(SniffMPEG2PS);
    RegisterSniffer(SniffWVM);

    char value[PROPERTY_VALUE_MAX];
    if (property_get("drm.service.enabled", value, NULL)
            && (!strcmp(value, "1") || !strcasecmp(value, "true"))) {
        RegisterSniffer(SniffDRM);
    }
}

// static
sp<DataSource> DataSource::CreateFromURI(
        const char *uri, const KeyedVector<String8, String8> *headers) {
    bool isWidevine = !strncasecmp("widevine://", uri, 11);

    sp<DataSource> source;
    if (!strncasecmp("file://", uri, 7)) {
        source = new FileSource(uri + 7);
    } else if (!strncasecmp("http://", uri, 7)
            || !strncasecmp("https://", uri, 8)
            || isWidevine) {
        sp<HTTPBase> httpSource = HTTPBase::Create();

        String8 tmp;
        if (isWidevine) {
            tmp = String8("http://");
            tmp.append(uri + 11);

            uri = tmp.string();
        }

        if (httpSource->connect(uri, headers) != OK) {
            return NULL;
        }

        if (!isWidevine) {
            String8 cacheConfig;
            bool disconnectAtHighwatermark;
            if (headers != NULL) {
                KeyedVector<String8, String8> copy = *headers;
                NuCachedSource2::RemoveCacheSpecificHeaders(
                        &copy, &cacheConfig, &disconnectAtHighwatermark);
            }

            source = new NuCachedSource2(
                    httpSource,
                    cacheConfig.isEmpty() ? NULL : cacheConfig.string());
        } else {
            // We do not want that prefetching, caching, datasource wrapper
            // in the widevine:// case.
            source = httpSource;
        }

# if CHROMIUM_AVAILABLE
    } else if (!strncasecmp("data:", uri, 5)) {
        source = createDataUriSource(uri);
#endif
    } else {
        // Assume it's a filename.
        source = new FileSource(uri);
    }

    if (source == NULL || source->initCheck() != OK) {
        return NULL;
    }

    return source;
}

#endif

String8 DataSource::getMIMEType() const {
    return String8("application/octet-stream");
}

}  // namespace stagefright
