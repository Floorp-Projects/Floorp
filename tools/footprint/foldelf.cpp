/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is foldelf.cpp, released November 28, 2000.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Chris Waterson <waterson@netscape.com>
 *
 * This program reads an ELF file and computes information about
 * redundancies.
 *
 * 
 */

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

//----------------------------------------------------------------------

char* opt_type    = "func";
char* opt_section = ".text";

//----------------------------------------------------------------------

static void
hexdump(ostream& out, const char* bytes, size_t count)
{
    hex(out);

    size_t off = 0;
    while (off < count) {
        out.form("%08lx: ", off);

        const char* p = bytes + off;

        int j = 0;
        while (j < 16) {
            out.form("%02x", p[j++] & 0xff);
            if (j + off >= count)
                break;

            out.form("%02x ", p[j++] & 0xff);
            if (j + off >= count)
                break;
        }

        // Pad
        for (; j < 16; ++j)
            out << ((j%2) ? "   " : "  ");

        for (j = 0; j < 16; ++j) {
            if (j + off < count)
                out.put(isprint(p[j]) ? p[j] : '.');
        }

        out << endl;
        off += 16;
    }
}

//----------------------------------------------------------------------

int
verify_elf_header(const Elf32_Ehdr* hdr)
{
    if (hdr->e_ident[EI_MAG0] != ELFMAG0
        || hdr->e_ident[EI_MAG1] != ELFMAG1
        || hdr->e_ident[EI_MAG2] != ELFMAG2
        || hdr->e_ident[EI_MAG3] != ELFMAG3) {
        cerr << "not an elf file" << endl;
        return -1;
    }

    if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        cerr << "not a 32-bit elf file" << endl;
        return -1;
    }

    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        cerr << "not a little endian elf file" << endl;
        return -1;
    }

    if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        cerr << "incompatible version" << endl;
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------

class elf_symbol : public Elf32_Sym
{
public:
    elf_symbol(const Elf32_Sym& sym)
    { ::memcpy(static_cast<Elf32_Sym*>(this), &sym, sizeof(Elf32_Sym)); }

    friend bool operator==(const elf_symbol& lhs, const elf_symbol& rhs) {
        return 0 == ::memcmp(static_cast<const Elf32_Sym*>(&lhs),
                             static_cast<const Elf32_Sym*>(&rhs),
                             sizeof(Elf32_Sym)); }
};

//----------------------------------------------------------------------

static const char*
st_bind(unsigned char info)
{
    switch (ELF32_ST_BIND(info)) {
    case STB_LOCAL:      return "local";
    case STB_GLOBAL:     return "global";
    case STB_WEAK:       return "weak";
    default:             return "unknown";
    }
}

static const char*
st_type(unsigned char info)
{
    switch (ELF32_ST_TYPE(info)) {
    case STT_NOTYPE:     return "none";
    case STT_OBJECT:     return "object";
    case STT_FUNC:       return "func";
    case STT_SECTION:    return "section";
    case STT_FILE:       return "file";
    default:             return "unknown";
    }
}

static unsigned char
st_type(const char* type)
{
    if (strcmp(type, "none") == 0) {
        return STT_NOTYPE;
    }
    else if (strcmp(type, "object") == 0) {
        return STT_OBJECT;
    }
    else if (strcmp(type, "func") == 0) {
        return STT_FUNC;
    }
    else {
        return 0;
    }
}

//----------------------------------------------------------------------

typedef vector<elf_symbol> elf_symbol_table;
typedef map< basic_string<char>, elf_symbol_table > elf_text_map;

