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

// minidump_file_writer.h:  Implements file-based minidump generation

#ifndef CLIENT_MINIDUMP_FILE_WRITER_H__
#define CLIENT_MINIDUMP_FILE_WRITER_H__

#include <string>

#include "google_airbag/common/minidump_format.h"

namespace google_airbag {

class MinidumpFileWriter {
 public:
  // Invalid MDRVA (Minidump Relative Virtual Address)
  // returned on failed allocation
  static const MDRVA kInvalidMDRVA = static_cast<MDRVA>(-1);

  MinidumpFileWriter();
  ~MinidumpFileWriter();

  // Open |path| as the destination of the minidump data.  Any existing file
  // will be overwritten.
  // Return true on success, or false on failure
  bool Open(const std::string &path);

  // Close the current file
  // Return true on success, or false on failure
  bool Close();

  // Write |str| to a MDString.
  // |str| is expected to be either UTF-16 or UTF-32 depending on the size
  // of wchar_t.
  // Maximum |length| of characters to copy from |str|, or specify 0 to use the
  // entire NULL terminated string.  Copying will stop at the first NULL.
  // |location| the allocated location
  // Return true on success, or false on failure
  bool WriteString(const wchar_t *str, unsigned int length,
                   MDLocationDescriptor *location);

  // Similar to above with |str| as an UTF-8 encoded string
  bool WriteString(const char *str, unsigned int length,
                   MDLocationDescriptor *location);

  // Write |size| bytes starting at |src| into the current position.
  // Return true on success and set |output| to position, or false on failure
  bool WriteMemory(const void *src, size_t size, MDMemoryDescriptor *output);

  // Copies |size| bytes from |src| to |position|
  // Return true on success, or false on failure
  bool Copy(MDRVA position, const void *src, ssize_t size);

  // Return the current position for writing to the minidump
  MDRVA position() const { return position_; }

 private:
  friend class UntypedMDRVA;

  // Allocates an area of |size| bytes.
  // Returns the position of the allocation, or kInvalidMDRVA if it was
  // unable to allocate the bytes.
  MDRVA Allocate(size_t size);

  // The file descriptor for the output file
  int file_;

  // Current position in buffer
  MDRVA position_;

  // Current allocated size
  size_t size_;
};

// Represents an untyped allocated chunk
class UntypedMDRVA {
 public:
  explicit UntypedMDRVA(MinidumpFileWriter *writer)
      : writer_(writer),
        position_(writer->position()),
        size_(0) {}

  // Allocates |size| bytes.  Must not call more than once.
  // Return true on success, or false on failure
  bool Allocate(size_t size);

  // Returns the current position or kInvalidMDRVA if allocation failed
  MDRVA position() const { return position_; }

  // Number of bytes allocated
  size_t size() const { return size_; }

  // Return size and position
  MDLocationDescriptor location() const {
    MDLocationDescriptor location = { size_, position_ };
    return location;
  }

  // Copy |size| bytes starting at |src| into the minidump at |position|
  // Return true on success, or false on failure
  bool Copy(MDRVA position, const void *src, size_t size);

  // Copy |size| bytes from |src| to the current position
  bool Copy(const void *src, size_t size) {
    return Copy(position_, src, size);
  }

 protected:
  // Writer we associate with
  MinidumpFileWriter *writer_;

  // Position of the start of the data
  MDRVA position_;

  // Allocated size
  size_t size_;
};

// Represents a Minidump object chunk.  Additional memory can be allocated at
// the end of the object as a:
// - single allocation
// - Array of MDType objects
// - A MDType object followed by an array
template<typename MDType>
class TypedMDRVA : public UntypedMDRVA {
 public:
  // Constructs an unallocated MDRVA
  explicit TypedMDRVA(MinidumpFileWriter *writer)
      : UntypedMDRVA(writer),
        data_(),
        allocation_state_(UNALLOCATED) {}

  ~TypedMDRVA() {
    // Ensure that the data_ object is written out
    if (allocation_state_ != ARRAY)
      Flush();
  }

  // Address of object data_ of MDType.  This is not declared const as the
  // typical usage will be to access the underlying |data_| object as to
  // alter its contents.
  MDType *get() { return &data_; }

  // Allocates sizeof(MDType) bytes.
  // Must not call more than once.
  // Return true on success, or false on failure
  bool Allocate();

  // Allocates sizeof(MDType) + |additional| bytes.
  // Must not call more than once.
  // Return true on success, or false on failure
  bool Allocate(size_t additional);

  // Allocate an array of |count| elements of MDType.
  // Must not call more than once.
  // Return true on success, or false on failure
  bool AllocateArray(size_t count);

  // Allocate an array of |count| elements of |size| after object of MDType
  // Must not call more than once.
  // Return true on success, or false on failure
  bool AllocateObjectAndArray(unsigned int count, size_t size);

  // Copy |item| to |index|
  // Must have been allocated using AllocateArray().
  // Return true on success, or false on failure
  bool CopyIndex(unsigned int index, MDType *item);

  // Copy |size| bytes starting at |str| to |index|
  // Must have been allocated using AllocateObjectAndArray().
  // Return true on success, or false on failure
  bool CopyIndexAfterObject(unsigned int index, void *src, size_t size);

  // Write data_
  bool Flush();

 private:
  enum AllocationState {
    UNALLOCATED = 0,
    SINGLE_OBJECT,
    ARRAY,
    SINGLE_OBJECT_WITH_ARRAY
  };

  MDType data_;
  AllocationState allocation_state_;
};

}  // namespace google_airbag

#endif  // CLIENT_MINIDUMP_FILE_WRITER_H__
