// Copyright (c) 2009, Google Inc.
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

#ifndef CLIENT_LINUX_MINIDUMP_WRITER_MINIDUMP_WRITER_H_
#define CLIENT_LINUX_MINIDUMP_WRITER_MINIDUMP_WRITER_H_

#include <list>
#include <utility>

#include <stdint.h>
#include <unistd.h>

#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

// A list of <MappingInfo, GUID>
typedef std::pair<struct MappingInfo, u_int8_t[sizeof(MDGUID)]> MappingEntry;
typedef std::list<MappingEntry> MappingList;

// Write a minidump to the filesystem. This function does not malloc nor use
// libc functions which may. Thus, it can be used in contexts where the state
// of the heap may be corrupt.
//   filename: the filename to write to. This is opened O_EXCL and fails if
//     open fails.
//   crashing_process: the pid of the crashing process. This must be trusted.
//   blob: a blob of data from the crashing process. See exception_handler.h
//   blob_size: the length of |blob|, in bytes
//
// Returns true iff successful.
bool WriteMinidump(const char* filename, pid_t crashing_process,
                   const void* blob, size_t blob_size);

// Alternate form of WriteMinidump() that works with processes that
// are not expected to have crashed.  If |process_blamed_thread| is
// meaningful, it will be the one from which a crash signature is
// extracted.  It is not expected that this function will be called
// from a compromised context, but it is safe to do so.
bool WriteMinidump(const char* filename, pid_t process,
                   pid_t process_blamed_thread);

// This overload also allows passing a list of known mappings.
bool WriteMinidump(const char* filename, pid_t crashing_process,
                   const void* blob, size_t blob_size,
                   const MappingList& mappings);

}  // namespace google_breakpad

#endif  // CLIENT_LINUX_MINIDUMP_WRITER_MINIDUMP_WRITER_H_
