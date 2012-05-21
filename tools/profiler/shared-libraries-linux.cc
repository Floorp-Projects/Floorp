/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PATH_MAX_TOSTRING(x) #x
#define PATH_MAX_STRING(x) PATH_MAX_TOSTRING(x)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "platform.h"
#include "shared-libraries.h"

#ifndef __GLIBC__
/* a crapy version of getline, because it's not included in bionic */
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

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf()
{
  pid_t pid = getpid();
  SharedLibraryInfo info;
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
    SharedLibrary shlib(start, end, offset, name);
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
