/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fstream>
#include "elf_utils.h"

int
elf_verify_header(const Elf32_Ehdr *hdr)
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

