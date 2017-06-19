/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulMain_h
#define LulMain_h

#include "PlatformMacros.h"
#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"

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
  // RUNS IN NO-MALLOC CONTEXT
  // Construct a valid one.
  explicit TaggedUWord(uintptr_t w)
    : mValue(w)
    , mValid(true)
  {}

  // RUNS IN NO-MALLOC CONTEXT
  // Construct an invalid one.
  TaggedUWord()
    : mValue(0)
    , mValid(false)
  {}

  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord operator+(TaggedUWord rhs) const {
    return (Valid() && rhs.Valid()) ? TaggedUWord(Value() + rhs.Value())
                                    : TaggedUWord();
  }

  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord operator-(TaggedUWord rhs) const {
    return (Valid() && rhs.Valid()) ? TaggedUWord(Value() - rhs.Value())
                                    : TaggedUWord();
  }

  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord operator&(TaggedUWord rhs) const {
    return (Valid() && rhs.Valid()) ? TaggedUWord(Value() & rhs.Value())
                                    : TaggedUWord();
  }

  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord operator|(TaggedUWord rhs) const {
    return (Valid() && rhs.Valid()) ? TaggedUWord(Value() | rhs.Value())
                                    : TaggedUWord();
  }

  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord CmpGEs(TaggedUWord rhs) const {
    if (Valid() && rhs.Valid()) {
      intptr_t s1 = (intptr_t)Value();
      intptr_t s2 = (intptr_t)rhs.Value();
      return TaggedUWord(s1 >= s2 ? 1 : 0);
    }
    return TaggedUWord();
  }

  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord operator<<(TaggedUWord rhs) const {
    if (Valid() && rhs.Valid()) {
      uintptr_t shift = rhs.Value();
      if (shift < 8 * sizeof(uintptr_t))
        return TaggedUWord(Value() << shift);
    }
    return TaggedUWord();
  }

  // RUNS IN NO-MALLOC CONTEXT
  // Is equal?  Note: non-validity on either side gives non-equality.
  bool operator==(TaggedUWord other) const {
    return (mValid && other.Valid()) ? (mValue == other.Value()) : false;
  }

  // RUNS IN NO-MALLOC CONTEXT
  // Is it word-aligned?
  bool IsAligned() const {
    return mValid && (mValue & (sizeof(uintptr_t)-1)) == 0;
  }

  // RUNS IN NO-MALLOC CONTEXT
  uintptr_t Value() const { return mValue; }

  // RUNS IN NO-MALLOC CONTEXT
  bool      Valid() const { return mValid; }

private:
  uintptr_t mValue;
  bool mValid;
};


// The registers, with validity tags, that will be unwound.

struct UnwindRegs {
#if defined(GP_ARCH_arm)
  TaggedUWord r7;
  TaggedUWord r11;
  TaggedUWord r12;
  TaggedUWord r13;
  TaggedUWord r14;
  TaggedUWord r15;
#elif defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
  TaggedUWord xbp;
  TaggedUWord xsp;
  TaggedUWord xip;
#else
# error "Unknown plat"
#endif
};


// The maximum number of bytes in a stack snapshot.  This value can be increased
// if necessary, but testing showed that 160k is enough to obtain good
// backtraces on x86_64 Linux.  Most backtraces fit comfortably into 4-8k of
// stack space, but we do have some very deep stacks occasionally.  Please see
// the comments in DoNativeBacktrace as to why it's OK to have this value be so
// large.
static const size_t N_STACK_BYTES = 160*1024;

// The stack chunk image that will be unwound.
struct StackImage {
  // [start_avma, +len) specify the address range in the buffer.
  // Obviously we require 0 <= len <= N_STACK_BYTES.
  uintptr_t mStartAvma;
  size_t    mLen;
  uint8_t   mContents[N_STACK_BYTES];
};


// Statistics collection for the unwinder.
template<typename T>
class LULStats {
public:
  LULStats()
    : mContext(0)
    , mCFI(0)
    , mFP(0)
  {}

  template <typename S>
  explicit LULStats(const LULStats<S>& aOther)
    : mContext(aOther.mContext)
    , mCFI(aOther.mCFI)
    , mFP(aOther.mFP)
  {}

  template <typename S>
  LULStats<T>& operator=(const LULStats<S>& aOther)
  {
    mContext = aOther.mContext;
    mCFI     = aOther.mCFI;
    mFP      = aOther.mFP;
    return *this;
  }

  template <typename S>
  uint32_t operator-(const LULStats<S>& aOther) {
    return (mContext - aOther.mContext) +
           (mCFI - aOther.mCFI) + (mFP - aOther.mFP);
  }

  T mContext; // Number of context frames
  T mCFI;     // Number of CFI/EXIDX frames
  T mFP;      // Number of frame-pointer recovered frames
};


// The core unwinder library class.  Just one of these is needed, and
// it can be shared by multiple unwinder threads.
//
// The library operates in one of two modes.
//
// * Admin mode.  The library is this state after creation.  In Admin
//   mode, no unwinding may be performed.  It is however allowable to
//   perform administrative tasks -- primarily, loading of unwind info
//   -- in this mode.  In particular, it is safe for the library to
//   perform dynamic memory allocation in this mode.  Safe in the
//   sense that there is no risk of deadlock against unwinding threads
//   that might -- because of where they have been sampled -- hold the
//   system's malloc lock.
//
// * Unwind mode.  In this mode, calls to ::Unwind may be made, but
//   nothing else.  ::Unwind guarantees not to make any dynamic memory
//   requests, so as to guarantee that the calling thread won't
//   deadlock in the case where it already holds the system's malloc lock.
//
// The library is created in Admin mode.  After debuginfo is loaded,
// the caller must switch it into Unwind mode by calling
// ::EnableUnwinding.  There is no way to switch it back to Admin mode
// after that.  To safely switch back to Admin mode would require the
// caller (or other external agent) to guarantee that there are no
// pending ::Unwind calls.

