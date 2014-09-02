/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulMain_h
#define LulMain_h

#include <pthread.h>   // pthread_t

#include <map>

#include "LulPlatformMacros.h"
#include "LulRWLock.h"

// LUL: A Lightweight Unwind Library.
// This file provides the end-user (external) interface for LUL.

// Some comments about naming in the implementation.  These are safe
// to ignore if you are merely using LUL, but are important if you
// hack on its internals.
//
// Debuginfo readers in general have tended to use the word "address"
// to mean several different things.  This sometimes makes them
// difficult to understand and maintain.  LUL tries hard to avoid
// using the word "address" and instead uses the following more
// precise terms:
//
// * SVMA ("Stated Virtual Memory Address"): this is an address of a
//   symbol (etc) as it is stated in the symbol table, or other
//   metadata, of an object.  Such values are typically small and
//   start from zero or thereabouts, unless the object has been
//   prelinked.
//
// * AVMA ("Actual Virtual Memory Address"): this is the address of a
//   symbol (etc) in a running process, that is, once the associated
//   object has been mapped into a process.  Such values are typically
//   much larger than SVMAs, since objects can get mapped arbitrarily
//   far along the address space.
//
// * "Bias": the difference between AVMA and SVMA for a given symbol
//   (specifically, AVMA - SVMA).  The bias is always an integral
//   number of pages.  Once we know the bias for a given object's
//   text section (for example), we can compute the AVMAs of all of
//   its text symbols by adding the bias to their SVMAs.
//
// * "Image address": typically, to read debuginfo from an object we
//   will temporarily mmap in the file so as to read symbol tables
//   etc.  Addresses in this temporary mapping are called "Image
//   addresses".  Note that the temporary mapping is entirely
//   unrelated to the mappings of the file that the dynamic linker
//   must perform merely in order to get the program to run.  Hence
//   image addresses are unrelated to either SVMAs or AVMAs.


namespace lul {

// A machine word plus validity tag.
class TaggedUWord {
public:
  // Construct a valid one.
  explicit TaggedUWord(uintptr_t w)
    : mValue(w)
    , mValid(true)
  {}

  // Construct an invalid one.
  TaggedUWord()
    : mValue(0)
    , mValid(false)
  {}

  // Add in a second one.
  void Add(TaggedUWord other) {
    if (mValid && other.Valid()) {
      mValue += other.Value();
    } else {
      mValue = 0;
      mValid = false;
    }
  }

  // Is it word-aligned?
  bool IsAligned() const {
    return mValid && (mValue & (sizeof(uintptr_t)-1)) == 0;
  }

  uintptr_t Value() const { return mValue; }
  bool      Valid() const { return mValid; }

private:
  uintptr_t mValue;
  bool mValid;
};


// The registers, with validity tags, that will be unwound.

struct UnwindRegs {
#if defined(LUL_ARCH_arm)
  TaggedUWord r7;
  TaggedUWord r11;
  TaggedUWord r12;
  TaggedUWord r13;
  TaggedUWord r14;
  TaggedUWord r15;
#elif defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
  TaggedUWord xbp;
  TaggedUWord xsp;
  TaggedUWord xip;
#else
# error "Unknown plat"
#endif
};


// The maximum number of bytes in a stack snapshot.  This can be
// increased if necessary, but larger values cost performance, since a
// stack snapshot needs to be copied between sampling and worker
// threads for each snapshot.  In practice 32k seems to be enough
// to get good backtraces.
static const size_t N_STACK_BYTES = 32768;

// The stack chunk image that will be unwound.
struct StackImage {
  // [start_avma, +len) specify the address range in the buffer.
  // Obviously we require 0 <= len <= N_STACK_BYTES.
  uintptr_t mStartAvma;
  size_t    mLen;
  uint8_t   mContents[N_STACK_BYTES];
};


// The core unwinder library class.  Just one of these is needed, and
// it can be shared by multiple unwinder threads.
//
// Access to the library is mediated by a single reader-writer lock.
// All attempts to change the library's internal shared state -- that
// is, loading or unloading unwind info -- are forced single-threaded
// by causing the called routine to acquire a write-lock.  Unwind
// requests do not change the library's internal shared state and
// therefore require only a read-lock.  Hence multiple threads can
// unwind in parallel.
//
// The library needs to maintain state which is private to each
// unwinder thread -- the CFI (Dwarf Call Frame Information) fast
// cache.  Hence unwinder threads first need to register with the
// library, so their identities are known.  Also, for maximum
// effectiveness of the CFI caching, it is preferable to have a small
// number of very-busy unwinder threads rather than a large number of
// mostly-idle unwinder threads.
//
// None of the methods may be safely called from within a signal
// handler, since this risks deadlock.  In particular this means
// a thread may not unwind itself from within a signal handler
// frame.  It might be safe to call Unwind() on its own stack
// from not-inside a signal frame, although even that cannot be
// guaranteed deadlock free.

class PriMap;
class SegArray;
class CFICache;

class LUL {
public:
  // Create; supply a logging sink.  Initialises the rw-lock.
  explicit LUL(void (*aLog)(const char*));

  // Destroy.  This acquires mRWlock for writing.  By doing that, waits
  // for all unwinder threads to finish any Unwind() calls they may be
  // in.  All resources are freed and all registered unwinder threads
  // are deregistered.
  ~LUL();

  // Notify of a new r-x mapping, and load the associated unwind info.
  // The filename is strdup'd and used for debug printing.  If
  // aMappedImage is NULL, this function will mmap/munmap the file
  // itself, so as to be able to read the unwind info.  If
  // aMappedImage is non-NULL then it is assumed to point to a
  // called-supplied and caller-managed mapped image of the file.
  //
  // Acquires mRWlock for writing.  This must be called only after the
  // code area in question really has been mapped.
  void NotifyAfterMap(uintptr_t aRXavma, size_t aSize,
                      const char* aFileName, const void* aMappedImage);

