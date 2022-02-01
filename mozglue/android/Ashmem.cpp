/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <dlfcn.h>
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

static void* libhandle() {
  static void* handle = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
  return handle;
}

int ashmem_create(const char* name, size_t size) {
  static auto fCreate =
      (int (*)(const char*, size_t))dlsym(libhandle(), "ASharedMemory_create");

  if (fCreate) {
    return fCreate(name, size);
  }

  int fd = open("/" ASHMEM_NAME_DEF, O_RDWR);
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
  static auto fGetSize =
      (size_t(*)(int))dlsym(libhandle(), "ASharedMemory_getSize");
  if (fGetSize) {
    return fGetSize(fd);
  }

  return (size_t)ioctl(fd, ASHMEM_GET_SIZE, nullptr);
}

int ashmem_setProt(int fd, int prot) {
  static auto fSetProt =
      (int (*)(int, int))dlsym(libhandle(), "ASharedMemory_setProt");
  if (fSetProt) {
    return fSetProt(fd, prot);
  }

  return ioctl(fd, ASHMEM_SET_PROT_MASK, prot);
}

}  // namespace android
}  // namespace mozilla
