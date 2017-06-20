/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MINIDUMP_CALLBACK_H__
#define MINIDUMP_CALLBACK_H__

#include <windows.h>
#include <dbghelp.h>

#include <list>

namespace google_breakpad {

// These entries store a list of memory regions that the client wants included
// in the minidump.
struct AppMemory {
  ULONG64 ptr;
  ULONG length;

  bool operator==(const struct AppMemory& other) const {
    return ptr == other.ptr;
  }

  bool operator==(const void* other) const {
    return ptr == reinterpret_cast<ULONG64>(other);
  }
};
typedef std::list<AppMemory> AppMemoryList;

// This is passed as the context to the MinidumpWriteDump callback.
typedef struct {
  AppMemoryList::const_iterator iter;
  AppMemoryList::const_iterator end;
} MinidumpCallbackContext;

// This function is used as a callback when calling MinidumpWriteDump,
// in order to add additional memory regions to the dump.
BOOL CALLBACK MinidumpWriteDumpCallback(
    PVOID context,
    const PMINIDUMP_CALLBACK_INPUT callback_input,
    PMINIDUMP_CALLBACK_OUTPUT callback_output);

}  // namespace google_breakpad

#endif

