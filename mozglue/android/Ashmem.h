/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Ashmem_h__
#define Ashmem_h__

#include <linux/ashmem.h>
#include "mozilla/Types.h"

namespace mozilla {
namespace android {

// Wrappers for the ASharedMemory function in the NDK
// https://developer.android.com/ndk/reference/group/memory
MFBT_API int ashmem_create(const char* name, size_t size);
MFBT_API size_t ashmem_getSize(int fd);
MFBT_API int ashmem_setProt(int fd, int prot);

}  // namespace android
}  // namespace mozilla

#endif  // Ashmem_h__
