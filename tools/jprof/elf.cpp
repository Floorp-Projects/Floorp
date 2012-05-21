/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
