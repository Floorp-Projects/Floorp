// The contents of this file are subject to the Mozilla Public License
// Version 1.0 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

#include "leaky.h"

#ifdef USE_BFD
#include <stdio.h>
#include <string.h>
#include <bfd.h>

extern "C" {
  char *cplus_demangle (const char *mangled, int options);
}

void leaky::ReadSymbols(const char *aFileName, u_long aBaseAddress)
{
  static bfd_boolean kDynamic = (bfd_boolean) false;

  static int firstTime = 1;
  if (firstTime) {
    firstTime = 0;
    bfd_init ();
  }

  bfd* bfd = bfd_openr(aFileName, NULL);
  if (NULL == bfd) {
    return;
  }
  char **matching;
  if (!bfd_check_format_matches(bfd, bfd_object, &matching)) {
    bfd_close(bfd);
    return;
  }

  asymbol* store;
  store = bfd_make_empty_symbol(bfd);

  // read mini symbols
  PTR minisyms;
  unsigned int size;
  long symcount = bfd_read_minisymbols(bfd, kDynamic, &minisyms, &size);

  int initialSymbols = usefulSymbols;
  if (NULL == externalSymbols) {
    externalSymbols = (Symbol*) malloc(sizeof(Symbol) * 10000);
    numExternalSymbols = 10000;
  }
  Symbol* sp = externalSymbols + usefulSymbols;
  Symbol* last = externalSymbols + numExternalSymbols;

  // Scan symbols
  bfd_byte* from = (bfd_byte *) minisyms;
  bfd_byte* fromend = from + symcount * size;
  for (; from < fromend; from += size) {
    asymbol *sym;
    sym = bfd_minisymbol_to_symbol(bfd, kDynamic, (const PTR) from, store);

    symbol_info syminfo;
    bfd_get_symbol_info (bfd, sym, &syminfo);

//    if ((syminfo.type == 'T') || (syminfo.type == 't')) {
      const char* nm = bfd_asymbol_name(sym);
      if (nm) {
//	char* dnm = cplus_demangle(nm, 1);
//	sp->name = dnm ? dnm : strdup(nm);
	sp->name = strdup(nm);
	sp->address = syminfo.value + aBaseAddress;
	sp++;
	if (sp >= last) {
	  long n = numExternalSymbols + 10000;
	  externalSymbols = (Symbol*)
	    realloc(externalSymbols, (size_t) (sizeof(Symbol) * n));
	  last = externalSymbols + n;
	  sp = externalSymbols + numExternalSymbols;
	  numExternalSymbols = n;
	}
      }
//    }
  }


  bfd_close(bfd);

  int interesting = sp - externalSymbols;
  if (!quiet) {
    printf("%s provided %d symbols\n", aFileName,
	   interesting - initialSymbols);
  }
  usefulSymbols = interesting;
}

#endif /* USE_BFD */
