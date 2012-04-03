/* libunwind - a platform-independent unwind library
   Copyright (C) 2003-2005 Hewlett-Packard Co
	Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

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

#include <limits.h>
#include <stdio.h>

#include "libunwind_i.h"
#include "os-linux.h"
#include "dl-iterate-phdr.h"

struct memory_page {
  struct memory_page* next;
  uint32_t allocated;
  // This data structure is getpagesize() bytes long,
  // the rest will be memory which will be allocated
  // on the fly.
};

struct dl_phdr_info_cache {
  struct dl_phdr_info info;
  struct dl_phdr_info_cache* next;
};

#define ALIGNMENT 8 // on ARM
#define OBJECT_SIZE \
  (sizeof(struct dl_phdr_info_cache) + \
   sizeof(struct dl_phdr_info_cache) % ALIGNMENT)

static struct memory_page* heap = 0;

static void*
mmap_malloc()
{
  struct memory_page* prev = 0;
  struct memory_page* current = heap;
  while (current && current->next) {
    prev = current;
    current = current->next;
  }
  if (current) {
    // Do we have enough room in the current page?
    int pagesize = getpagesize();
    int used_space = sizeof(struct memory_page) +
      OBJECT_SIZE * current->allocated;
    int remaining = pagesize - used_space;
    if (remaining < OBJECT_SIZE)
      current = 0;
  }
  if (!current) {
    // Allocate a new page if we have to
    current = mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (current == MAP_FAILED)
      return 0;
    if (prev)
      prev->next = current;
    else
      heap = current;
  }
  void* object = current;
  object += sizeof(struct memory_page);
  object += OBJECT_SIZE * current->allocated;
  ++current->allocated;
  return object;
}

static struct dl_phdr_info_cache* dl_info_cache = 0;

static void
delete_memory_page(struct memory_page* page)
{
  if (page && page->next)
    delete_memory_page(page->next);
  munmap(page, getpagesize());
}

static void
delete_names()
{
  struct dl_phdr_info_cache* current = dl_info_cache;
  for (; current; current = current->next) {
    munmap(current->info.dlpi_name, PATH_MAX);
  }
}

static void
dl_delete_cache()
{
  delete_names();
  delete_memory_page(heap);
}

static int
dl_iterate_phdr_cache ()
{
  struct map_iterator mi;
  unsigned long hi;
  unsigned long low;
  unsigned long offset;
  struct dl_phdr_info_cache* current;

  assert(!dl_info_cache);

  if (maps_init (&mi, getpid()) < 0)
    return 0;

  current = mmap_malloc();
  current->next = 0;
  dl_info_cache = current;

  while (maps_next (&mi, &low, &hi, &offset)) {
    // We don't care about entries which have been mapped at anything except for the first segment
    if (offset != 0)
      continue;

    // Look for .so in the file name as a poor means of detecting possible shared objects
    if (!strstr(mi.path, ".so"))
      continue;

    struct elf_image ei;
    ei.image = (void*)low;
    ei.size = hi - low;

    if (!elf_w(valid_object) (&ei))
      continue;

    current->info.dlpi_name = mmap(0, PATH_MAX, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (current->info.dlpi_name == MAP_FAILED)
      continue;
    strncpy(current->info.dlpi_name, mi.path, PATH_MAX);
    current->info.dlpi_name[PATH_MAX - 1] = 0;
    current->info.dlpi_addr = low;

    Elf_W(Ehdr) *ehdr;

    ehdr = ei.image;
    Elf_W(Phdr) *phdr = (Elf_W(Phdr) *) ((char *) ei.image + ehdr->e_phoff);
    current->info.dlpi_phdr = phdr;
    current->info.dlpi_phnum = ehdr->e_phnum;

    current->next = mmap_malloc();
    current->next->next = 0;
    current = current->next;
  }
  maps_close (&mi);

  atexit(dl_delete_cache);

  return 1;
}

int
dl_iterate_phdr (int (*callback) (struct dl_phdr_info *info, size_t size, void *data),
                 void *data)
{
  int rc = 0;
  struct dl_phdr_info_cache* current;

  if (!dl_info_cache && !dl_iterate_phdr_cache())
    return -1;

  for (current = dl_info_cache; !rc && current->next; current = current->next) {
    rc = callback(&current->info, sizeof(current->info), data);
  }

  return rc;
}
