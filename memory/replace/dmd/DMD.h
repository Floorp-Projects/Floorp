/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DMD_h___
#define DMD_h___

#include <stdarg.h>
#include <string.h>

#include "mozilla/Types.h"

namespace mozilla {
namespace dmd {

// Mark a heap block as reported by a memory reporter.
MOZ_EXPORT void
Report(const void* aPtr);

// Mark a heap block as reported immediately on allocation.
MOZ_EXPORT void
ReportOnAlloc(const void* aPtr);

class Writer
{
public:
  typedef void (*WriterFun)(void* aWriteState, const char* aFmt, va_list aAp);

  Writer(WriterFun aWriterFun, void* aWriteState)
    : mWriterFun(aWriterFun), mWriteState(aWriteState)
  {}

  void Write(const char* aFmt, ...) const;

private:
  WriterFun mWriterFun;
  void*     mWriteState;
};

// Clears existing reportedness data from any prior runs of the memory
// reporters.  The following sequence should be used.
// - ClearReports()
// - run the memory reporters
// - Dump()
// This sequence avoids spurious twice-reported warnings.
MOZ_EXPORT void
ClearReports();

// Checks which heap blocks have been reported, and dumps a human-readable
// summary (via |aWrite|).  If |aWrite| is nullptr it will dump to stderr.
// Beware:  this output may have very long lines.
MOZ_EXPORT void
Dump(Writer aWriter);

// A useful |WriterFun|.  If |fp| is a FILE* you want |Dump|'s output to be
// written to, call:
//
//   dmd::Writer writer(FpWrite, fp);
//   dmd::Dump(writer);
MOZ_EXPORT void
FpWrite(void* aFp, const char* aFmt, va_list aAp);

struct Sizes
{
  size_t mStackTracesUsed;
  size_t mStackTracesUnused;
  size_t mStackTraceTable;
  size_t mBlockTable;

  Sizes() { Clear(); }
  void Clear() { memset(this, 0, sizeof(Sizes)); }
};

// Gets the size of various data structures.  Used to implement a memory
// reporter for DMD.
MOZ_EXPORT void
SizeOf(Sizes* aSizes);

// Indicates whether or not DMD is running.
MOZ_EXPORT bool
IsRunning();

} // namespace mozilla
} // namespace dmd

#endif /* DMD_h___ */
