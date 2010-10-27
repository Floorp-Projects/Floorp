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
#include <assert.h>
#include <elf.h>
#include <fcntl.h>
#if defined(__ANDROID__)
#include "client/linux/android_link.h"
#else
#include <link.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include "common/linux/linux_libc_support.h"
#include "common/linux/linux_syscall_support.h"

namespace google_breakpad {

FileID::FileID(const char* path) {
  strncpy(path_, path, sizeof(path_));
}

struct ElfClass32 {
  typedef Elf32_Ehdr Ehdr;
  typedef Elf32_Shdr Shdr;
  static const int kClass = ELFCLASS32;
};

struct ElfClass64 {
  typedef Elf64_Ehdr Ehdr;
  typedef Elf64_Shdr Shdr;
  static const int kClass = ELFCLASS64;
};

// These three functions are also used inside the crashed process, so be safe
// and use the syscall/libc wrappers instead of direct syscalls or libc.
template<typename ElfClass>
static void FindElfClassTextSection(const char *elf_base,
                                    const void **text_start,
                                    int *text_size) {
  typedef typename ElfClass::Ehdr Ehdr;
  typedef typename ElfClass::Shdr Shdr;

  assert(elf_base);
  assert(text_start);
  assert(text_size);

  assert(my_strncmp(elf_base, ELFMAG, SELFMAG) == 0);

  const char* text_section_name = ".text";
  int name_len = my_strlen(text_section_name);

  const Ehdr* elf_header = reinterpret_cast<const Ehdr*>(elf_base);
  assert(elf_header->e_ident[EI_CLASS] == ElfClass::kClass);

  const Shdr* sections =
      reinterpret_cast<const Shdr*>(elf_base + elf_header->e_shoff);
  const Shdr* string_section = sections + elf_header->e_shstrndx;

  const Shdr* text_section = NULL;
  for (int i = 0; i < elf_header->e_shnum; ++i) {
    if (sections[i].sh_type == SHT_PROGBITS) {
      const char* section_name = (char*)(elf_base +
                                         string_section->sh_offset +
                                         sections[i].sh_name);
      if (!my_strncmp(section_name, text_section_name, name_len)) {
        text_section = &sections[i];
        break;
      }
    }
  }
  if (text_section != NULL && text_section->sh_size > 0) {
    *text_start = elf_base + text_section->sh_offset;
    *text_size = text_section->sh_size;
  }
}

static bool FindElfTextSection(const void *elf_mapped_base,
                               const void **text_start,
                               int *text_size) {
  assert(elf_mapped_base);
  assert(text_start);
  assert(text_size);

  const char* elf_base =
    static_cast<const char*>(elf_mapped_base);
  const ElfW(Ehdr)* elf_header =
    reinterpret_cast<const ElfW(Ehdr)*>(elf_base);
  if (my_strncmp(elf_base, ELFMAG, SELFMAG) != 0)
    return false;

  if (elf_header->e_ident[EI_CLASS] == ELFCLASS32) {
    FindElfClassTextSection<ElfClass32>(elf_base, text_start, text_size);
  } else if (elf_header->e_ident[EI_CLASS] == ELFCLASS64) {
    FindElfClassTextSection<ElfClass64>(elf_base, text_start, text_size);
  } else {
    return false;
  }

  return true;
}

// static
bool FileID::ElfFileIdentifierFromMappedFile(void* base,
                                             uint8_t identifier[kMDGUIDSize])
{
  const void* text_section = NULL;
  int text_size = 0;
  bool success = false;
  if (FindElfTextSection(base, &text_section, &text_size) && (text_size > 0)) {
    my_memset(identifier, 0, kMDGUIDSize);
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(text_section);
    const uint8_t* ptr_end = ptr + std::min(text_size, 4096);
    while (ptr < ptr_end) {
      for (unsigned i = 0; i < kMDGUIDSize; i++)
        identifier[i] ^= ptr[i];
      ptr += kMDGUIDSize;
    }
    success = true;
  }
  return success;
}

bool FileID::ElfFileIdentifier(uint8_t identifier[kMDGUIDSize]) {
  int fd = open(path_, O_RDONLY);
  if (fd < 0)
    return false;
  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return false;
  }
  void* base = mmap(NULL, st.st_size,
                    PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  close(fd);
  if (base == MAP_FAILED)
    return false;

  bool success = ElfFileIdentifierFromMappedFile(base, identifier);
  munmap(base, st.st_size);
  return success;
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
