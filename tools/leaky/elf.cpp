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

#ifdef USE_ELF

#include "leaky.h"
#include <stdio.h>
#include <malloc.h>
#include <libelf/libelf.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void leaky::readSymbols(const char *fileName)
{
    int fd = ::open(fileName, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "%s: unable to open \"%s\"\n", applicationName,
		fileName);
	exit(-1);
    }

    elf_version(EV_CURRENT);
    Elf *elf = elf_begin(fd, ELF_C_READ, 0);
    if (!elf) {
	fprintf(stderr, "%s: \"%s\": has no symbol table\n", applicationName,
		fileName);
	exit(-1);
    }

    long alloced = 10000;
    Symbol* syms = (Symbol*) malloc(sizeof(Symbol) * 10000);
    Symbol* sp = syms;
    Symbol* last = syms + alloced;

    // Get each of the relevant sections and add them to the list of
    // symbols.
    Elf32_Ehdr *ehdr = elf32_getehdr(elf);
    if (!ehdr) {
	fprintf(stderr, "%s: elf library lossage\n", applicationName);
	exit(-1);
    }
#if 0
    Elf32_Half ndx = ehdr->e_shstrndx;
#endif

    Elf_Scn *scn = 0;
    int strtabndx = -1;
    for (int i = 1; (scn = elf_nextscn(elf, scn)) != 0; i++) {
	Elf32_Shdr *shdr = elf32_getshdr(scn);
#if 0
	char *name = elf_strptr(elf, ndx, (size_t) shdr->sh_name);
	printf("Section %s (%d 0x%x)\n", name ? name : "(null)",
	       shdr->sh_type, shdr->sh_type);
#endif
	if (shdr->sh_type == SHT_STRTAB) {
	    /* We assume here that string tables preceed symbol tables... */
	    strtabndx = i;
	    continue;
	}
#if 0
	if (shdr->sh_type == SHT_DYNAMIC) {
	    /* Dynamic */
	    Elf_Data *data = elf_getdata(scn, 0);
	    if (!data || !data->d_size) {
		printf("No data...");
		continue;
	    }

	    Elf32_Dyn *dyn = (Elf32_Dyn*) data->d_buf;
	    Elf32_Dyn *lastdyn =
		(Elf32_Dyn*) ((char*) data->d_buf + data->d_size);
	    for (; dyn < lastdyn; dyn++) {
		printf("tag=%d value=0x%x\n", dyn->d_tag, dyn->d_un.d_val);
	    }
	} else
#endif
	if ((shdr->sh_type == SHT_SYMTAB) ||
	    (shdr->sh_type == SHT_DYNSYM)) {
	    /* Symbol table */
	    Elf_Data *data = elf_getdata(scn, 0);
	    if (!data || !data->d_size) {
		printf("No data...");
		continue;
	    }

	    /* In theory we now have the symbols... */
	    Elf32_Sym *esym = (Elf32_Sym*) data->d_buf;
	    Elf32_Sym *lastsym =
		(Elf32_Sym*) ((char*) data->d_buf + data->d_size);
	    for (; esym < lastsym; esym++) {
#if 0
		char *nm = elf_strptr(elf, strtabndx, (size_t)esym->st_name);
		printf("%20s 0x%08x %02x %02x\n",
		       nm, esym->st_value, ELF32_ST_BIND(esym->st_info),
		       ELF32_ST_TYPE(esym->st_info));
#endif
		if ((esym->st_value == 0) ||
		    (ELF32_ST_BIND(esym->st_info) == STB_WEAK) ||
		    (ELF32_ST_BIND(esym->st_info) == STB_NUM) ||
		    (ELF32_ST_TYPE(esym->st_info) != STT_FUNC)) {
		    continue;
		}
#if 1
		char *nm = elf_strptr(elf, strtabndx, (size_t)esym->st_name);
#endif
		sp->name = nm ? strdup(nm) : "(no name)";
		sp->address = esym->st_value;
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

#endif /* USE_ELF */
