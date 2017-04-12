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
#include "mozilla/Unused.h"
#include "nsDebug.h"
#include "nsNativeCharsetUtils.h"

#include "common/linux/file_id.h"
#include <algorithm>


// There are three different configuration cases:
//
// (1) GP_OS_linux
//       Use dl_iterate_phdr for everything.
//
// (2) GP_OS_android non-GONK
//       If dl_iterate_phdr doesn't exist, give up immediately.  Otherwise use
//       dl_iterate_phdr for almost all info and /proc/self/maps to get the
//       mapping for /dev/ashmem/dalvik-jit-code-cache.
//
// (3) GP_OS_android GONK
//       Use /proc/self/maps for everything.

#undef CONFIG_CASE_1
#undef CONFIG_CASE_2
#undef CONFIG_CASE_3

#if defined(GP_OS_linux)
# define CONFIG_CASE_1 1
# include <link.h> // dl_phdr_info
# include <features.h>
# include <dlfcn.h>
# include <sys/types.h>

#elif defined(GP_OS_android) && !defined(MOZ_WIDGET_GONK)
# define CONFIG_CASE_2 1
# include "ElfLoader.h" // dl_phdr_info
# include <features.h>
# include <dlfcn.h>
# include <sys/types.h>
extern "C" MOZ_EXPORT __attribute__((weak))
int dl_iterate_phdr(
          int (*callback)(struct dl_phdr_info *info, size_t size, void *data),
          void *data);

#elif defined(GP_OS_android) && defined(MOZ_WIDGET_GONK)
# define CONFIG_CASE_3 1
  // No config-specific includes.

#else
# error "Unexpected configuration"
#endif


// Get the breakpad Id for the binary file pointed by bin_name
static std::string getId(const char *bin_name)
{
  using namespace google_breakpad;
  using namespace std;

  PageAllocator allocator;
  auto_wasteful_vector<uint8_t, sizeof(MDGUID)> identifier(&allocator);

  FileID file_id(bin_name);
  if (file_id.ElfFileIdentifier(identifier)) {
    return FileID::ConvertIdentifierToUUIDString(identifier) + "0";
  }

  return "";
}


// Config cases (1) and (2) use dl_iterate_phdr.
#if defined(CONFIG_CASE_1) || defined(CONFIG_CASE_2)
static int
dl_iterate_callback(struct dl_phdr_info *dl_info, size_t size, void *data)
{
  SharedLibraryInfo& info = *reinterpret_cast<SharedLibraryInfo*>(data);

  if (dl_info->dlpi_phnum <= 0)
    return 0;

  unsigned long libStart = -1;
  unsigned long libEnd = 0;

  for (size_t i = 0; i < dl_info->dlpi_phnum; i++) {
    if (dl_info->dlpi_phdr[i].p_type != PT_LOAD) {
      continue;
    }
    unsigned long start = dl_info->dlpi_addr + dl_info->dlpi_phdr[i].p_vaddr;
    unsigned long end = start + dl_info->dlpi_phdr[i].p_memsz;
    if (start < libStart)
      libStart = start;
    if (end > libEnd)
      libEnd = end;
  }
  const char *path = dl_info->dlpi_name;

  nsAutoString pathStr;
  mozilla::Unused <<
    NS_WARN_IF(NS_FAILED(NS_CopyNativeToUnicode(nsDependentCString(path),
                                                pathStr)));

  nsAutoString nameStr = pathStr;
  int32_t pos = nameStr.RFindChar('/');
  if (pos != kNotFound) {
    nameStr.Cut(0, pos + 1);
  }

  SharedLibrary shlib(libStart, libEnd, 0, getId(path),
                      nameStr, pathStr, nameStr, pathStr,
                      "", "");
  info.AddSharedLibrary(shlib);

  return 0;
}
#endif // config cases (1) and (2)


SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf()
{
  SharedLibraryInfo info;

#if defined(CONFIG_CASE_2)
  // If dl_iterate_phdr doesn't exist, we give up immediately.
  if (!dl_iterate_phdr) {
    // On ARM Android, dl_iterate_phdr is provided by the custom linker.
    // So if libxul was loaded by the system linker (e.g. as part of
    // xpcshell when running tests), it won't be available and we should
    // not call it.
    return info;
  }
#endif

#if defined(CONFIG_CASE_2) || defined(CONFIG_CASE_3)
  // Read info from /proc/self/maps.  We do this for config cases (2) and (3),
  // but only in case (3) are we building the module list from that information.
  // For case (2) we're collecting information only for one specific mapping.
  pid_t pid = getpid();
  char path[PATH_MAX];
  snprintf(path, PATH_MAX, "/proc/%d/maps", pid);
  std::ifstream maps(path);
  std::string line;
  int count = 0;
  while (std::getline(maps, line)) {
    int ret;
    unsigned long start;
    unsigned long end;
    char perm[6] = "";
    unsigned long offset;
    char modulePath[PATH_MAX] = "";
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

#if defined(CONFIG_CASE_2)
    // Use /proc/pid/maps to get the dalvik-jit section since it has no
    // associated phdrs.
    if (0 != strcmp(modulePath, "/dev/ashmem/dalvik-jit-code-cache")) {
      continue;
    }
    // Otherwise proceed to the tail of the loop, so as to record the entry.
#elif defined(CONFIG_CASE_3)
    if (strcmp(perm, "r-xp") != 0) {
      // Ignore entries that are writable and/or shared.
      // At least one graphics driver uses short-lived "rwxs" mappings
      // (see bug 926734 comment 5), so just checking for 'x' isn't enough.
      continue;
    }
    // Record all other entries.
#endif

    nsAutoString pathStr;
    mozilla::Unused <<
      NS_WARN_IF(NS_FAILED(NS_CopyNativeToUnicode(
                             nsDependentCString(modulePath), pathStr)));

    nsAutoString nameStr = pathStr;
    int32_t pos = nameStr.RFindChar('/');
    if (pos != kNotFound) {
      nameStr.Cut(0, pos + 1);
    }

    SharedLibrary shlib(start, end, offset, getId(path),
                        nameStr, pathStr, nameStr, pathStr, "", "");
    info.AddSharedLibrary(shlib);
    if (count > 10000) {
      LOG("SharedLibraryInfo::GetInfoForSelf(): "
          "implausibly large number of mappings acquired");
      break;
    }
    count++;
  }
#endif // config cases 2 and 3

#if defined(CONFIG_CASE_1) || defined(CONFIG_CASE_2)
  // For config cases (1) and (2), we collect the bulk of the library info using
  // dl_iterate_phdr.
  dl_iterate_phdr(dl_iterate_callback, &info);
#endif

  return info;
}
