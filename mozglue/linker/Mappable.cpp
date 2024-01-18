/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <sys/stat.h>

#include "Mappable.h"

Mappable* Mappable::Create(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd != -1) return new Mappable(fd);
  return nullptr;
}

MemoryRange Mappable::mmap(const void* addr, size_t length, int prot, int flags,
                           off_t offset) {
  MOZ_ASSERT(fd != -1);
  MOZ_ASSERT(!(flags & MAP_SHARED));
  flags |= MAP_PRIVATE;

  return MemoryRange::mmap(const_cast<void*>(addr), length, prot, flags, fd,
                           offset);
}

void Mappable::finalize() {
  /* Close file ; equivalent to close(fd.forget()) */
  fd = -1;
}

size_t Mappable::GetLength() const {
  struct stat st;
  return fstat(fd, &st) ? 0 : st.st_size;
}
