/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CodeAddressService_h__
#define CodeAddressService_h__

#include "mozilla/AllocPolicy.h"
#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StackWalk.h"
#include "mozilla/Types.h"

namespace mozilla {

namespace detail {

template <class AllocPolicy>
class CodeAddressServiceAllocPolicy : public AllocPolicy {
 public:
  char* strdup_(const char* aStr) {
    char* s = AllocPolicy::template pod_malloc<char>(strlen(aStr) + 1);
    if (!s) {
      MOZ_CRASH("CodeAddressService OOM");
    }
    strcpy(s, aStr);
    return s;
  }
};

// Default implementation of DescribeCodeAddressLock.
struct DefaultDescribeCodeAddressLock {
  static void Unlock() {}
  static void Lock() {}
  // Because CodeAddressService asserts that IsLocked() is true, returning true
  // here is a sensible default when there is no relevant lock.
  static bool IsLocked() { return true; }
};

}  // namespace detail

// This class is used to print details about code locations.
//
// |AllocPolicy_| must adhere to the description in mfbt/AllocPolicy.h.
//
// |DescribeCodeAddressLock| is needed when the callers may be holding a lock
// used by MozDescribeCodeAddress.  |DescribeCodeAddressLock| must implement
// static methods IsLocked(), Unlock() and Lock().
template <class AllocPolicy_ = MallocAllocPolicy,
          class DescribeCodeAddressLock =
              detail::DefaultDescribeCodeAddressLock>
class CodeAddressService
    : private detail::CodeAddressServiceAllocPolicy<AllocPolicy_> {
 protected:
  // GetLocation() is the key function in this class.  It's basically a wrapper
  // around MozDescribeCodeAddress.
  //
  // However, MozDescribeCodeAddress is very slow on some platforms, and we
  // have lots of repeated (i.e. same PC) calls to it.  So we do some caching
  // of results.  Each cached result includes two strings (|mFunction| and
  // |mLibrary|), so we also optimize them for space in the following ways.
  //
  // - The number of distinct library names is small, e.g. a few dozen.  There
  //   is lots of repetition, especially of libxul.  So we intern them in their
  //   own table, which saves space over duplicating them for each cache entry.
  //
  // - The number of distinct function names is much higher, so we duplicate
  //   them in each cache entry.  That's more space-efficient than interning
  //   because entries containing single-occurrence function names are quickly
  //   overwritten, and their copies released.  In addition, empty function
  //   names are common, so we use nullptr to represent them compactly.

  using AllocPolicy = detail::CodeAddressServiceAllocPolicy<AllocPolicy_>;
  using StringHashSet = HashSet<const char*, CStringHasher, AllocPolicy>;

  StringHashSet mLibraryStrings;

  struct Entry : private AllocPolicy {
    const void* mPc;
    char* mFunction;       // owned by the Entry;  may be null
    const char* mLibrary;  // owned by mLibraryStrings;  never null
                           //   in a non-empty entry is in use
    ptrdiff_t mLOffset;
    char* mFileName;  // owned by the Entry; may be null
    uint32_t mLineNo : 31;
    uint32_t mInUse : 1;  // is the entry used?

    Entry()
        : mPc(0),
          mFunction(nullptr),
          mLibrary(nullptr),
          mLOffset(0),
          mFileName(nullptr),
          mLineNo(0),
          mInUse(0) {}

    ~Entry() {
      // We don't free mLibrary because it's externally owned.
      AllocPolicy::free_(mFunction);
      AllocPolicy::free_(mFileName);
    }

    void Replace(const void* aPc, const char* aFunction, const char* aLibrary,
                 ptrdiff_t aLOffset, const char* aFileName,
                 unsigned long aLineNo) {
      mPc = aPc;

      // Convert "" to nullptr.  Otherwise, make a copy of the name.
      AllocPolicy::free_(mFunction);
      mFunction = !aFunction[0] ? nullptr : AllocPolicy::strdup_(aFunction);
      AllocPolicy::free_(mFileName);
      mFileName = !aFileName[0] ? nullptr : AllocPolicy::strdup_(aFileName);

      mLibrary = aLibrary;
      mLOffset = aLOffset;
      mLineNo = aLineNo;

      mInUse = 1;
    }

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      // Don't measure mLibrary because it's externally owned.
      size_t n = 0;
      n += aMallocSizeOf(mFunction);
      n += aMallocSizeOf(mFileName);
      return n;
    }
  };

