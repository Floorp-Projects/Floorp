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

#ifndef MEDIA_ERRORS_H_

#define MEDIA_ERRORS_H_

#include <utils/Errors.h>

namespace stagefright {

#define MEDIA_ERROR_BASE (-1000)

#define ERROR_ALREADY_CONNECTED (MEDIA_ERROR_BASE)
#define ERROR_NOT_CONNECTED     (MEDIA_ERROR_BASE - 1)
#define ERROR_UNKNOWN_HOST      (MEDIA_ERROR_BASE - 2)
#define ERROR_CANNOT_CONNECT    (MEDIA_ERROR_BASE - 3)
#define ERROR_IO                (MEDIA_ERROR_BASE - 4)
#define ERROR_CONNECTION_LOST   (MEDIA_ERROR_BASE - 5)
#define ERROR_MALFORMED         (MEDIA_ERROR_BASE - 7)
#define ERROR_OUT_OF_RANGE      (MEDIA_ERROR_BASE - 8)
#define ERROR_BUFFER_TOO_SMALL  (MEDIA_ERROR_BASE - 9)
#define ERROR_UNSUPPORTED       (MEDIA_ERROR_BASE - 10)
#define ERROR_END_OF_STREAM     (MEDIA_ERROR_BASE - 11)

// Not technically an error.
#define INFO_FORMAT_CHANGED    (MEDIA_ERROR_BASE - 12)
#define INFO_DISCONTINUITY     (MEDIA_ERROR_BASE - 13)
#define INFO_OUTPUT_BUFFERS_CHANGED (MEDIA_ERROR_BASE - 14)

// The following constant values should be in sync with
// drm/drm_framework_common.h
#define DRM_ERROR_BASE (-2000)

#define ERROR_DRM_UNKNOWN                       (DRM_ERROR_BASE)
#define ERROR_DRM_NO_LICENSE                    (DRM_ERROR_BASE - 1)
#define ERROR_DRM_LICENSE_EXPIRED               (DRM_ERROR_BASE - 2)
#define ERROR_DRM_SESSION_NOT_OPENED            (DRM_ERROR_BASE - 3)
#define ERROR_DRM_DECRYPT_UNIT_NOT_INITIALIZED  (DRM_ERROR_BASE - 4)
#define ERROR_DRM_DECRYPT                       (DRM_ERROR_BASE - 5)
#define ERROR_DRM_CANNOT_HANDLE                 (DRM_ERROR_BASE - 6)
#define ERROR_DRM_TAMPER_DETECTED               (DRM_ERROR_BASE - 7)
#define ERROR_DRM_NOT_PROVISIONED               (DRM_ERROR_BASE - 8)
#define ERROR_DRM_DEVICE_REVOKED                (DRM_ERROR_BASE - 9)
#define ERROR_DRM_RESOURCE_BUSY                 (DRM_ERROR_BASE - 10)

#define ERROR_DRM_VENDOR_MAX                    (DRM_ERROR_BASE - 500)
#define ERROR_DRM_VENDOR_MIN                    (DRM_ERROR_BASE - 999)

// Heartbeat Error Codes
#define HEARTBEAT_ERROR_BASE (-3000)
#define ERROR_HEARTBEAT_TERMINATE_REQUESTED     (HEARTBEAT_ERROR_BASE)

}  // namespace stagefright

#endif  // MEDIA_ERRORS_H_