class PriMap;
class SegArray;
class UniqueStringUniverse;

class LUL {
public:
  // Create; supply a logging sink.  Sets the object in Admin mode.
  explicit LUL(void (*aLog)(const char*));

  // Destroy.  Caller is responsible for ensuring that no other
  // threads are in Unwind calls.  All resources are freed and all
  // registered unwinder threads are deregistered.  Can be called
  // either in Admin or Unwind mode.
  ~LUL();

  // Notify the library that unwinding is now allowed and so
  // admin-mode calls are no longer allowed.  The object is initially
  // created in admin mode.  The only possible transition is
  // admin->unwinding, therefore.
  void EnableUnwinding();

  // Notify of a new r-x mapping, and load the associated unwind info.
  // The filename is strdup'd and used for debug printing.  If
  // aMappedImage is NULL, this function will mmap/munmap the file
  // itself, so as to be able to read the unwind info.  If
  // aMappedImage is non-NULL then it is assumed to point to a
  // called-supplied and caller-managed mapped image of the file.
  // May only be called in Admin mode.
  void NotifyAfterMap(uintptr_t aRXavma, size_t aSize,
                      const char* aFileName, const void* aMappedImage);

  // In rare cases we know an executable area exists but don't know
  // what the associated file is.  This call notifies LUL of such
  // areas.  This is important for correct functioning of stack
  // scanning and of the x86-{linux,android} special-case
  // __kernel_syscall function handling.
  // This must be called only after the code area in
  // question really has been mapped.
  // May only be called in Admin mode.
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
  // May only be called in Admin mode.
  void NotifyBeforeUnmap(uintptr_t aAvmaMin, uintptr_t aAvmaMax);

  // Apply NotifyBeforeUnmap to the entire address space.  This causes
  // LUL to discard all unwind and executable-area information for the
  // entire address space.
  // May only be called in Admin mode.
  void NotifyBeforeUnmapAll() {
    NotifyBeforeUnmap(0, UINTPTR_MAX);
  }

  // Returns the number of mappings currently registered.
  // May only be called in Admin mode.
  size_t CountMappings();

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
  // This function assumes that the SP values increase as it unwinds
  // away from the innermost frame -- that is, that the stack grows
  // down.  It monitors SP values as it unwinds to check they
  // decrease, so as to avoid looping on corrupted stacks.
  //
  // May only be called in Unwind mode.  Multiple threads may unwind
  // at once.  LUL user is responsible for ensuring that no thread makes
  // any Admin calls whilst in Unwind mode.
  // MOZ_CRASHes if the calling thread is not registered for unwinding.
  //
  // The calling thread must previously have been registered via a call to
  // RegisterSampledThread.
  void Unwind(/*OUT*/uintptr_t* aFramePCs,
              /*OUT*/uintptr_t* aFrameSPs,
              /*OUT*/size_t* aFramesUsed,
              /*OUT*/size_t* aFramePointerFramesAcquired,
              size_t aFramesAvail,
              UnwindRegs* aStartRegs, StackImage* aStackImg);

  // The logging sink.  Call to send debug strings to the caller-
  // specified destination.  Can only be called by the Admin thread.
  void (*mLog)(const char*);

  // Statistics relating to unwinding.  These have to be atomic since
  // unwinding can occur on different threads simultaneously.
  LULStats<mozilla::Atomic<uint32_t>> mStats;

  // Possibly show the statistics.  This may not be called from any
  // registered sampling thread, since it involves I/O.
  void MaybeShowStats();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const;

private:
  // The statistics counters at the point where they were last printed.
  LULStats<uint32_t> mStatsPrevious;

  // Are we in admin mode?  Initially |true| but changes to |false|
  // once unwinding begins.
  bool mAdminMode;

  // The thread ID associated with admin mode.  This is the only thread
  // that is allowed do perform non-Unwind calls on this object.  Conversely,
  // no registered Unwinding thread may be the admin thread.  This is so
  // as to clearly partition the one thread that may do dynamic memory
  // allocation from the threads that are being sampled, since the latter
  // absolutely may not do dynamic memory allocation.
  int mAdminThreadId;

  // The top level mapping from code address ranges to postprocessed
  // unwind info.  Basically a sorted array of (addr, len, info)
  // records.  This field is updated by NotifyAfterMap and NotifyBeforeUnmap.
  PriMap* mPriMap;

  // An auxiliary structure that records which address ranges are
  // mapped r-x, for the benefit of the stack scanner.
  SegArray* mSegArray;

  // A UniqueStringUniverse that holds all the strdup'd strings created
  // whilst reading unwind information.  This is included so as to make
  // it possible to free them in ~LUL.
  UniqueStringUniverse* mUSU;
};


// Run unit tests on an initialised, loaded-up LUL instance, and print
// summary results on |aLUL|'s logging sink.  Also return the number
// of tests run in *aNTests and the number that passed in
// *aNTestsPassed.
void
RunLulUnitTests(/*OUT*/int* aNTests, /*OUT*/int*aNTestsPassed, LUL* aLUL);

} // namespace lul

#endif // LulMain_h
