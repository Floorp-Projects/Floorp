/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shared-libraries.h"

#define PATH_MAX_TOSTRING(x) #x
#define PATH_MAX_STRING(x) PATH_MAX_TOSTRING(x)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fstream>
#include "platform.h"
#include "shared-libraries.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"
#include "nsDebug.h"
#include "nsNativeCharsetUtils.h"
#include <nsTArray.h>

#include "common/linux/file_id.h"
#include <algorithm>
#include <dlfcn.h>
#if defined(GP_OS_linux) || defined(GP_OS_android)
#  include <features.h>
#endif
#include <sys/types.h>

#if defined(MOZ_LINKER)
#  include "AutoObjectMapper.h"
#endif
#if defined(GP_OS_linux) || defined(GP_OS_android) || defined(GP_OS_freebsd)
#  include <link.h>  // dl_phdr_info
#else
#  error "Unexpected configuration"
#endif

#if defined(GP_OS_android)
extern "C" MOZ_EXPORT __attribute__((weak)) int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data),
    void* data);
#endif

struct LoadedLibraryInfo {
  LoadedLibraryInfo(const char* aName, unsigned long aBaseAddress,
                    unsigned long aFirstMappingStart,
                    unsigned long aLastMappingEnd)
      : mName(aName),
        mBaseAddress(aBaseAddress),
        mFirstMappingStart(aFirstMappingStart),
        mLastMappingEnd(aLastMappingEnd) {}

  nsCString mName;
  unsigned long mBaseAddress;
  unsigned long mFirstMappingStart;
  unsigned long mLastMappingEnd;
};

#if defined(MOZ_LINKER)
static void outputMapperLog(const char* aBuf) { LOG("%s", aBuf); }
#endif

static nsCString IDtoUUIDString(
    const google_breakpad::wasteful_vector<uint8_t>& aIdentifier) {
  using namespace google_breakpad;

  nsCString uuid;
  const std::string str = FileID::ConvertIdentifierToUUIDString(aIdentifier);
  uuid.Append(str.c_str(), str.size());
  // This is '0', not '\0', since it represents the breakpad id age.
  uuid.Append('0');
  return uuid;
}

// Get the breakpad Id for the binary file pointed by bin_name
static nsCString getId(const char* bin_name) {
  using namespace google_breakpad;

  PageAllocator allocator;
  auto_wasteful_vector<uint8_t, kDefaultBuildIdSize> identifier(&allocator);

#if defined(MOZ_LINKER)
  if (nsDependentCString(bin_name).Find("!/") != kNotFound) {
    AutoObjectMapperFaultyLib mapper(outputMapperLog);
    void* image = nullptr;
    size_t size = 0;
    if (mapper.Map(&image, &size, bin_name) && image && size) {
      if (FileID::ElfFileIdentifierFromMappedFile(image, identifier)) {
        return IDtoUUIDString(identifier);
      }
    }
  }
#endif

  FileID file_id(bin_name);
  if (file_id.ElfFileIdentifier(identifier)) {
    return IDtoUUIDString(identifier);
  }

  return EmptyCString();
}

static SharedLibrary SharedLibraryAtPath(const char* path,
                                         unsigned long libStart,
                                         unsigned long libEnd,
                                         unsigned long offset = 0) {
  nsAutoString pathStr;
  mozilla::Unused << NS_WARN_IF(
      NS_FAILED(NS_CopyNativeToUnicode(nsDependentCString(path), pathStr)));

  nsAutoString nameStr = pathStr;
  int32_t pos = nameStr.RFindChar('/');
  if (pos != kNotFound) {
    nameStr.Cut(0, pos + 1);
  }

  return SharedLibrary(libStart, libEnd, offset, getId(path), nameStr, pathStr,
                       nameStr, pathStr, EmptyCString(), "");
}