void
process_mapping(char* mapping, size_t size)
{
    const Elf32_Ehdr* ehdr = reinterpret_cast<Elf32_Ehdr*>(mapping);
    if (verify_elf_header(ehdr) < 0)
        return;

    // find the section headers
    const Elf32_Shdr* shdrs = reinterpret_cast<Elf32_Shdr*>(mapping + ehdr->e_shoff);

    // find the section header string table, .shstrtab
    const Elf32_Shdr* shstrtabsh = shdrs + ehdr->e_shstrndx;
    const char* shstrtab = mapping + shstrtabsh->sh_offset;

    // find the sections we care about
    const Elf32_Shdr *symtabsh, *strtabsh, *textsh;
    int textndx;

    for (int i = 0; i < ehdr->e_shnum; ++i) {
        basic_string<char> name(shstrtab + shdrs[i].sh_name);
        if (name == opt_section) {
            textsh = shdrs + i;
            textndx = i;
        }
        else if (name == ".symtab") {
            symtabsh = shdrs + i;
        }
        else if (name == ".strtab") {
            strtabsh = shdrs + i;
        }
    }

    // find the .strtab
    char* strtab = mapping + strtabsh->sh_offset;

    // find the .text
    char* text = mapping + textsh->sh_offset;
    int textaddr = textsh->sh_addr;

    // find the symbol table
    int nentries = symtabsh->sh_size / sizeof(Elf32_Sym);
    Elf32_Sym* symtab = reinterpret_cast<Elf32_Sym*>(mapping + symtabsh->sh_offset);

    // look for symbols in the .text section
    elf_text_map textmap;

    for (int i = 0; i < nentries; ++i) {
        const Elf32_Sym* sym = symtab + i;
        if (sym->st_shndx == textndx &&
            ELF32_ST_TYPE(sym->st_info) == st_type(opt_type) &&
            sym->st_size) {
            basic_string<char> functext(text + sym->st_value - textaddr, sym->st_size);

            elf_symbol_table& syms = textmap[functext];
            if (syms.end() == find(syms.begin(), syms.end(), elf_symbol(*sym)))
                syms.insert(syms.end(), *sym);
        }
    }

    int uniquebytes = 0, totalbytes = 0;
    int uniquecount = 0, totalcount = 0;

    for (elf_text_map::const_iterator entry = textmap.begin();
         entry != textmap.end();
         ++entry) {
        const elf_symbol_table& syms = entry->second;

        if (syms.size() <= 1)
            continue;

        int sz = syms.begin()->st_size;
        uniquebytes += sz;
        totalbytes += sz * syms.size();
        uniquecount += 1;
        totalcount += syms.size();

        for (elf_symbol_table::const_iterator sym = syms.begin(); sym != syms.end(); ++sym)
            cout << strtab + sym->st_name << endl;

        dec(cout);
        cout << syms.size() << " copies of " << sz << " bytes";
        cout << " (" << ((syms.size() - 1) * sz) << " redundant bytes)" << endl;

        hexdump(cout, entry->first.data(), entry->first.size());
        cout << endl;
    }

    dec(cout);
    cout << "bytes unique=" << uniquebytes << ", total=" << totalbytes << endl;
    cout << "entries unique=" << uniquecount << ", total=" << totalcount << endl;
}

void
process_file(const char* name)
{
    int fd = open(name, O_RDWR);
    if (fd >= 0) {
        struct stat statbuf;
        if (fstat(fd, &statbuf) >= 0) {
            size_t size = statbuf.st_size;

            void* mapping = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
            if (mapping != MAP_FAILED) {
                process_mapping(static_cast<char*>(mapping), size);
                munmap(mapping, size);
            }
        }
        close(fd);
    }
}

static void
usage()
{
    cerr << "foldelf [--section=<section>] [--type=<type>] [file ...]\n\
   --section, -s  the section of the ELF file to scan; defaults\n\
                  to ``.text''. Valid values include any section\n\
                  of the ELF file.\n\
   --type, -t     the type of object to examine in the section;\n\
                  defaults to ``func''. Valid values include\n\
                  ``none'', ``func'', or ``object''.\n";

}

static struct option opts[] = {
    { "type",    required_argument, 0, 't' },
    { "section", required_argument, 0, 's' },
    { "help",    no_argument,       0, '?' },
    { 0,         0, 0, 0 }
};
    
int
main(int argc, char* argv[])
{
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "t:s:", opts, &option_index);

        if (c < 0) break;

        switch (c) {
        case 't':
            opt_type = optarg;
            break;

        case 's':
            opt_section = optarg;
            break;

        case '?':
            usage();
            break;
        }
    }

    for (int i = optind; i < argc; ++i)
        process_file(argv[i]);

    return 0;
}
