/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

// Copyright (c) 2006, 2011, 2012 Google Inc.
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

// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/linux/dump_symbols.h

#ifndef LulElfExt_h
#define LulElfExt_h

// These two functions are the external interface to the
// ELF/Dwarf/EXIDX reader.

#include "LulMainInt.h"

using lul::SecMap;

namespace lul {

// Find all the unwind information in OBJ_FILE, an ELF executable
// or shared library, and add it to SMAP.
bool ReadSymbolData(const std::string& obj_file,
                    const std::vector<std::string>& debug_dirs,
                    SecMap* smap,
                    void* rx_avma, size_t rx_size,
                    void (*log)(const char*));

// The same as ReadSymbolData, except that OBJ_FILE is assumed to
// point to a mapped-in image of OBJ_FILENAME.
bool ReadSymbolDataInternal(const uint8_t* obj_file,
                            const std::string& obj_filename,
                            const std::vector<std::string>& debug_dirs,
                            SecMap* smap,
                            void* rx_avma, size_t rx_size,
                            void (*log)(const char*));

}  // namespace lul

#endif // LulElfExt_h
