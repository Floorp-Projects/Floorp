// vim:ts=8:sw=2:et:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "leaky.h"

#ifdef USE_BFD
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <bfd.h>

extern "C" {
  char *cplus_demangle (const char *mangled, int options);
}

static bfd *try_debug_file(const char *filename, unsigned long crc32)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
    return NULL;

  unsigned char buf[4*1024];
  unsigned long crc = 0;

  while (1) {
    ssize_t count = read(fd, buf, sizeof(buf));
    if (count <= 0)
      break;

    crc = bfd_calc_gnu_debuglink_crc32(crc, buf, count);
  }

  close(fd);

  if (crc != crc32)
    return NULL;

  bfd *object = bfd_openr(filename, NULL);
  if (!bfd_check_format(object, bfd_object)) {
    bfd_close(object);
    return NULL;
  }

  return object;
}

static bfd *find_debug_file(bfd *lib, const char *aFileName)
{
  // check for a separate debug file with symbols
  asection *sect = bfd_get_section_by_name(lib, ".gnu_debuglink");

  if (!sect)
    return NULL;

  bfd_size_type debuglinkSize = bfd_section_size (objfile->obfd, sect);

  char *debuglink = new char[debuglinkSize];
  bfd_get_section_contents(lib, sect, debuglink, 0, debuglinkSize);

  // crc checksum is aligned to 4 bytes, and after the NUL.
  int crc_offset = (int(strlen(debuglink)) & ~3) + 4;
  unsigned long crc32 = bfd_get_32(lib, debuglink + crc_offset);

  // directory component
  char *dirbuf = strdup(aFileName);
  const char *dir = dirname(dirbuf);

  static const char debug_subdir[] = ".debug";
  // This is gdb's default global debugging info directory, but gdb can
  // be instructed to use a different directory.
  static const char global_debug_dir[] = "/usr/lib/debug";

  char *filename =
    new char[strlen(global_debug_dir) + strlen(dir) + crc_offset + 3];

  // /path/debuglink
  sprintf(filename, "%s/%s", dir, debuglink);
  bfd *debugFile = try_debug_file(filename, crc32);
  if (!debugFile) {

    // /path/.debug/debuglink
    sprintf(filename, "%s/%s/%s", dir, debug_subdir, debuglink);
    debugFile = try_debug_file(filename, crc32);
    if (!debugFile) {

      // /usr/lib/debug/path/debuglink
      sprintf(filename, "%s/%s/%s", global_debug_dir, dir, debuglink);
      debugFile = try_debug_file(filename, crc32);
    }
  }

  delete[] filename;
  free(dirbuf);
  delete[] debuglink;

  return debugFile;
}


// Use an indirect array to avoid copying tons of objects
Symbol ** leaky::ExtendSymbols(int num)
{
  long n = numExternalSymbols + num;

  externalSymbols = (Symbol**)
                    realloc(externalSymbols,
                            (size_t) (sizeof(externalSymbols[0]) * n));
  Symbol *new_array = new Symbol[n];
  for (int i = 0; i < num; i++) {
    externalSymbols[i + numExternalSymbols] = &new_array[i];
  }
  lastSymbol = externalSymbols + n;
  Symbol **sp = externalSymbols + numExternalSymbols;
  numExternalSymbols = n;
  return sp;
}

#define NEXT_SYMBOL do { sp++; \
                         if (sp >= lastSymbol) { \
                           sp = ExtendSymbols(16384); \
                         } \
                       } while (0)

void leaky::ReadSymbols(const char *aFileName, u_long aBaseAddress)
{
  int initialSymbols = usefulSymbols;
  if (NULL == externalSymbols) {
    externalSymbols = (Symbol**) calloc(sizeof(Symbol*),10000);
    Symbol *new_array = new Symbol[10000];
    for (int i = 0; i < 10000; i++) {
      externalSymbols[i] = &new_array[i];
    }
    numExternalSymbols = 10000;
  }
  Symbol** sp = externalSymbols + usefulSymbols;
  lastSymbol = externalSymbols + numExternalSymbols;

  // Create a dummy symbol for the library so, if it doesn't have any
  // symbols, we show it by library.
  (*sp)->Init(aFileName, aBaseAddress);
  NEXT_SYMBOL;

  bfd_boolean kDynamic = (bfd_boolean) false;

  static int firstTime = 1;
  if (firstTime) {
    firstTime = 0;
    bfd_init ();
  }

  bfd* lib = bfd_openr(aFileName, NULL);
  if (NULL == lib) {
    return;
  }
  if (!bfd_check_format(lib, bfd_object)) {
    bfd_close(lib);
    return;
  }

  bfd *symbolFile = find_debug_file(lib, aFileName);

  // read mini symbols
  PTR minisyms;
  unsigned int size;
  long symcount = 0;

  if (symbolFile) {
    symcount = bfd_read_minisymbols(symbolFile, kDynamic, &minisyms, &size);
    if (symcount == 0) {
      bfd_close(symbolFile);
    } else {
      bfd_close(lib);
    }
  }
  if (symcount == 0) {
    symcount = bfd_read_minisymbols(lib, kDynamic, &minisyms, &size);
    if (symcount == 0) {
      // symtab is empty; try dynamic symbols
      kDynamic = (bfd_boolean) true;
      symcount = bfd_read_minisymbols(lib, kDynamic, &minisyms, &size);
    }
    symbolFile = lib;
  }

  asymbol* store;
  store = bfd_make_empty_symbol(symbolFile);

  // Scan symbols
  bfd_byte* from = (bfd_byte *) minisyms;
  bfd_byte* fromend = from + symcount * size;
  for (; from < fromend; from += size) {
    asymbol *sym;
    sym = bfd_minisymbol_to_symbol(symbolFile, kDynamic, (const PTR) from, store);

    symbol_info syminfo;
    bfd_get_symbol_info (symbolFile, sym, &syminfo);

//    if ((syminfo.type == 'T') || (syminfo.type == 't')) {
      const char* nm = bfd_asymbol_name(sym);
      if (nm && nm[0]) {
        char* dnm = NULL;
        if (strncmp("__thunk", nm, 7)) {
          dnm = cplus_demangle(nm, 1);
        }
        (*sp)->Init(dnm ? dnm : nm, syminfo.value + aBaseAddress);
        NEXT_SYMBOL;
      }
//    }
  }

  bfd_close(symbolFile);

  int interesting = sp - externalSymbols;
  if (!quiet) {
    printf("%s provided %d symbols\n", aFileName,
           interesting - initialSymbols);
  }
  usefulSymbols = interesting;
}

#endif /* USE_BFD */
