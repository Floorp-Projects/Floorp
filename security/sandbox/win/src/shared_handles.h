// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SHARED_HANDLES_H__
#define SANDBOX_SRC_SHARED_HANDLES_H__

#include "base/basictypes.h"

#ifndef HANDLE
// We can provide our own windows compatilble handle definition, but
// in general we want to rely on the client of this api to include
// the proper windows headers. Note that we don't want to bring the
// whole <windows.h> into scope if we don't have to.
typedef void* HANDLE;
#endif

namespace sandbox {

// SharedHandles is a simple class to stash and find windows object handles
// given a raw block of memory which is shared between two processes.
// It addresses the need to communicate a handle value between two windows
// processes given that they are already sharing some memory.
//
// This class is not exposed directly to users of the sanbox API, instead
// we expose the wrapper methods TargetProcess::TransferHandle( ) and
// TargetServices::GetTransferHandle()
//
// Use it for a small number of items, since internaly uses linear seach
//
// The use is very simple. Given a shared memory between proces A and B:
// process A:
//  HANDLE handle = SomeFunction(..);
//  SharedHandles shared_handes;
//  shared_handles.Init(memory)
//  shared_handles.SetHandle(3, handle);
//
// process B:
//  SharedHandles shared_handes;
//  shared_handles.Init(memory)
//  HANDLE handle = shared_handles.GetHandle(3);
//
// Note that '3' in this example is a unique id, that must be agreed before
// transfer
//
// Note2: While this class can be used in a single process, there are
// better alternatives such as STL
//
// Note3: Under windows a kernel object handle in one process does not
// make sense for another process unless there is a DuplicateHandle( )
// call involved which this class DOES NOT do that for you.
//
// Note4: Under windows, shared memory when created is initialized to
// zeros always. If you are not using shared memory it is your responsability
// to zero it for the setter process and to copy it to the getter process.
class SharedHandles {
 public:
  SharedHandles();

  // Initializes the shared memory for use.
  // Pass the shared memory base and size. It will internally compute
  // how many handles can it store. If initialization fails the return value
  // is false.
  bool Init(void* raw_mem, size_t size_bytes);

  // Sets a handle in the shared memory for transfer.
  // Parameters:
  // tag : an integer, different from zero that uniquely identfies the
  // handle to transfer.
  // handle: the handle value associated with 'tag' to tranfer
  // Returns false if there is not enough space in the shared memory for
  // this handle.
  bool SetHandle(uint32 tag, HANDLE handle);

  // Gets a handle previously stored by SetHandle.
  // Parameters:
  // tag: an integer different from zero that uniquely identfies the handle
  // to retrieve.
  // *handle: output handle value if the call was succesful.
  // If a handle with the provided tag is not found the return value is false.
  // If the tag is found the return value is true.
  bool GetHandle(uint32 tag, HANDLE* handle);

 private:
  // A single item is the tuple handle/tag
  struct SharedItem {
    uint32 tag;
    void* item;
  };

  // SharedMem is used to layout the memory as an array of SharedItems
  struct SharedMem {
    size_t max_items;
    SharedItem* items;
  };

  // Finds an Item tuple provided the handle tag.
  // Uses linear search because we expect the number of handles to be
  // small (say less than ~100).
  SharedItem* FindByTag(uint32 tag);

  SharedMem shared_;
  DISALLOW_COPY_AND_ASSIGN(SharedHandles);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SHARED_HANDLES_H__
