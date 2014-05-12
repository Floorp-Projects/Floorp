/*
 * Copyright 2007 The Android Open Source Project
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

#ifndef ANDROID_PIXELFLINGER_ERRORS_H
#define ANDROID_PIXELFLINGER_ERRORS_H

#include <sys/types.h>
#include <errno.h>

namespace android {
namespace tinyutils {

// use this type to return error codes
typedef int32_t     status_t;

/*
 * Error codes. 
 * All error codes are negative values.
 */

enum {
    NO_ERROR          = 0,    // No errors.
    NO_MEMORY           = -ENOMEM,
    BAD_VALUE           = -EINVAL,
    BAD_INDEX           = -EOVERFLOW,
    NAME_NOT_FOUND      = -ENOENT,
};


} // namespace tinyutils
} // namespace android
    
// ---------------------------------------------------------------------------
    
#endif // ANDROID_PIXELFLINGER_ERRORS_H
