/*
 * Copyright 2014 The Android Open Source Project
 * Copyright (C) 2014 Mozilla Foundation
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

#include "GonkBufferSlot.h"

namespace android {

const char* GonkBufferSlot::bufferStateName(BufferState state) {
    switch (state) {
        case GonkBufferSlot::DEQUEUED: return "DEQUEUED";
        case GonkBufferSlot::QUEUED: return "QUEUED";
        case GonkBufferSlot::FREE: return "FREE";
        case GonkBufferSlot::ACQUIRED: return "ACQUIRED";
        default: return "Unknown";
    }
}

} // namespace android
