/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shared-libraries.h"

#include "ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"
#include "nsNativeCharsetUtils.h"
#include <AvailabilityMacros.h>

#include <dlfcn.h>
#include <mach-o/arch.h>
#include <mach-o/dyld_images.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#include <mach/task_info.h>
#include <mach/task.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <vector>

// Architecture specific abstraction.
#if defined(GP_ARCH_x86)
typedef mach_header platform_mach_header;
typedef segment_command mach_segment_command_type;
#  define MACHO_MAGIC_NUMBER MH_MAGIC
#  define CMD_SEGMENT LC_SEGMENT
#  define seg_size uint32_t
#else
typedef mach_header_64 platform_mach_header;
typedef segment_command_64 mach_segment_command_type;
#  define MACHO_MAGIC_NUMBER MH_MAGIC_64
#  define CMD_SEGMENT LC_SEGMENT_64
#  define seg_size uint64_t
#endif

struct NativeSharedLibrary {
  const platform_mach_header* header;
  std::string path;
};
static std::vector<NativeSharedLibrary>* sSharedLibrariesList = nullptr;
static mozilla::StaticMutex sSharedLibrariesMutex;

static void SharedLibraryAddImage(const struct mach_header* mh,
                                  intptr_t vmaddr_slide) {
  // NOTE: Presumably for backwards-compatibility reasons, this function accepts
  // a mach_header even on 64-bit where it ought to be a mach_header_64. We cast
  // it to the right type here.
  auto header = reinterpret_cast<const platform_mach_header*>(mh);

  Dl_info info;
  if (!dladdr(header, &info)) {
    return;
  }

  mozilla::StaticMutexAutoLock lock(sSharedLibrariesMutex);
  if (!sSharedLibrariesList) {
    return;
  }

  NativeSharedLibrary lib = {header, info.dli_fname};
  sSharedLibrariesList->push_back(lib);
}

static void SharedLibraryRemoveImage(const struct mach_header* mh,
                                     intptr_t vmaddr_slide) {
  // NOTE: Presumably for backwards-compatibility reasons, this function accepts
  // a mach_header even on 64-bit where it ought to be a mach_header_64. We cast
  // it to the right type here.
  auto header = reinterpret_cast<const platform_mach_header*>(mh);

  mozilla::StaticMutexAutoLock lock(sSharedLibrariesMutex);
  if (!sSharedLibrariesList) {
    return;
  }

  uint32_t count = sSharedLibrariesList->size();
  for (uint32_t i = 0; i < count; ++i) {
    if ((*sSharedLibrariesList)[i].header == header) {
      sSharedLibrariesList->erase(sSharedLibrariesList->begin() + i);
      return;
    }
  }
}

void SharedLibraryInfo::Initialize() {
  // NOTE: We intentionally leak this memory here. We're allocating dynamically
  // in order to avoid static initializers.
  sSharedLibrariesList = new std::vector<NativeSharedLibrary>();

  _dyld_register_func_for_add_image(SharedLibraryAddImage);
  _dyld_register_func_for_remove_image(SharedLibraryRemoveImage);
}

static void addSharedLibrary(const platform_mach_header* header,
                             const char* path, SharedLibraryInfo& info) {
  const struct load_command* cmd =
      reinterpret_cast<const struct load_command*>(header + 1);

  seg_size size = 0;
  unsigned long long start = reinterpret_cast<unsigned long long>(header);
  // Find the cmd segment in the macho image. It will contain the offset we care
  // about.
  const uint8_t* uuid_bytes = nullptr;
  for (unsigned int i = 0;
       cmd && (i < header->ncmds) && (uuid_bytes == nullptr || size == 0);
       ++i) {
    if (cmd->cmd == CMD_SEGMENT) {
      const mach_segment_command_type* seg =
          reinterpret_cast<const mach_segment_command_type*>(cmd);

      if (!strcmp(seg->segname, "__TEXT")) {
        size = seg->vmsize;
      }
    } else if (cmd->cmd == LC_UUID) {
      const uuid_command* ucmd = reinterpret_cast<const uuid_command*>(cmd);
      uuid_bytes = ucmd->uuid;
    }

    cmd = reinterpret_cast<const struct load_command*>(
        reinterpret_cast<const char*>(cmd) + cmd->cmdsize);
  }

  nsAutoCString uuid;
  if (uuid_bytes != nullptr) {
    uuid.AppendPrintf(
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "%02X"
        "0" /* breakpad id age */,
        uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
        uuid_bytes[4], uuid_bytes[5], uuid_bytes[6], uuid_bytes[7],
        uuid_bytes[8], uuid_bytes[9], uuid_bytes[10], uuid_bytes[11],
        uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);
  }

  nsAutoString pathStr;
  mozilla::Unused << NS_WARN_IF(
      NS_FAILED(NS_CopyNativeToUnicode(nsDependentCString(path), pathStr)));

  nsAutoString nameStr = pathStr;
  int32_t pos = nameStr.RFindChar('/');
  if (pos != kNotFound) {
    nameStr.Cut(0, pos + 1);
  }

  const NXArchInfo* archInfo =
      NXGetArchInfoFromCpuType(header->cputype, header->cpusubtype);

  info.AddSharedLibrary(SharedLibrary(start, start + size, 0, uuid, nameStr,
                                      pathStr, nameStr, pathStr, EmptyCString(),
                                      archInfo ? archInfo->name : ""));
}

// Translate the statically stored sSharedLibrariesList information into a
// SharedLibraryInfo object.
SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  mozilla::StaticMutexAutoLock lock(sSharedLibrariesMutex);
  SharedLibraryInfo sharedLibraryInfo;

  for (auto& info : *sSharedLibrariesList) {
    addSharedLibrary(info.header, info.path.c_str(), sharedLibraryInfo);
  }

  return sharedLibraryInfo;
}
