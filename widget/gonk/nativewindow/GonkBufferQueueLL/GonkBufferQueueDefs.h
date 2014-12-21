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

#ifndef NATIVEWINDOW_BUFFERQUEUECOREDEFS_H
#define NATIVEWINDOW_BUFFERQUEUECOREDEFS_H

#include "GonkBufferSlot.h"

namespace android {
    class GonkBufferQueueCore;

    namespace GonkBufferQueueDefs {
        // GonkBufferQueue will keep track of at most this value of buffers.
        // Attempts at runtime to increase the number of buffers past this
        // will fail.
        enum { NUM_BUFFER_SLOTS = 64 };

        typedef GonkBufferSlot SlotsType[NUM_BUFFER_SLOTS];
    } // namespace GonkBufferQueueDefs
} // namespace android

#endif
