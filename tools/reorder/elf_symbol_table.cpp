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
 * The Original Code is ``elf_symbol_table''
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corp.  Portions created by the Initial Developer are
 * Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
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

#include <string>
#include <fstream>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "elf_symbol_table.h"
#include "elf_utils.h"

int
elf_symbol_table::init(const char *name)
{
    // Open the file readonly.
    m_fd = open(name, O_RDONLY);
    if (m_fd < 0) {
        perror(name);
        return m_fd;
    }

    // Get its size.
    struct stat statbuf;
    if (fstat(m_fd, &statbuf) < 0) {
        perror(name);
        return -1;
    }

    m_size = statbuf.st_size;

    // Memory map it.
    m_mapping = mmap(0, m_size, PROT_READ, MAP_SHARED, m_fd, 0);
    if (m_mapping == MAP_FAILED) {
        perror(name);
        return -1;
    }

    // Make sure it's an ELF header.
    const Elf32_Ehdr *ehdr = reinterpret_cast<const Elf32_Ehdr *>(m_mapping);
    if (elf_verify_header(ehdr) < 0)
        return -1;

    const char *mapping = reinterpret_cast<const char *>(m_mapping);

    // Find the section headers
    const Elf32_Shdr *shdrs
        = reinterpret_cast<const Elf32_Shdr *>(mapping + ehdr->e_shoff);

    // find the section header string table, .shstrtab
    const Elf32_Shdr *shstrtabsh = shdrs + ehdr->e_shstrndx;
    const char *shstrtab = mapping + shstrtabsh->sh_offset;

    // parse the sections we care about
    int shndx = 0;
    const Elf32_Shdr *shlimit = shdrs + ehdr->e_shnum;
    for (const Elf32_Shdr *shdr = shdrs; shdr < shlimit; ++shdr, ++shndx) {
        basic_string<char> name(shstrtab + shdr->sh_name);
        if (name == ".symtab") {
            m_symbols = reinterpret_cast<const Elf32_Sym *>(mapping + shdr->sh_offset);
            m_nsymbols = shdr->sh_size / sizeof(Elf32_Sym);
        }
        else if (name == ".strtab") {
            m_strtab = mapping + shdr->sh_offset;
        }
        else if (name == ".text") {
            m_text_shndx = shndx;
        }
    }

    // Parse the symbol table
    const Elf32_Sym *limit = m_symbols + m_nsymbols;
    for (const Elf32_Sym *sym = m_symbols; sym < limit; ++sym) {
        if (is_function(sym)) {
#ifdef DEBUG
            hex(cout);
            cout << sym->st_value << endl;
#endif
            m_rsymtab.put(sym->st_value, sym->st_value + sym->st_size, sym);
        }
    }

    return 0;
}

int
elf_symbol_table::finish()
{
    if (m_mapping != MAP_FAILED) {
        munmap(m_mapping, m_size);
        m_mapping = MAP_FAILED;
    }

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }

    return 0;
}

const Elf32_Sym *
elf_symbol_table::lookup(unsigned int addr) const
{
    rsymtab_t::const_iterator result = m_rsymtab.get(addr);
    return result != m_rsymtab.end() ? reinterpret_cast<const Elf32_Sym *>(*result) : 0;
}

const char *
elf_symbol_table::get_symbol_name(const Elf32_Sym *sym) const
{
    return m_strtab + sym->st_name;
}
