// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// macho_utilties.cc: Utilities for dealing with mach-o files
//
// Author: Dave Camp

#include "common/mac/macho_utilities.h"

void breakpad_swap_uuid_command(struct breakpad_uuid_command *uc,
                                enum NXByteOrder target_byte_order)
{
  uc->cmd = NXSwapLong(uc->cmd);
  uc->cmdsize = NXSwapLong(uc->cmdsize);
}

void breakpad_swap_segment_command_64(struct segment_command_64 *sg,
                                      enum NXByteOrder target_byte_order)
{
  sg->cmd = NXSwapLong(sg->cmd);
  sg->cmdsize = NXSwapLong(sg->cmdsize);

  sg->vmaddr = NXSwapLongLong(sg->vmaddr);
  sg->vmsize = NXSwapLongLong(sg->vmsize);
  sg->fileoff = NXSwapLongLong(sg->fileoff);
  sg->filesize = NXSwapLongLong(sg->filesize);

  sg->maxprot = NXSwapLong(sg->maxprot);
  sg->initprot = NXSwapLong(sg->initprot);
  sg->nsects = NXSwapLong(sg->nsects);
  sg->flags = NXSwapLong(sg->flags);
}

void breakpad_swap_mach_header_64(struct mach_header_64 *mh,
                                  enum NXByteOrder target_byte_order)
{
  mh->magic = NXSwapLong(mh->magic);
  mh->cputype = NXSwapLong(mh->cputype);
  mh->cpusubtype = NXSwapLong(mh->cpusubtype);
  mh->filetype = NXSwapLong(mh->filetype);
  mh->ncmds = NXSwapLong(mh->ncmds);
  mh->sizeofcmds = NXSwapLong(mh->sizeofcmds);
  mh->flags = NXSwapLong(mh->flags);
  mh->reserved = NXSwapLong(mh->reserved);
}

void breakpad_swap_section_64(struct section_64 *s,
                              uint32_t nsects,
                              enum NXByteOrder target_byte_order)
{
  for (uint32_t i = 0; i < nsects; i++) {
    s[i].addr = NXSwapLongLong(s[i].addr);
    s[i].size = NXSwapLongLong(s[i].size);

    s[i].offset = NXSwapLong(s[i].offset);
    s[i].align = NXSwapLong(s[i].align);
    s[i].reloff = NXSwapLong(s[i].reloff);
    s[i].nreloc = NXSwapLong(s[i].nreloc);
    s[i].flags = NXSwapLong(s[i].flags);
    s[i].reserved1 = NXSwapLong(s[i].reserved1);
    s[i].reserved2 = NXSwapLong(s[i].reserved2);
  }
}
