/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DMD_h___
#define DMD_h___

#include <string.h>

#include "mozilla/Types.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class JSONWriteFunc;

namespace dmd {

// Mark a heap block as reported by a memory reporter.
MOZ_EXPORT void
Report(const void* aPtr);

// Mark a heap block as reported immediately on allocation.
MOZ_EXPORT void
ReportOnAlloc(const void* aPtr);

// Clears existing reportedness data from any prior runs of the memory
// reporters.  The following sequence should be used.
// - ClearReports()
// - run the memory reporters
// - AnalyzeReports()
// This sequence avoids spurious twice-reported warnings.
MOZ_EXPORT void
ClearReports();

// Determines which heap blocks have been reported, and dumps JSON output
// (via |aWriter|) describing the heap.
//
// The following sample output contains comments that explain the format and
// design choices. The output files can be quite large, so a number of
// decisions were made to minimize size, such as using short property names and
// omitting properties whenever possible.
//
// {
//   // The version number of the format, which will be incremented each time
//   // backwards-incompatible changes are made. A mandatory integer.
//   "version": 1,
//
//   // Information about how DMD was invoked. A mandatory object.
//   "invocation": {
//     // The contents of the $DMD environment variable. A mandatory string.
//     "dmdEnvVar": "1",
//
//     // The value of the --sample-below-size option. A mandatory integer.
//     "sampleBelowSize": 4093
//   },
//
//   // Details of all analyzed heap blocks. A mandatory array.
//   "blockList": [
//     // An example of a non-sampled heap block.
//     {
//       // Requested size, in bytes. In non-sampled blocks this is a
//       // mandatory integer. In sampled blocks this is not present, and the
//       // requested size is equal to the "sampleBelowSize" value. Therefore,
//       // the block is sampled if and only if this property is absent.
//       "req": 3584,
//
//       // Requested slop size, in bytes. This is mandatory if it is non-zero,
//       // but omitted otherwise. Because sampled blocks never have slop, this
//       // property is never present for non-sampled blocks.
//       "slop": 512,
//
//       // The stack trace at which the block was allocated. A mandatory
//       // string which indexes into the "traceTable" object.
//       "alloc": "A"
//     },
//
//     // An example of a sampled heap block.
//     {
//       "alloc": "B",
//
//       // One or more stack traces at which this heap block was reported by a
//       // memory reporter. An optional array. The elements are strings that
//       // index into the "traceTable" object.
//       "reps": ["C"]
//     }
//   ],
//
//   // The stack traces referenced by elements of the "blockList" array. This
//   // could be an array, but making it an object makes it easier to see
//   // which stacks correspond to which references in the "blockList" array.
//   "traceTable": {
//     // Each property corresponds to a stack trace mentioned in the "blocks"
//     // object. Each element is an index into the "frameTable" object.
//     "A": ["D", "E"],
//     "B": ["D", "F"],
//     "C": ["G", "H"]
//   },
//
//   // The stack frames referenced by the "traceTable" object. The
//   // descriptions can be quite long, so they are stored separately from the
//   // "traceTable" object so that each one only has to be written once.
//   // This could also be an array, but again, making it an object makes it
//   // easier to see which frames correspond to which references in the
//   // "traceTable" object.
//   "frameTable": {
//     // Each property key is a frame key mentioned in the "traceTable" object.
//     // Each property value is a string containing a frame description. Each
//     // frame description must be in a format recognized by the stack-fixing
//     // scripts (e.g. fix_linux_stack.py), which require a frame number at
//     // the start. Because each stack frame description in this table can
//     // be shared between multiple stack traces, we use a dummy value of
//     // #00. The proper frame number can be reconstructed later by scripts
//     // that output stack traces in a conventional non-shared format.
//     "D": "#00: foo (Foo.cpp:123)",
//     "E": "#00: bar (Bar.cpp:234)",
//     "F": "#00: baz (Baz.cpp:345)",
//     "G": "#00: quux (Quux.cpp:456)",
//     "H": "#00: quuux (Quux.cpp:567)"
//   }
// }
MOZ_EXPORT void
AnalyzeReports(mozilla::UniquePtr<mozilla::JSONWriteFunc>);

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

// Prints a status message prefixed with "DMD[<pid>]". Use sparingly.
MOZ_EXPORT void
StatusMsg(const char* aFmt, ...);

// Indicates whether or not DMD is running.
MOZ_EXPORT bool
IsRunning();

// Sets the sample-below size. Only used for testing purposes.
MOZ_EXPORT void
SetSampleBelowSize(size_t aSize);

// Clears all records of live allocations. Only used for testing purposes.
MOZ_EXPORT void
ClearBlocks();

} // namespace mozilla
} // namespace dmd

#endif /* DMD_h___ */
