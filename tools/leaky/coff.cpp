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
 * The Initial Developer of the Original Code is Kipp E.B. Hickman.
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

#ifdef USE_COFF

#define	LANGUAGE_C
#include <sym.h>
#include <cmplrs/stsupport.h>
#include <symconst.h>
#include <filehdr.h>
#include <ldfcn.h>
#include <string.h>
#include <stdlib.h>

#ifdef IRIX4
extern "C" {
    extern char *demangle(char const* in);
};
#else
#include <dem.h>
#endif

static char *Demangle(char *rawName)
{
#ifdef IRIX4
    return strdup(demangle(rawName));
#else
    char namebuf[4000];
    demangle(rawName, namebuf);
    return strdup(namebuf);
#endif
}

void leaky::readSymbols(const char *fileName)
{
    LDFILE *ldptr;

    ldptr = ldopen(fileName, NULL);
    if (!ldptr) {
	fprintf(stderr, "%s: unable to open \"%s\"\n", applicationName,
		fileName);
	exit(-1);
    }
    if (PSYMTAB(ldptr) == 0) {
	fprintf(stderr, "%s: \"%s\": has no symbol table\n", applicationName,
		fileName);
	exit(-1);
    }

    long isymMax = SYMHEADER(ldptr).isymMax;
    long iextMax = SYMHEADER(ldptr).iextMax;
    long iMax = isymMax + iextMax;

    long alloced = 10000;
    Symbol* syms = (Symbol*) malloc(sizeof(Symbol) * 10000);
    Symbol* sp = syms;
    Symbol* last = syms + alloced;
    SYMR symr;

    for (long isym = 0; isym < iMax; isym++) {
	if (ldtbread(ldptr, isym, &symr) != SUCCESS) {
	    fprintf(stderr, "%s: can't read symbol #%d\n", applicationName,
		    isym);
	    exit(-1);
	}
	if (isym < isymMax) {
	    if ((symr.st == stStaticProc)
		|| ((symr.st == stProc) &&
		    ((symr.sc == scText) || (symr.sc == scAbs)))
		|| ((symr.st == stBlock) &&
		    (symr.sc == scText))) {
		// Text symbol. Set name field to point to the symbol name
		sp->name = Demangle(ldgetname(ldptr, &symr));
		sp->address = symr.value;
		sp++;
		if (sp >= last) {
		    long n = alloced + 10000;
		    syms = (Symbol*)
			realloc(syms, (size_t) (sizeof(Symbol) * n));
		    last = syms + n;
		    sp = syms + alloced;
		    alloced = n;
		}
	    }
	}
    }

    int interesting = sp - syms;
    if (!quiet) {
	printf("Total of %d symbols\n", interesting);
    }
    usefulSymbols = interesting;
    externalSymbols = syms;
}

#endif /* USE_COFF */
