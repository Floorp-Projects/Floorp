/* -*- Mode: C++ -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef elf_utils_h__
#define elf_utils_h__

/*

  Random utilities for twiddling ELF files.

 */

#include "elf.h"

/**
 * Verify that an ELF header is sane. Currently hard-coded to x86.
 */
int
elf_verify_header(const Elf32_Ehdr *hdr);

#endif
