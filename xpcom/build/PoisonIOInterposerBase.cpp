/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Mutex.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"

#include <algorithm>

#include "PoisonIOInterposer.h"

#ifdef MOZ_REPLACE_MALLOC
#include "replace_malloc_bridge.h"
#endif

// Auxiliary method to convert file descriptors to ids
#if defined(XP_WIN32)
#include <io.h>
inline intptr_t
FileDescriptorToHandle(int aFd)
{
  return _get_osfhandle(aFd);
}
#else
inline intptr_t
FileDescriptorToHandle(int aFd)
{
  return aFd;
}
#endif /* if not XP_WIN32 */

using namespace mozilla;

namespace {
struct DebugFilesAutoLockTraits
{
  typedef PRLock* type;
  typedef const PRLock* const_type;
  static const_type empty() { return nullptr; }
  static void release(type aL) { PR_Unlock(aL); }
};

class DebugFilesAutoLock : public Scoped<DebugFilesAutoLockTraits>
{
  static PRLock* Lock;
public:
  static void Clear();
  static PRLock* getDebugFileIDsLock()
  {
    // On windows this static is not thread safe, but we know that the first
    // call is from
    // * An early registration of a debug FD or
    // * The call to InitWritePoisoning.
    // Since the early debug FDs are logs created early in the main thread
    // and no writes are trapped before InitWritePoisoning, we are safe.
    if (!Lock) {
      Lock = PR_NewLock();
    }

    // We have to use something lower level than a mutex. If we don't, we
    // can get recursive in here when called from logging a call to free.
    return Lock;
  }

  DebugFilesAutoLock()
    : Scoped<DebugFilesAutoLockTraits>(getDebugFileIDsLock())
  {
    PR_Lock(get());
  }
};

PRLock* DebugFilesAutoLock::Lock;
void
DebugFilesAutoLock::Clear()
{
  MOZ_ASSERT(Lock != nullptr);
  Lock = nullptr;
}

// The ChunkedList<T> class implements, at the high level, a non-iterable
// list of instances of T. Its goal is to be somehow minimalist for the
// use case of storing the debug files handles here, with the property of
// not requiring a lock to look up whether it contains a specific value.
// It is also chunked in blocks of chunk_size bytes so that its
// initialization doesn't require a memory allocation, while keeping the
// possibility to increase its size as necessary. Note that chunks are
// never deallocated (except in the destructor).
// All operations are essentially O(N) but N is not expected to be large
// enough to matter.
template <typename T, size_t chunk_size=64>
class ChunkedList {
  struct ListChunk {
    static const size_t kLength = \
      (chunk_size - sizeof(ListChunk*)) / sizeof(mozilla::Atomic<T>);

    mozilla::Atomic<T> mElements[kLength];
    mozilla::UniquePtr<ListChunk> mNext;

    ListChunk() : mNext(nullptr) {}
  };

  ListChunk mList;
  mozilla::Atomic<size_t> mLength;

public:
  ChunkedList() : mLength(0) {}

  ~ChunkedList() {
    // There can be writes happening after this destructor runs, so keep
    // the list contents and don't reset mLength. But if there are more
    // elements left than the first chunk can hold, then all hell breaks
    // loose for any write that would happen after that because any extra
    // chunk would be deallocated, so just crash in that case.
    MOZ_RELEASE_ASSERT(mLength <= ListChunk::kLength);
  }

  // Add an element at the end of the last chunk of the list. Create a new
  // chunk if there is not enough room.
  // This is not thread-safe with another thread calling Add or Remove.
  void Add(T aValue)
  {
    ListChunk *list = &mList;
    size_t position = mLength;
    for (; position >= ListChunk::kLength; position -= ListChunk::kLength) {
      if (!list->mNext) {
        list->mNext.reset(new ListChunk());
      }
      list = list->mNext.get();
    }
    // Use an order of operations that ensures any racing Contains call
    // can't be hurt.
    list->mElements[position] = aValue;
    mLength++;
  }

