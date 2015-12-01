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

#ifndef MEDIA_SOURCE_H_

#define MEDIA_SOURCE_H_

#include <sys/types.h>

#include <media/stagefright/MediaErrors.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>
#include "nsTArray.h"

namespace stagefright {

class MediaBuffer;
class MetaData;

struct MediaSource : public virtual RefBase {
    MediaSource();

    // Returns the format of the data output by this media source.
    virtual sp<MetaData> getFormat() = 0;

    struct Indice
    {
      uint64_t start_offset;
      uint64_t end_offset;
      uint64_t start_composition;
      uint64_t end_composition;
      uint64_t start_decode;
      bool sync;
    };

    virtual nsTArray<Indice> exportIndex() = 0;

protected:
    virtual ~MediaSource();

private:
    MediaSource(const MediaSource &);
    MediaSource &operator=(const MediaSource &);
};

}  // namespace stagefright

#endif  // MEDIA_SOURCE_H_
