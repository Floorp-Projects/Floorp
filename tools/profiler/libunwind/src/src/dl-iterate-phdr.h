/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct dl_phdr_info {
        Elf_W(Addr)        dlpi_addr;  /* Base address of object */
        /*const*/ char   *dlpi_name;  /* (Null-terminated) name of
                                         object */
        const Elf_W(Phdr) *dlpi_phdr;  /* Pointer to array of
                                         ELF program headers
                                         for this object */
        Elf_W(Half)        dlpi_phnum; /* # of items in dlpi_phdr */
};

int
dl_iterate_phdr (int (*callback) (struct dl_phdr_info *info, size_t size, void *data),
                 void *data);