  // Remove an element from the list by replacing it with the last element
  // of the list, and then shrinking the list.
  // This is not thread-safe with another thread calling Add or Remove.
  void Remove(T aValue)
  {
    if (!mLength) {
      return;
    }
    ListChunk *list = &mList;
    size_t last = mLength - 1;
    do {
      size_t position = 0;
      // Look for an element matching the given value.
      for (; position < ListChunk::kLength; position++) {
        if (aValue == list->mElements[position]) {
          ListChunk *last_list = list;
          // Look for the last element in the list, starting from where we are
          // instead of starting over.
          for (; last >= ListChunk::kLength; last -= ListChunk::kLength) {
            last_list = last_list->mNext.get();
          }
          // Use an order of operations that ensures any racing Contains call
          // can't be hurt.
          T value = last_list->mElements[last];
          list->mElements[position] = value;
          mLength--;
          return;
        }
      }
      last -= ListChunk::kLength;
      list = list->mNext.get();
    } while (list);
  }

  // Returns whether the list contains the given value. It is meant to be safe
  // to use without locking, with the tradeoff of being not entirely accurate
  // if another thread adds or removes an element while this function runs.
  bool Contains(T aValue)
  {
    ListChunk *list = &mList;
    // Fix the range of the lookup to whatever the list length is when the
    // function is called.
    size_t length = mLength;
    do {
      size_t list_length = ListChunk::kLength;
      list_length = std::min(list_length, length);
      for (size_t position = 0; position < list_length; position++) {
        if (aValue == list->mElements[position]) {
          return true;
        }
      }
      length -= ListChunk::kLength;
      list = list->mNext.get();
    } while (list);

    return false;
  }
};

typedef ChunkedList<intptr_t> FdList;

// Return a list used to hold the IDs of the current debug files. On unix
// an ID is a file descriptor. On Windows it is a file HANDLE.
FdList&
getDebugFileIDs()
{
  static FdList DebugFileIDs;
  return DebugFileIDs;
}


} // namespace

namespace mozilla {

// Auxiliary Method to test if a file descriptor is registered to be ignored
// by the poisoning IO interposer
bool
IsDebugFile(intptr_t aFileID)
{
  return getDebugFileIDs().Contains(aFileID);
}

} // namespace mozilla

extern "C" {

void
MozillaRegisterDebugHandle(intptr_t aHandle)
{
  DebugFilesAutoLock lockedScope;
  FdList& DebugFileIDs = getDebugFileIDs();
  MOZ_ASSERT(!DebugFileIDs.Contains(aHandle));
  DebugFileIDs.Add(aHandle);
}

void
MozillaRegisterDebugFD(int aFd)
{
  MozillaRegisterDebugHandle(FileDescriptorToHandle(aFd));
}

void
MozillaRegisterDebugFILE(FILE* aFile)
{
  int fd = fileno(aFile);
  if (fd == 1 || fd == 2) {
    return;
  }
  MozillaRegisterDebugFD(fd);
}

void
MozillaUnRegisterDebugHandle(intptr_t aHandle)
{
  DebugFilesAutoLock lockedScope;
  FdList& DebugFileIDs = getDebugFileIDs();
  MOZ_ASSERT(DebugFileIDs.Contains(aHandle));
  DebugFileIDs.Remove(aHandle);
}

void
MozillaUnRegisterDebugFD(int aFd)
{
  MozillaUnRegisterDebugHandle(FileDescriptorToHandle(aFd));
}

void
MozillaUnRegisterDebugFILE(FILE* aFile)
{
  int fd = fileno(aFile);
  if (fd == 1 || fd == 2) {
    return;
  }
  fflush(aFile);
  MozillaUnRegisterDebugFD(fd);
}

}  // extern "C"

#ifdef MOZ_REPLACE_MALLOC
void
DebugFdRegistry::RegisterHandle(intptr_t aHandle)
{
  MozillaRegisterDebugHandle(aHandle);
}

void
DebugFdRegistry::UnRegisterHandle(intptr_t aHandle)
{
  MozillaUnRegisterDebugHandle(aHandle);
}
#endif
