/* -*- Mode: C++ -*- */
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

#ifndef elf_symbol_table_h__
#define elf_symbol_table_h__

/*

  Utility class for reading ELF symbol tables.

 */

#include <elf.h>
#include <sys/mman.h>

#include "interval_map.h"

class elf_symbol_table {
protected:
    int              m_fd;
    void            *m_mapping;
    size_t           m_size;
    const char      *m_strtab;
    const Elf32_Sym *m_symbols;
    int              m_nsymbols;
    int              m_text_shndx;

    typedef interval_map<unsigned int, const Elf32_Sym *> rsymtab_t;
    rsymtab_t        m_rsymtab;

    int verify_elf_header(const Elf32_Ehdr *hdr);
    int finish();

public:
    elf_symbol_table()
        : m_fd(-1),
          m_mapping(MAP_FAILED),
          m_size(0),
          m_strtab(0),
          m_symbols(0),
          m_nsymbols(0),
          m_text_shndx(0) {}

    ~elf_symbol_table() { finish(); }

    /**
     * Read the symbol table information from the specified file. (This will
     * memory-map the file.)
     *
     * Currently, only symbols from the `.text' section are loaded.
     */
    int init(const char *file);

    /**
     * Determine what symbol the specified image address corresponds
     * to. Addresses need not refer to a symbol's start address:
     * symbol size information is used to reverse-map the address.
     */
    const Elf32_Sym *lookup(unsigned int addr) const;

    /**
     * Determine the symbol name for the specified symbol.
     */
    const char *get_symbol_name(const Elf32_Sym *sym) const;

    /**
     * Return `true' if the specified symbol is a function.
     */
    bool is_function(const Elf32_Sym *sym) const {
        return (sym->st_size > 0) &&
            (ELF32_ST_TYPE(sym->st_info) == STT_FUNC) &&
            (sym->st_shndx == m_text_shndx); }

    typedef const Elf32_Sym *const_iterator;

    const_iterator begin() const { return const_iterator(m_symbols); }
    const_iterator end() const { return const_iterator(m_symbols + m_nsymbols); }
};

#endif // elf_symbol_table_h__
