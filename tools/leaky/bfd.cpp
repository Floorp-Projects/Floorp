/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Kipp E.B. Hickman.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  bfd* lib = bfd_openr(aFileName, NULL);
  if (NULL == lib) {
    return;
  }
  char **matching;
  if (!bfd_check_format_matches(lib, bfd_object, &matching)) {
    bfd_close(lib);
    return;
  }

  asymbol* store;
  store = bfd_make_empty_symbol(lib);

  // read mini symbols
  PTR minisyms;
  unsigned int size;
  long symcount = bfd_read_minisymbols(lib, kDynamic, &minisyms, &size);

  int initialSymbols = usefulSymbols;
  if (NULL == externalSymbols) {
    externalSymbols = (Symbol*) malloc(sizeof(Symbol) * 10000);
    numExternalSymbols = 10000;
  }
  Symbol* sp = externalSymbols + usefulSymbols;
  Symbol* lastSymbol = externalSymbols + numExternalSymbols;

  // Scan symbols
  bfd_byte* from = (bfd_byte *) minisyms;
  bfd_byte* fromend = from + symcount * size;
  for (; from < fromend; from += size) {
    asymbol *sym;
    sym = bfd_minisymbol_to_symbol(lib, kDynamic, (const PTR) from, store);

    symbol_info syminfo;
    bfd_get_symbol_info (lib, sym, &syminfo);

//    if ((syminfo.type == 'T') || (syminfo.type == 't')) {
      const char* nm = bfd_asymbol_name(sym);
      if (nm && nm[0]) {
	char* dnm = NULL;
	if (strncmp("__thunk", nm, 7)) {
	  dnm = cplus_demangle(nm, 1);
	}
	sp->Init(dnm ? dnm : nm, syminfo.value + aBaseAddress);
	sp++;
	if (sp >= lastSymbol) {
	  long n = numExternalSymbols + 10000;
	  externalSymbols = (Symbol*)
	    realloc(externalSymbols, (size_t) (sizeof(Symbol) * n));
	  lastSymbol = externalSymbols + n;
	  sp = externalSymbols + numExternalSymbols;
	  numExternalSymbols = n;
	}
      }
//    }
  }


  bfd_close(lib);

  int interesting = sp - externalSymbols;
  if (!quiet) {
    printf("%s provided %d symbols\n", aFileName,
	   interesting - initialSymbols);
  }
  usefulSymbols = interesting;
}

#endif /* USE_BFD */
