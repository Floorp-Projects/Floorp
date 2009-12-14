// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// file_id.cc: Return a unique identifier for a file
//
// See file_id.h for documentation
//

#include "common/linux/file_id.h"

#include <arpa/inet.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>

namespace google_breakpad {

FileID::FileID(const char* path) {
  strncpy(path_, path, sizeof(path_));
}

bool FileID::ElfFileIdentifier(uint8_t identifier[kMDGUIDSize]) {
  const ssize_t mapped_len = 4096;  // Page size (matches WriteMappings())
  int fd = open(path_, O_RDONLY);
  if (fd < 0)
    return false;
  struct stat st;
  if (fstat(fd, &st) != 0 || st.st_size <= mapped_len) {
    close(fd);
    return false;
  }
  void* base = mmap(NULL, mapped_len,
                    PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  close(fd);
  if (base == MAP_FAILED)
    return false;

  memset(identifier, 0, kMDGUIDSize);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(base);
  uint8_t* ptr_end = ptr + mapped_len;
  while (ptr < ptr_end) {
    for (unsigned i = 0; i < kMDGUIDSize; i++)
      identifier[i] ^= ptr[i];
    ptr += kMDGUIDSize;
  }

  munmap(base, mapped_len);
  return true;
}

// static
void FileID::ConvertIdentifierToString(const uint8_t identifier[kMDGUIDSize],
                                       char* buffer, int buffer_length) {
  uint8_t identifier_swapped[kMDGUIDSize];

  // Endian-ness swap to match dump processor expectation.
  memcpy(identifier_swapped, identifier, kMDGUIDSize);
  uint32_t* data1 = reinterpret_cast<uint32_t*>(identifier_swapped);
  *data1 = htonl(*data1);
  uint16_t* data2 = reinterpret_cast<uint16_t*>(identifier_swapped + 4);
  *data2 = htons(*data2);
  uint16_t* data3 = reinterpret_cast<uint16_t*>(identifier_swapped + 6);
  *data3 = htons(*data3);

  int buffer_idx = 0;
  for (unsigned int idx = 0;
       (buffer_idx < buffer_length) && (idx < kMDGUIDSize);
       ++idx) {
    int hi = (identifier_swapped[idx] >> 4) & 0x0F;
    int lo = (identifier_swapped[idx]) & 0x0F;

    if (idx == 4 || idx == 6 || idx == 8 || idx == 10)
      buffer[buffer_idx++] = '-';

    buffer[buffer_idx++] = (hi >= 10) ? 'A' + hi - 10 : '0' + hi;
    buffer[buffer_idx++] = (lo >= 10) ? 'A' + lo - 10 : '0' + lo;
  }

  // NULL terminate
  buffer[(buffer_idx < buffer_length) ? buffer_idx : buffer_idx - 1] = 0;
}

}  // namespace google_breakpad
