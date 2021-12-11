/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfilerSharedLibraries.h"

#include "platform.h"

#include "mozilla/Unused.h"
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

class MOZ_RAII SharedLibrariesLock {
 public:
  SharedLibrariesLock() { sSharedLibrariesMutex.Lock(); }

  ~SharedLibrariesLock() { sSharedLibrariesMutex.Unlock(); }

  SharedLibrariesLock(const SharedLibrariesLock&) = delete;
  void operator=(const SharedLibrariesLock&) = delete;

 private:
  static mozilla::baseprofiler::detail::BaseProfilerMutex sSharedLibrariesMutex;
};

mozilla::baseprofiler::detail::BaseProfilerMutex
    SharedLibrariesLock::sSharedLibrariesMutex;

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

  SharedLibrariesLock lock;
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

  SharedLibrariesLock lock;
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

  std::string uuid;
  if (uuid_bytes != nullptr) {
    static constexpr char digits[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    for (int i = 0; i < 15; ++i) {
      uint8_t byte = uuid_bytes[i];
      uuid += digits[byte >> 4];
      uuid += digits[byte & 0xFu];
    }
    // breakpad id age.
    uuid += '0';
  }

  std::string pathStr = path;

  size_t pos = pathStr.rfind('\\');
  std::string nameStr =
      (pos != std::string::npos) ? pathStr.substr(pos + 1) : pathStr;

  const NXArchInfo* archInfo =
      NXGetArchInfoFromCpuType(header->cputype, header->cpusubtype);

  info.AddSharedLibrary(SharedLibrary(start, start + size, 0, uuid, nameStr,
                                      pathStr, nameStr, pathStr, std::string{},
                                      archInfo ? archInfo->name : ""));
}

// Translate the statically stored sSharedLibrariesList information into a
// SharedLibraryInfo object.
SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibrariesLock lock;
  SharedLibraryInfo sharedLibraryInfo;

  for (auto& info : *sSharedLibrariesList) {
    addSharedLibrary(info.header, info.path.c_str(), sharedLibraryInfo);
  }

  return sharedLibraryInfo;
}