static int dl_iterate_callback(struct dl_phdr_info* dl_info, size_t size,
                               void* data) {
  auto libInfoList = reinterpret_cast<nsTArray<LoadedLibraryInfo>*>(data);

  if (dl_info->dlpi_phnum <= 0) return 0;

  unsigned long baseAddress = dl_info->dlpi_addr;
  unsigned long firstMappingStart = -1;
  unsigned long lastMappingEnd = 0;

  for (size_t i = 0; i < dl_info->dlpi_phnum; i++) {
    if (dl_info->dlpi_phdr[i].p_type != PT_LOAD) {
      continue;
    }
    unsigned long start = dl_info->dlpi_addr + dl_info->dlpi_phdr[i].p_vaddr;
    unsigned long end = start + dl_info->dlpi_phdr[i].p_memsz;
    if (start < firstMappingStart) {
      firstMappingStart = start;
    }
    if (end > lastMappingEnd) {
      lastMappingEnd = end;
    }
  }

  libInfoList->AppendElement(LoadedLibraryInfo(
      dl_info->dlpi_name, baseAddress, firstMappingStart, lastMappingEnd));

  return 0;
}

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf() {
  SharedLibraryInfo info;

#if defined(GP_OS_linux)
  // We need to find the name of the executable (exeName, exeNameLen) and the
  // address of its executable section (exeExeAddr) in the running image.
  char exeName[PATH_MAX];
  memset(exeName, 0, sizeof(exeName));

  ssize_t exeNameLen = readlink("/proc/self/exe", exeName, sizeof(exeName) - 1);
  if (exeNameLen == -1) {
    // readlink failed for whatever reason.  Note this, but keep going.
    exeName[0] = '\0';
    exeNameLen = 0;
    LOG("SharedLibraryInfo::GetInfoForSelf(): readlink failed");
  } else {
    // Assert no buffer overflow.
    MOZ_RELEASE_ASSERT(exeNameLen >= 0 &&
                       exeNameLen < static_cast<ssize_t>(sizeof(exeName)));
  }

  unsigned long exeExeAddr = 0;
#endif

#if defined(GP_OS_android)
  // If dl_iterate_phdr doesn't exist, we give up immediately.
  if (!dl_iterate_phdr) {
    // On ARM Android, dl_iterate_phdr is provided by the custom linker.
    // So if libxul was loaded by the system linker (e.g. as part of
    // xpcshell when running tests), it won't be available and we should
    // not call it.
    return info;
  }
#endif

#if defined(GP_OS_linux) || defined(GP_OS_android)
  // Read info from /proc/self/maps. We ignore most of it.
  pid_t pid = profiler_current_process_id();
  char path[PATH_MAX];
  SprintfLiteral(path, "/proc/%d/maps", pid);
  std::ifstream maps(path);
  std::string line;
  while (std::getline(maps, line)) {
    int ret;
    unsigned long start;
    unsigned long end;
    char perm[6 + 1] = "";
    unsigned long offset;
    char modulePath[PATH_MAX + 1] = "";
    ret = sscanf(line.c_str(),
                 "%lx-%lx %6s %lx %*s %*x %" PATH_MAX_STRING(PATH_MAX) "s\n",
                 &start, &end, perm, &offset, modulePath);
    if (!strchr(perm, 'x')) {
      // Ignore non executable entries
      continue;
    }
    if (ret != 5 && ret != 4) {
      LOG("SharedLibraryInfo::GetInfoForSelf(): "
          "reading /proc/self/maps failed");
      continue;
    }

#  if defined(GP_OS_linux)
    // Try to establish the main executable's load address.
    if (exeNameLen > 0 && strcmp(modulePath, exeName) == 0) {
      exeExeAddr = start;
    }
#  elif defined(GP_OS_android)
    // Use /proc/pid/maps to get the dalvik-jit section since it has no
    // associated phdrs.
    if (0 == strcmp(modulePath, "/dev/ashmem/dalvik-jit-code-cache")) {
      info.AddSharedLibrary(
          SharedLibraryAtPath(modulePath, start, end, offset));
      if (info.GetSize() > 10000) {
        LOG("SharedLibraryInfo::GetInfoForSelf(): "
            "implausibly large number of mappings acquired");
        break;
      }
    }
#  endif
  }
#endif

  nsTArray<LoadedLibraryInfo> libInfoList;

  // We collect the bulk of the library info using dl_iterate_phdr.
  dl_iterate_phdr(dl_iterate_callback, &libInfoList);

  for (const auto& libInfo : libInfoList) {
    info.AddSharedLibrary(
        SharedLibraryAtPath(libInfo.mName.get(), libInfo.mFirstMappingStart,
                            libInfo.mLastMappingEnd,
                            libInfo.mFirstMappingStart - libInfo.mBaseAddress));
  }

#if defined(GP_OS_linux)
  // Make another pass over the information we just harvested from
  // dl_iterate_phdr.  If we see a nameless object mapped at what we earlier
  // established to be the main executable's load address, attach the
  // executable's name to that entry.
  for (size_t i = 0; i < info.GetSize(); i++) {
    SharedLibrary& lib = info.GetMutableEntry(i);
    if (lib.GetStart() <= exeExeAddr && exeExeAddr <= lib.GetEnd() &&
        lib.GetNativeDebugPath().empty()) {
      lib = SharedLibraryAtPath(exeName, lib.GetStart(), lib.GetEnd(),
                                lib.GetOffset());

      // We only expect to see one such entry.
      break;
    }
  }
#endif

  return info;
}

void SharedLibraryInfo::Initialize() { /* do nothing */
}
