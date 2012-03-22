/* libunwind - a platform-independent unwind library
   Copyright (C) 2010 Konstantin Belousov <kib@freebsd.org>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <stdio.h>

#include "libunwind_i.h"

static void *
get_mem(size_t sz)
{
  void *res;

  res = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (res == MAP_FAILED)
    return (NULL);
  return (res);
}

static void
free_mem(void *ptr, size_t sz)
{
  munmap(ptr, sz);
}

PROTECTED int
tdep_get_elf_image (struct elf_image *ei, pid_t pid, unw_word_t ip,
		    unsigned long *segbase, unsigned long *mapoff, char *path, size_t pathlen)
{
  int mib[4], error, ret;
  size_t len, len1;
  char *buf, *bp, *eb;
  struct kinfo_vmentry *kv;

  len = 0;
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_VMMAP;
  mib[3] = pid;

  error = sysctl(mib, 4, NULL, &len, NULL, 0);
  if (error)
    return (-1);
  len1 = len * 4 / 3;
  buf = get_mem(len1);
  if (buf == NULL)
    return (-1);
  len = len1;
  error = sysctl(mib, 4, buf, &len, NULL, 0);
  if (error) {
    free_mem(buf, len1);
    return (-1);
  }
  ret = -1;
  for (bp = buf, eb = buf + len; bp < eb; bp += kv->kve_structsize) {
     kv = (struct kinfo_vmentry *)(uintptr_t)bp;
     if (ip < kv->kve_start || ip >= kv->kve_end)
       continue;
     if (kv->kve_type != KVME_TYPE_VNODE)
       break;
     *segbase = kv->kve_start;
     *mapoff = kv->kve_offset;
     if (path)
       {
         strncpy(path, kv->kve_path, pathlen);
       }
     ret = elf_map_image (ei, kv->kve_path);
     break;
  }
  free_mem(buf, len1);
  return (ret);
}
