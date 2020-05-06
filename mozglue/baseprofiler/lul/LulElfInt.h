/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

// Copyright (c) 2006, 2012, Google Inc.
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

// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/android/include/elf.h
//   src/common/linux/elfutils.h
//   src/common/linux/file_id.h
//   src/common/linux/elfutils-inl.h

#ifndef LulElfInt_h
#define LulElfInt_h

// This header defines functions etc internal to the ELF reader.  It
// should not be included outside of LulElf.cpp.

#include <elf.h>
#include <stdlib.h>

#include "mozilla/Assertions.h"

#include "PlatformMacros.h"

// (derived from)
// elfutils.h: Utilities for dealing with ELF files.
//
#include <link.h>

#if defined(GP_OS_android)

// From toolkit/crashreporter/google-breakpad/src/common/android/include/elf.h
// The Android headers don't always define this constant.
#  ifndef EM_X86_64
#    define EM_X86_64 62
#  endif

#  ifndef EM_PPC64
#    define EM_PPC64 21
#  endif

#  ifndef EM_S390
#    define EM_S390 22
#  endif

#  ifndef NT_GNU_BUILD_ID
#    define NT_GNU_BUILD_ID 3
#  endif

#  ifndef ElfW
#    define ElfW(type) _ElfW(Elf, ELFSIZE, type)
#    define _ElfW(e, w, t) _ElfW_1(e, w, _##t)
#    define _ElfW_1(e, w, t) e##w##t
#  endif

#endif

#if defined(GP_OS_freebsd)

#  ifndef ElfW
#    define ElfW(type) Elf_##type
#  endif

#endif

namespace lul {

// Traits classes so consumers can write templatized code to deal
// with specific ELF bits.
struct ElfClass32 {
  typedef Elf32_Addr Addr;
  typedef Elf32_Ehdr Ehdr;
  typedef Elf32_Nhdr Nhdr;
  typedef Elf32_Phdr Phdr;
  typedef Elf32_Shdr Shdr;
  typedef Elf32_Half Half;
  typedef Elf32_Off Off;
  typedef Elf32_Word Word;
  static const int kClass = ELFCLASS32;
  static const size_t kAddrSize = sizeof(Elf32_Addr);
};

struct ElfClass64 {
  typedef Elf64_Addr Addr;
  typedef Elf64_Ehdr Ehdr;
  typedef Elf64_Nhdr Nhdr;
  typedef Elf64_Phdr Phdr;
  typedef Elf64_Shdr Shdr;
  typedef Elf64_Half Half;
  typedef Elf64_Off Off;
  typedef Elf64_Word Word;
  static const int kClass = ELFCLASS64;
  static const size_t kAddrSize = sizeof(Elf64_Addr);
};

bool IsValidElf(const void* elf_header);
int ElfClass(const void* elf_base);

// Attempt to find a section named |section_name| of type |section_type|
// in the ELF binary data at |elf_mapped_base|. On success, returns true
// and sets |*section_start| to point to the start of the section data,
// and |*section_size| to the size of the section's data. If |elfclass|
// is not NULL, set |*elfclass| to the ELF file class.
bool FindElfSection(const void* elf_mapped_base, const char* section_name,
                    uint32_t section_type, const void** section_start,
                    int* section_size, int* elfclass);

// Internal helper method, exposed for convenience for callers
// that already have more info.
template <typename ElfClass>
const typename ElfClass::Shdr* FindElfSectionByName(
    const char* name, typename ElfClass::Word section_type,
    const typename ElfClass::Shdr* sections, const char* section_names,
    const char* names_end, int nsection);

// Attempt to find the first segment of type |segment_type| in the ELF
// binary data at |elf_mapped_base|. On success, returns true and sets
// |*segment_start| to point to the start of the segment data, and
// and |*segment_size| to the size of the segment's data. If |elfclass|
// is not NULL, set |*elfclass| to the ELF file class.
bool FindElfSegment(const void* elf_mapped_base, uint32_t segment_type,
                    const void** segment_start, int* segment_size,
                    int* elfclass);

// Convert an offset from an Elf header into a pointer to the mapped
// address in the current process. Takes an extra template parameter
// to specify the return type to avoid having to dynamic_cast the
// result.
template <typename ElfClass, typename T>
const T* GetOffset(const typename ElfClass::Ehdr* elf_header,
                   typename ElfClass::Off offset);

// (derived from)
// file_id.h: Return a unique identifier for a file
//

static const size_t kMDGUIDSize = sizeof(MDGUID);

class FileID {
 public:
  // Load the identifier for the elf file mapped into memory at |base| into
  // |identifier|.  Return false if the identifier could not be created for the
  // file.
  static bool ElfFileIdentifierFromMappedFile(const void* base,
                                              uint8_t identifier[kMDGUIDSize]);

  // Convert the |identifier| data to a NULL terminated string.  The string will
  // be formatted as a UUID (e.g., 22F065BB-FC9C-49F7-80FE-26A7CEBD7BCE).
  // The |buffer| should be at least 37 bytes long to receive all of the data
  // and termination.  Shorter buffers will contain truncated data.
  static void ConvertIdentifierToString(const uint8_t identifier[kMDGUIDSize],
                                        char* buffer, int buffer_length);
};

template <typename ElfClass, typename T>
const T* GetOffset(const typename ElfClass::Ehdr* elf_header,
                   typename ElfClass::Off offset) {
  return reinterpret_cast<const T*>(reinterpret_cast<uintptr_t>(elf_header) +
                                    offset);
}

template <typename ElfClass>
const typename ElfClass::Shdr* FindElfSectionByName(
    const char* name, typename ElfClass::Word section_type,
    const typename ElfClass::Shdr* sections, const char* section_names,
    const char* names_end, int nsection) {
  MOZ_ASSERT(name != NULL);
  MOZ_ASSERT(sections != NULL);
  MOZ_ASSERT(nsection > 0);

  int name_len = strlen(name);
  if (name_len == 0) return NULL;

  for (int i = 0; i < nsection; ++i) {
    const char* section_name = section_names + sections[i].sh_name;
    if (sections[i].sh_type == section_type &&
        names_end - section_name >= name_len + 1 &&
        strcmp(name, section_name) == 0) {
      return sections + i;
    }
  }
  return NULL;
}

}  // namespace lul

// And finally, the external interface, offered to LulMain.cpp
#include "LulElfExt.h"

#endif  // LulElfInt_h
