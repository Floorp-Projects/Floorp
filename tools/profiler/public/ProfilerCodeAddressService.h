/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerCodeAddressService_h
#define ProfilerCodeAddressService_h

#include "CodeAddressService.h"

namespace mozilla {

// This SymbolTable struct, and the CompactSymbolTable struct in the
// profiler rust module, have the exact same memory layout.
// nsTArray and ThinVec are FFI-compatible, because the thin-vec crate is
// being compiled with the "gecko-ffi" feature enabled.
struct SymbolTable {
  SymbolTable() = default;
  SymbolTable(SymbolTable&& aOther) = default;

  nsTArray<uint32_t> mAddrs;
  nsTArray<uint32_t> mIndex;
  nsTArray<uint8_t> mBuffer;
};

}  // namespace mozilla

/**
 * Cache and look up function symbol names.
 *
 * We don't template this on AllocPolicy since we need to use nsTArray in
 * SymbolTable above, which doesn't work with AllocPolicy.  (We can't switch
 * to Vector, as we would lose FFI compatibility with ThinVec.)
 */
class ProfilerCodeAddressService : public mozilla::CodeAddressService<> {
 public:
  // Like GetLocation, but only returns the symbol name.
  bool GetFunction(const void* aPc, nsACString& aResult);

 private:
#ifdef XP_LINUX
  // Map of library names (owned by mLibraryStrings) to SymbolTables filled
  // in by profiler_get_symbol_table.
  mozilla::HashMap<const char*, mozilla::SymbolTable,
                   mozilla::DefaultHasher<const char*>, AllocPolicy>
      mSymbolTables;
#endif
};

#endif  // ProfilerCodeAddressService_h
