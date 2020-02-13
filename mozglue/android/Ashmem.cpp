/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <fcntl.h>
#include <linux/ashmem.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "Ashmem.h"

namespace mozilla {
namespace android {

// Use weak symbols for these, since we may not be running
// on a device that has them.
MOZ_EXPORT extern "C" int __attribute__((weak))
ASharedMemory_create(const char*, size_t);
MOZ_EXPORT extern "C" size_t __attribute__((weak)) ASharedMemory_getSize(int);
MOZ_EXPORT extern "C" int __attribute__((weak)) ASharedMemory_setProt(int, int);

int ashmem_create(const char* name, size_t size) {
  if (ASharedMemory_create) {
    return ASharedMemory_create(name, size);
  }

  int fd = open("/" ASHMEM_NAME_DEF, O_RDWR, 0600);
  if (fd < 0) {
    return fd;
  }

  if (name) {
    char str[ASHMEM_NAME_LEN];
    strlcpy(str, name, sizeof(str));
    ioctl(fd, ASHMEM_SET_NAME, str);
  }

  if (ioctl(fd, ASHMEM_SET_SIZE, size) != 0) {
    close(fd);
    return -1;
  }

  return fd;
}

size_t ashmem_getSize(int fd) {
  if (ASharedMemory_getSize) {
    return ASharedMemory_getSize(fd);
  }

  return (size_t)ioctl(fd, ASHMEM_GET_SIZE, nullptr);
}

int ashmem_setProt(int fd, int prot) {
  if (ASharedMemory_setProt) {
    return ASharedMemory_setProt(fd, prot);
  }

  return ioctl(fd, ASHMEM_SET_PROT_MASK, prot);
}

}  // namespace android
}  // namespace mozilla