  // In rare cases we know an executable area exists but don't know
  // what the associated file is.  This call notifies LUL of such
  // areas.  This is important for correct functioning of stack
  // scanning and of the x86-{linux,android} special-case
  // __kernel_syscall function handling.  Acquires mRWlock for
  // writing.  This must be called only after the code area in
  // question really has been mapped.
  void NotifyExecutableArea(uintptr_t aRXavma, size_t aSize);

  // Notify that a mapped area has been unmapped; discard any
  // associated unwind info.  Acquires mRWlock for writing.  Note that
  // to avoid segfaulting the stack-scan unwinder, which inspects code
  // areas, this must be called before the code area in question is
  // really unmapped.  Note that, unlike NotifyAfterMap(), this
  // function takes the start and end addresses of the range to be
  // unmapped, rather than a start and a length parameter.  This is so
  // as to make it possible to notify an unmap for the entire address
  // space using a single call.
  void NotifyBeforeUnmap(uintptr_t aAvmaMin, uintptr_t aAvmaMax);

  // Apply NotifyBeforeUnmap to the entire address space.  This causes
  // LUL to discard all unwind and executable-area information for the
  // entire address space.
  void NotifyBeforeUnmapAll() {
    NotifyBeforeUnmap(0, UINTPTR_MAX);
  }

  // Returns the number of mappings currently registered.  Acquires
  // mRWlock for writing.
  size_t CountMappings();

  // Register the calling thread for unwinding.  Acquires mRWlock for
  // writing.
  void RegisterUnwinderThread();

  // Unwind |aStackImg| starting with the context in |aStartRegs|.
  // Write the number of frames recovered in *aFramesUsed.  Put
  // the PC values in aFramePCs[0 .. *aFramesUsed-1] and
  // the SP values in aFrameSPs[0 .. *aFramesUsed-1].
  // |aFramesAvail| is the size of the two output arrays and hence the
  // largest possible value of *aFramesUsed.  PC values are always
  // valid, and the unwind will stop when the PC becomes invalid, but
  // the SP values might be invalid, in which case the value zero will
  // be written in the relevant frameSPs[] slot.
  //
  // Unwinding may optionally use stack scanning.  The maximum number
  // of frames that may be recovered by stack scanning is
  // |aScannedFramesAllowed| and the actual number recovered is
  // written into *aScannedFramesAcquired.  |aScannedFramesAllowed|
  // must be less than or equal to |aFramesAvail|.
  //
  // This function assumes that the SP values increase as it unwinds
  // away from the innermost frame -- that is, that the stack grows
  // down.  It monitors SP values as it unwinds to check they
  // decrease, so as to avoid looping on corrupted stacks.
  //
  // Acquires mRWlock for reading.  Hence multiple threads may unwind
  // at once, but no thread may be unwinding whilst the library loads
  // or discards unwind information.  Returns false if the calling
  // thread is not registered for unwinding.
  //
  // Up to aScannedFramesAllowed stack-scanned frames may be recovered.
  //
  // The calling thread must previously have registered itself via
  // RegisterUnwinderThread.
  void Unwind(/*OUT*/uintptr_t* aFramePCs,
              /*OUT*/uintptr_t* aFrameSPs,
              /*OUT*/size_t* aFramesUsed,
              /*OUT*/size_t* aScannedFramesAcquired,
              size_t aFramesAvail,
              size_t aScannedFramesAllowed,
              UnwindRegs* aStartRegs, StackImage* aStackImg);

  // The logging sink.  Call to send debug strings to the caller-
  // specified destination.
  void (*mLog)(const char*);

private:
  // Invalidate the caches.  Requires mRWlock to be held for writing;
  // does not acquire it itself.
  void InvalidateCFICaches();

  // The one-and-only lock, a reader-writer lock, for the library.
  LulRWLock* mRWlock;

  // The top level mapping from code address ranges to postprocessed
  // unwind info.  Basically a sorted array of (addr, len, info)
  // records.  Threads wishing to query this field must hold mRWlock
  // for reading.  Threads wishing to modify this field must hold
  // mRWlock for writing.  This field is updated by NotifyAfterMap and
  // NotifyBeforeUnmap.
  PriMap* mPriMap;

  // An auxiliary structure that records which address ranges are
  // mapped r-x, for the benefit of the stack scanner.  Threads
  // wishing to query this field must hold mRWlock for reading.
  // Threads wishing to modify this field must hold mRWlock for
  // writing.
  SegArray* mSegArray;

  // The thread-local data: a mapping from threads to CFI-fast-caches.
  // Threads wishing to query this field must hold mRWlock for
  // reading.  Threads wishing to modify this field must hold mRWlock
  // for writing.
  //
  // The CFICaches themselves are thread-local and can be both read
  // and written when mRWlock is held for reading.  It would probably
  // be faster to use the pthread_{set,get}specific functions, but
  // also more difficult.  This map is queried once per unwind, in
  // order to get hold of the CFI cache for a given thread.
  std::map<pthread_t, CFICache*> mCaches;
};


// Run unit tests on an initialised, loaded-up LUL instance, and print
// summary results on |aLUL|'s logging sink.  Also return the number
// of tests run in *aNTests and the number that passed in
// *aNTestsPassed.
void
RunLulUnitTests(/*OUT*/int* aNTests, /*OUT*/int*aNTestsPassed, LUL* aLUL);

} // namespace lul

#endif // LulMain_h
