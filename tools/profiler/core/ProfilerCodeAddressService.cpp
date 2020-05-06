/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerCodeAddressService.h"

#include "platform.h"
#include "mozilla/StackWalk.h"

using namespace mozilla;

#if defined(XP_LINUX) || defined(XP_FREEBSD)
static char* SearchSymbolTable(SymbolTable& aTable, uint32_t aOffset) {
  size_t index;
  bool exact =
      BinarySearch(aTable.mAddrs, 0, aTable.mAddrs.Length(), aOffset, &index);

  if (index == 0 && !exact) {
    // Our offset is before the first symbol in the table; no result.
    return nullptr;
  }

  // Extract the (mangled) symbol name out of the string table.
  auto strings = reinterpret_cast<char*>(aTable.mBuffer.Elements());
  nsCString symbol;
  symbol.Append(strings + aTable.mIndex[index - 1],
                aTable.mIndex[index] - aTable.mIndex[index - 1]);

  // First try demangling as a Rust identifier.
  char demangled[1024];
  if (!profiler_demangle_rust(symbol.get(), demangled,
                              ArrayLength(demangled))) {
    // Then as a C++ identifier.
    DemangleSymbol(symbol.get(), demangled, ArrayLength(demangled));
  }
  demangled[ArrayLength(demangled) - 1] = '\0';

  // Use the mangled name if we didn't successfully demangle.
  return strdup(demangled[0] != '\0' ? demangled : symbol.get());
}
#endif

bool ProfilerCodeAddressService::GetFunction(const void* aPc,
                                             nsACString& aResult) {
  Entry& entry = GetEntry(aPc);

#if defined(XP_LINUX) || defined(XP_FREEBSD)
  // On Linux, most symbols will not be found by the MozDescribeCodeAddress call
  // that GetEntry does.  So we read the symbol table directly from the ELF
  // image.

  // SymbolTable currently assumes library offsets will not be larger than
  // 4 GiB.
  if (entry.mLOffset <= 0xFFFFFFFF && !entry.mFunction) {
    auto p = mSymbolTables.lookupForAdd(entry.mLibrary);
    if (!p) {
      if (!mSymbolTables.add(p, entry.mLibrary, SymbolTable())) {
        MOZ_CRASH("ProfilerCodeAddressService OOM");
      }
      profiler_get_symbol_table(entry.mLibrary, nullptr, &p->value());
    }
    entry.mFunction =
        SearchSymbolTable(p->value(), static_cast<uint32_t>(entry.mLOffset));
  }
#endif

  if (!entry.mFunction || entry.mFunction[0] == '\0') {
    return false;
  }

  aResult = nsDependentCString(entry.mFunction);
  return true;
}