  const char* InternLibraryString(const char* aString) {
    auto p = mLibraryStrings.lookupForAdd(aString);
    if (p) {
      return *p;
    }

    const char* newString = AllocPolicy::strdup_(aString);
    if (!mLibraryStrings.add(p, newString)) {
      MOZ_CRASH("CodeAddressService OOM");
    }
    return newString;
  }

  Entry& GetEntry(const void* aPc) {
    MOZ_ASSERT(DescribeCodeAddressLock::IsLocked());

    uint32_t index = HashGeneric(aPc) & kMask;
    MOZ_ASSERT(index < kNumEntries);
    Entry& entry = mEntries[index];

    if (!entry.mInUse || entry.mPc != aPc) {
      mNumCacheMisses++;

      // MozDescribeCodeAddress can (on Linux) acquire a lock inside
      // the shared library loader.  Another thread might call malloc
      // while holding that lock (when loading a shared library).  So
      // we have to exit the lock around this call.  For details, see
      // https://bugzilla.mozilla.org/show_bug.cgi?id=363334#c3
      MozCodeAddressDetails details;
      {
        DescribeCodeAddressLock::Unlock();
        (void)MozDescribeCodeAddress(const_cast<void*>(aPc), &details);
        DescribeCodeAddressLock::Lock();
      }

      const char* library = InternLibraryString(details.library);
      entry.Replace(aPc, details.function, library, details.loffset,
                    details.filename, details.lineno);

    } else {
      mNumCacheHits++;
    }

    MOZ_ASSERT(entry.mPc == aPc);

    return entry;
  }

  // A direct-mapped cache.  When doing dmd::Analyze() just after starting
  // desktop Firefox (which is similar to analyzing after a longer-running
  // session, thanks to the limit on how many records we print), a cache with
  // 2^24 entries (which approximates an infinite-entry cache) has a ~91% hit
  // rate.  A cache with 2^12 entries has a ~83% hit rate, and takes up ~85 KiB
  // (on 32-bit platforms) or ~150 KiB (on 64-bit platforms).
  static const size_t kNumEntries = 1 << 12;
  static const size_t kMask = kNumEntries - 1;
  Entry mEntries[kNumEntries];

  size_t mNumCacheHits;
  size_t mNumCacheMisses;

 public:
  CodeAddressService()
      : mLibraryStrings(64), mEntries(), mNumCacheHits(0), mNumCacheMisses(0) {}

  ~CodeAddressService() {
    for (auto iter = mLibraryStrings.iter(); !iter.done(); iter.next()) {
      AllocPolicy::free_(const_cast<char*>(iter.get()));
    }
  }

  void GetLocation(uint32_t aFrameNumber, const void* aPc, char* aBuf,
                   size_t aBufLen) {
    Entry& entry = GetEntry(aPc);
    MozFormatCodeAddress(aBuf, aBufLen, aFrameNumber, entry.mPc,
                         entry.mFunction, entry.mLibrary, entry.mLOffset,
                         entry.mFileName, entry.mLineNo);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t n = aMallocSizeOf(this);
    for (uint32_t i = 0; i < kNumEntries; i++) {
      n += mEntries[i].SizeOfExcludingThis(aMallocSizeOf);
    }

    n += mLibraryStrings.shallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mLibraryStrings.iter(); !iter.done(); iter.next()) {
      n += aMallocSizeOf(iter.get());
    }

    return n;
  }

  size_t CacheCapacity() const { return kNumEntries; }

  size_t CacheCount() const {
    size_t n = 0;
    for (size_t i = 0; i < kNumEntries; i++) {
      if (mEntries[i].mInUse) {
        n++;
      }
    }
    return n;
  }

  size_t NumCacheHits() const { return mNumCacheHits; }
  size_t NumCacheMisses() const { return mNumCacheMisses; }
};

}  // namespace mozilla

#endif  // CodeAddressService_h__
