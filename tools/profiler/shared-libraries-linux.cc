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
#include "platform.h"
#include "shared-libraries.h"

#if !defined(__GLIBC__) && ANDROID_VERSION < 18
/* a crapy version of getline, because it's not included in old bionics */
static ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
 char *ret;
 if (!*lineptr) {
   *lineptr = (char*)malloc(4096);
 }
 ret = fgets(*lineptr, 4096, stream);
 if (!ret)
   return 0;
 return strlen(*lineptr);
}
#endif

#if !defined(MOZ_WIDGET_GONK)
// TODO fix me with proper include
#include "nsDebug.h"
#ifdef ANDROID
#include "ElfLoader.h" // dl_phdr_info
#else
#include <link.h> // dl_phdr_info
#endif
#include <features.h>
#include <dlfcn.h>
#include <sys/types.h>

#ifdef ANDROID
extern "C" __attribute__((weak))
int dl_iterate_phdr(
          int (*callback) (struct dl_phdr_info *info,
                           size_t size, void *data),
          void *data);
#endif

int dl_iterate_callback(struct dl_phdr_info *dl_info, size_t size, void *data)
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
  SharedLibrary shlib(libStart, libEnd, 0, "", dl_info->dlpi_name);
  info.AddSharedLibrary(shlib);

  return 0;
}
#endif

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf()
{
  SharedLibraryInfo info;

#if !defined(MOZ_WIDGET_GONK)
  dl_iterate_phdr(dl_iterate_callback, &info);
#ifndef ANDROID
  return info;
#endif
#endif

  pid_t pid = getpid();
  char path[PATH_MAX];
  snprintf(path, PATH_MAX, "/proc/%d/maps", pid);
  FILE *maps = fopen(path, "r");
  char *line = NULL;
  int count = 0;
  size_t line_size = 0;
  while (maps && getline (&line, &line_size, maps) > 0) {
    int ret;
    //XXX: needs input sanitizing
    unsigned long start;
    unsigned long end;
    char perm[6] = "";
    unsigned long offset;
    char name[PATH_MAX] = "";
    ret = sscanf(line,
                 "%lx-%lx %6s %lx %*s %*x %" PATH_MAX_STRING(PATH_MAX) "s\n",
                 &start, &end, perm, &offset, name);
    if (!strchr(perm, 'x')) {
      // Ignore non executable entries
      continue;
    }
    if (ret != 5 && ret != 4) {
      LOG("Get maps line failed");
      continue;
    }
#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
    // Use proc/pid/maps to get the dalvik-jit section since it has
    // no associated phdrs
    if (strcmp(name, "/dev/ashmem/dalvik-jit-code-cache") != 0)
      continue;
#endif
    SharedLibrary shlib(start, end, offset, "", name);
    info.AddSharedLibrary(shlib);
    if (count > 10000) {
      LOG("Get maps failed");
      break;
    }
    count++;
  }
  free(line);
  return info;
}
