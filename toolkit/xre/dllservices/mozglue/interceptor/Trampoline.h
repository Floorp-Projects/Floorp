/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_Trampoline_h
#define mozilla_interceptor_Trampoline_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"
#include "mozilla/Types.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/WindowsUnwindInfo.h"

namespace mozilla {
namespace interceptor {

template <typename MMPolicy>
class MOZ_STACK_CLASS Trampoline final {
 public:
  Trampoline(const MMPolicy* aMMPolicy, uint8_t* const aLocalBase,
             const uintptr_t aRemoteBase, const uint32_t aChunkSize)
      : mMMPolicy(aMMPolicy),
        mPrevLocalProt(0),
        mLocalBase(aLocalBase),
        mRemoteBase(aRemoteBase),
        mOffset(0),
        mExeOffset(0),
#ifdef _M_X64
        mCopyCodesEndOffset(0),
        mExeEndOffset(0),
#endif  // _M_X64
        mMaxOffset(aChunkSize),
        mAccumulatedStatus(true) {
    if (!::VirtualProtect(aLocalBase, aChunkSize,
                          MMPolicy::GetTrampWriteProtFlags(),
                          &mPrevLocalProt)) {
      mPrevLocalProt = 0;
    }
  }

  Trampoline(Trampoline&& aOther)
      : mMMPolicy(aOther.mMMPolicy),
        mPrevLocalProt(aOther.mPrevLocalProt),
        mLocalBase(aOther.mLocalBase),
        mRemoteBase(aOther.mRemoteBase),
        mOffset(aOther.mOffset),
        mExeOffset(aOther.mExeOffset),
#ifdef _M_X64
        mCopyCodesEndOffset(aOther.mCopyCodesEndOffset),
        mExeEndOffset(aOther.mExeEndOffset),
#endif  // _M_X64
        mMaxOffset(aOther.mMaxOffset),
        mAccumulatedStatus(aOther.mAccumulatedStatus) {
    aOther.mPrevLocalProt = 0;
    aOther.mAccumulatedStatus = false;
  }

  MOZ_IMPLICIT Trampoline(decltype(nullptr))
      : mMMPolicy(nullptr),
        mPrevLocalProt(0),
        mLocalBase(nullptr),
        mRemoteBase(0),
        mOffset(0),
        mExeOffset(0),
#ifdef _M_X64
        mCopyCodesEndOffset(0),
        mExeEndOffset(0),
#endif  // _M_X64
        mMaxOffset(0),
        mAccumulatedStatus(false) {
  }

  Trampoline(const Trampoline&) = delete;
  Trampoline& operator=(const Trampoline&) = delete;

  Trampoline& operator=(Trampoline&& aOther) {
    Clear();

    mMMPolicy = aOther.mMMPolicy;
    mPrevLocalProt = aOther.mPrevLocalProt;
    mLocalBase = aOther.mLocalBase;
    mRemoteBase = aOther.mRemoteBase;
    mOffset = aOther.mOffset;
    mExeOffset = aOther.mExeOffset;
#ifdef _M_X64
    mCopyCodesEndOffset = aOther.mCopyCodesEndOffset;
    mExeEndOffset = aOther.mExeEndOffset;
#endif  // _M_X64
    mMaxOffset = aOther.mMaxOffset;
    mAccumulatedStatus = aOther.mAccumulatedStatus;

    aOther.mPrevLocalProt = 0;
    aOther.mAccumulatedStatus = false;

    return *this;
  }

  ~Trampoline() { Clear(); }

  explicit operator bool() const {
    return IsNull() ||
           (mLocalBase && mRemoteBase && mPrevLocalProt && mAccumulatedStatus);
  }

  bool IsNull() const { return !mMMPolicy; }

#if defined(_M_ARM64)

  void WriteInstruction(uint32_t aInstruction) {
    const uint32_t kDelta = sizeof(uint32_t);

    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += kDelta;
      return;
    }

    if (mOffset + kDelta > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    *reinterpret_cast<uint32_t*>(mLocalBase + mOffset) = aInstruction;
    mOffset += kDelta;
  }

  void WriteLoadLiteral(const uintptr_t aAddress, const uint8_t aReg) {
    const uint32_t kDelta = sizeof(uint32_t) + sizeof(uintptr_t);

    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += kDelta;
      return;
    }

    // We grow the literal pool from the *end* of the tramp,
    // so we need to ensure that there is enough room for both an instruction
    // and a pointer
    if (mOffset + kDelta > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    mMaxOffset -= sizeof(uintptr_t);
    *reinterpret_cast<uintptr_t*>(mLocalBase + mMaxOffset) = aAddress;

    CheckedInt<intptr_t> pc(GetCurrentRemoteAddress());
    if (!pc.isValid()) {
      mAccumulatedStatus = false;
      return;
    }

    CheckedInt<intptr_t> literal(reinterpret_cast<uintptr_t>(mLocalBase) +
                                 mMaxOffset);
    if (!literal.isValid()) {
      mAccumulatedStatus = false;
      return;
    }

    CheckedInt<intptr_t> ptrOffset = (literal - pc);
    if (!ptrOffset.isValid()) {
      mAccumulatedStatus = false;
      return;
    }

    // ptrOffset must be properly aligned
    MOZ_ASSERT((ptrOffset.value() % 4) == 0);
    ptrOffset /= 4;

    CheckedInt<int32_t> offset(ptrOffset.value());
    if (!offset.isValid()) {
      mAccumulatedStatus = false;
      return;
    }

    // Ensure that offset falls within the range of a signed 19-bit value
    if (offset.value() < -0x40000 || offset.value() > 0x3FFFF) {
      mAccumulatedStatus = false;
      return;
    }

    const int32_t kimm19Mask = 0x7FFFF;
    int32_t masked = offset.value() & kimm19Mask;

    MOZ_ASSERT(aReg < 32);
    uint32_t loadInstr = 0x58000000 | (masked << 5) | aReg;
    WriteInstruction(loadInstr);
  }

#else

  void WriteByte(uint8_t aValue) {
    const uint32_t kDelta = sizeof(uint8_t);

    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += kDelta;
      return;
    }

    if (mOffset >= mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    *(mLocalBase + mOffset) = aValue;
    ++mOffset;
  }

  void WriteInteger(int32_t aValue) {
    const uint32_t kDelta = sizeof(int32_t);

    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += kDelta;
      return;
    }

    if (mOffset + kDelta > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    *reinterpret_cast<int32_t*>(mLocalBase + mOffset) = aValue;
    mOffset += kDelta;
  }

  void WriteDisp32(uintptr_t aAbsTarget) {
    const uint32_t kDelta = sizeof(int32_t);

    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += kDelta;
      return;
    }

    if (mOffset + kDelta > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    // This needs to be computed from the remote location
    intptr_t remoteTrampPosition = static_cast<intptr_t>(mRemoteBase + mOffset);

    intptr_t diff =
        static_cast<intptr_t>(aAbsTarget) - (remoteTrampPosition + kDelta);

    CheckedInt<int32_t> checkedDisp(diff);
    MOZ_ASSERT(checkedDisp.isValid());
    if (!checkedDisp.isValid()) {
      mAccumulatedStatus = false;
      return;
    }

    int32_t disp = checkedDisp.value();
    *reinterpret_cast<int32_t*>(mLocalBase + mOffset) = disp;
    mOffset += kDelta;
  }

  void WriteBytes(void* aAddr, size_t aSize) {
    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += aSize;
      return;
    }

    if (mOffset + aSize > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    std::memcpy(reinterpret_cast<void*>(mLocalBase + mOffset), aAddr, aSize);
    mOffset += aSize;
  }

#endif

  void WritePointer(uintptr_t aValue) {
    const uint32_t kDelta = sizeof(uintptr_t);

    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += kDelta;
      return;
    }

    if (mOffset + kDelta > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    *reinterpret_cast<uintptr_t*>(mLocalBase + mOffset) = aValue;
    mOffset += kDelta;
  }

  void WriteEncodedPointer(void* aValue) {
    uintptr_t encoded = ReadOnlyTargetFunction<MMPolicy>::EncodePtr(aValue);
    WritePointer(encoded);
  }

  Maybe<uintptr_t> ReadPointer() {
    if (mOffset + sizeof(uintptr_t) > mMaxOffset) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    auto result = Some(*reinterpret_cast<uintptr_t*>(mLocalBase + mOffset));
    mOffset += sizeof(uintptr_t);
    return std::move(result);
  }

  Maybe<uintptr_t> ReadEncodedPointer() {
    Maybe<uintptr_t> encoded(ReadPointer());
    if (!encoded) {
      return encoded;
    }

    return Some(ReadOnlyTargetFunction<MMPolicy>::DecodePtr(encoded.value()));
  }

#if defined(_M_IX86)
  // 32-bit only
  void AdjustDisp32AtOffset(uint32_t aOffset, uintptr_t aAbsTarget) {
    uint32_t effectiveOffset = mExeOffset + aOffset;

    if (effectiveOffset + sizeof(int32_t) > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    intptr_t diff = static_cast<intptr_t>(aAbsTarget) -
                    static_cast<intptr_t>(mRemoteBase + mExeOffset);
    *reinterpret_cast<int32_t*>(mLocalBase + effectiveOffset) += diff;
  }
#endif  // defined(_M_IX86)

  void CopyFrom(uintptr_t aOrigBytes, uint32_t aNumBytes) {
    if (!mMMPolicy) {
      // Null tramp, just track offset
      mOffset += aNumBytes;
      return;
    }

    if (!mMMPolicy || mOffset + aNumBytes > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }

    if (!mMMPolicy->Read(mLocalBase + mOffset,
                         reinterpret_cast<void*>(aOrigBytes), aNumBytes)) {
      mAccumulatedStatus = false;
      return;
    }

    mOffset += aNumBytes;
  }

  void CopyCodes(uintptr_t aOrigBytes, uint32_t aNumBytes) {
#ifdef _M_X64
    if (mOffset == mCopyCodesEndOffset) {
      mCopyCodesEndOffset += aNumBytes;
    }
#endif  // _M_X64
    CopyFrom(aOrigBytes, aNumBytes);
  }

  void Rewind() { mOffset = 0; }

  uintptr_t GetCurrentRemoteAddress() const { return mRemoteBase + mOffset; }

  void StartExecutableCode() {
    MOZ_ASSERT(!mExeOffset);
    mExeOffset = mOffset;
#ifdef _M_X64
    mCopyCodesEndOffset = mOffset;
#endif  // _M_X64
  }

  void* EndExecutableCode() {
    if (!mAccumulatedStatus || !mMMPolicy) {
      return nullptr;
    }

#ifdef _M_X64
    mExeEndOffset = mOffset;
#endif  // _M_X64

    // This must always return the start address the executable code
    // *in the target process*
    return reinterpret_cast<void*>(mRemoteBase + mExeOffset);
  }

  uint32_t GetCurrentExecutableCodeLen() const { return mOffset - mExeOffset; }

#ifdef _M_X64

  void Align(uint32_t aAlignment) {
    // aAlignment should be a power of 2
    MOZ_ASSERT(!(aAlignment & (aAlignment - 1)));

    uint32_t alignedOffset = (mOffset + aAlignment - 1) & ~(aAlignment - 1);
    if (alignedOffset > mMaxOffset) {
      mAccumulatedStatus = false;
      return;
    }
    mOffset = alignedOffset;
  }

  // We assume that all instructions that are part of the prologue are left
  // intact by detouring code, i.e. that they are copied using CopyCodes. This
  // is not true for calls and jumps for example, but calls and jumps cannot be
  // part of the prologue. This assumption allows us to copy unwind information
  // as-is, because unwind information only refers to instructions within the
  // prologue.
  bool AddUnwindInfo(uintptr_t aOrigFuncAddr, uintptr_t aOrigFuncStopOffset) {
    if constexpr (!MMPolicy::kSupportsUnwindInfo) {
      return false;
    }

    if (!mMMPolicy) {
      return false;
    }

    uint32_t origFuncOffsetFromBeginAddr = 0;
    uint32_t origFuncOffsetToEndAddr = 0;
    uintptr_t origImageBase = 0;
    auto unwindInfoData =
        mMMPolicy->LookupUnwindInfo(aOrigFuncAddr, &origFuncOffsetFromBeginAddr,
                                    &origFuncOffsetToEndAddr, &origImageBase);
    if (!unwindInfoData) {
      // If the original function does not have unwind info, there is nothing
      // more to do.
      return true;
    }

    // We do not support hooking at a location that isn't the beginning of a
    // function.
    MOZ_ASSERT(origFuncOffsetFromBeginAddr == 0);
    if (origFuncOffsetFromBeginAddr != 0) {
      return false;
    }

    IterableUnwindInfo unwindInfoIt(unwindInfoData.get());
    auto& unwindInfo = unwindInfoIt.Info();

    // The prologue should contain only instructions that we detour using
    // CopyCodes. If not, there is most likely a mismatch between the unwind
    // information and the actual code we are detouring, so we stop here. This
    // is a best-effort safeguard intended to detect situations where e.g.
    // third-party injected code would have altered the function we are
    // detouring.
    if (mCopyCodesEndOffset < aOrigFuncStopOffset &&
        unwindInfo.size_of_prolog > mCopyCodesEndOffset) {
      return false;
    }

    // According to the documentation, the array is sorted by descending order
    // of offset in the prologue. Let's double check this assumption if in
    // debug. This also checks that the full unwind information isn't
    // ill-formed, thanks to all the MOZ_ASSERT in iteration code.
#  ifdef DEBUG
    uint8_t previousOffset = 0xFF;
    for (const auto& unwindCode : unwindInfoIt) {
      MOZ_ASSERT(unwindCode.offset_in_prolog <= previousOffset);
      previousOffset = unwindCode.offset_in_prolog;
    }
#  endif  // DEBUG

    // We skip entries that are not part of the code we have detoured.
    // This code relies on the array being sorted by descending order of offset
    // in the prolog.
    uint8_t firstRelevantCode = 0;
    uint8_t countOfCodes = 0;
    auto it = unwindInfoIt.begin();
    for (; it != unwindInfoIt.end(); ++it) {
      const auto& unwindCode = *it;
      if (unwindCode.offset_in_prolog <= aOrigFuncStopOffset) {
        // Found a relevant entry
        firstRelevantCode = it.Index();
        countOfCodes = unwindInfo.count_of_codes - firstRelevantCode;
        break;
      }
    }

    // Check that we encountered no ill-formed unwind codes.
    if (!it.IsValid() && !it.IsAtEnd()) {
      return false;
    }

    // We do not support chained unwind info. We should add support for chained
    // unwind info if we ever reach this assert. Since we hook functions at
    // their start address, this should not happen.
    if (unwindInfo.flags & UNW_FLAG_CHAININFO) {
      MOZ_ASSERT(
          false,
          "Tried to detour at a location with chained unwind information");
      return false;
    }

    // We do not support exception handler info either. This could be a problem
    // if we detour code that does not belong to the prologue and contains a
    // call instruction, as this handler would then not be found if unwinding
    // from callees. The following assert checks that this does not happen.
    //
    // Our current assumption is that all the functions we hook either have no
    // associated exception handlers, or it is __GSHandlerCheck. This handler
    // is the most commonly found, for example it is present in LdrLoadDll,
    // SendMessageTimeoutW, GetWindowInfo. It is added to functions that use
    // stack buffers, in order to mitigate stack buffer overflows. We explain
    // below why it is not a problem that we do not preserve __GSHandlerCheck
    // information when we detour code.
    //
    // Preserving exception handler information would raise two challenges:
    //
    // (1) if the exception handler was not written in a generic way, it may
    //     behave differently when called for our detoured code compared to
    //     what it would do if called from the original location of the code;
    // (2) the exception handler can be followed by handler-specific data,
    //     which we cannot copy because we do not know its size.
    //
    // __GSHandlerCheck checks that the stack cookie value wasn't overwritten
    // before continuing to unwind and call further handlers. That is a
    // security feature that we want to preserve. However, since these
    // functions allocate stack space and write the stack cookie as part of
    // their prologue, the 13 bytes that we detour are necessarily part of
    // their prologue, which must contain at least the following instructions:
    //
    //   48 81 ec XX XX XX XX     sub rsp, 0xXXXXXXXX
    //   48 8b 05 XX XX XX XX     mov rax, qword ptr [rip+__security_cookie]
    //   48 33 c4                 xor rax, rsp
    //   48 89 84 24 XX XX XX XX  mov qword ptr [RSP + 0xXXXXXXXX],RAX
    //
    // As a consequence, code associated with __GSHandlerCheck will necessarily
    // satisfy (aOrigFuncStopOffset <= unwindInfo.size_of_prolog), and it is OK
    // to not preserve handler info in that case.
#  ifdef DEBUG
    if (aOrigFuncStopOffset > unwindInfo.size_of_prolog) {
      MOZ_ASSERT(!(unwindInfo.flags & (UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER)));
    }
#  endif  // DEBUG

    // The unwind info must be DWORD-aligned
    Align(sizeof(uint32_t));
    if (!mAccumulatedStatus) {
      return false;
    }
    uintptr_t unwindInfoOffset = mOffset;

    unwindInfo.flags &=
        ~(UNW_FLAG_CHAININFO | UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER);
    unwindInfo.count_of_codes = countOfCodes;
    if (aOrigFuncStopOffset < unwindInfo.size_of_prolog) {
      unwindInfo.size_of_prolog = aOrigFuncStopOffset;
    }

    WriteBytes(reinterpret_cast<void*>(&unwindInfo),
               offsetof(UnwindInfo, unwind_code));
    if (!mAccumulatedStatus) {
      return false;
    }

    WriteBytes(
        reinterpret_cast<void*>(&unwindInfo.unwind_code[firstRelevantCode]),
        countOfCodes * sizeof(UnwindCode));
    if (!mAccumulatedStatus) {
      return false;
    }

    // The function table must be DWORD-aligned
    Align(sizeof(uint32_t));
    if (!mAccumulatedStatus) {
      return false;
    }
    uintptr_t functionTableOffset = mOffset;

    WriteInteger(mExeOffset);
    if (!mAccumulatedStatus) {
      return false;
    }

    WriteInteger(mExeEndOffset);
    if (!mAccumulatedStatus) {
      return false;
    }

    WriteInteger(unwindInfoOffset);
    if (!mAccumulatedStatus) {
      return false;
    }

    return mMMPolicy->AddFunctionTable(mRemoteBase + functionTableOffset, 1,
                                       mRemoteBase);
  }

#endif  // _M_X64

  Trampoline<MMPolicy>& operator--() {
    MOZ_ASSERT(mOffset);
    --mOffset;
    return *this;
  }

 private:
  void Clear() {
    if (!mLocalBase || !mPrevLocalProt) {
      return;
    }

    DebugOnly<bool> ok = !!::VirtualProtect(mLocalBase, mMaxOffset,
                                            mPrevLocalProt, &mPrevLocalProt);
    MOZ_ASSERT(ok);

    mLocalBase = nullptr;
    mRemoteBase = 0;
    mPrevLocalProt = 0;
    mAccumulatedStatus = false;
  }

 private:
  const MMPolicy* mMMPolicy;
  DWORD mPrevLocalProt;
  uint8_t* mLocalBase;
  uintptr_t mRemoteBase;
  uint32_t mOffset;
  uint32_t mExeOffset;
#ifdef _M_X64
  uint32_t mCopyCodesEndOffset;
  uint32_t mExeEndOffset;
#endif  // _M_X64
  uint32_t mMaxOffset;
  bool mAccumulatedStatus;
};

template <typename MMPolicy>
class MOZ_STACK_CLASS TrampolineCollection final {
 public:
  class MOZ_STACK_CLASS TrampolineIterator final {
   public:
    Trampoline<MMPolicy> operator*() {
      uint32_t offset = mCurTramp * mCollection.mTrampSize;
      return Trampoline<MMPolicy>(
          &mCollection.mMMPolicy, mCollection.mLocalBase + offset,
          mCollection.mRemoteBase + offset, mCollection.mTrampSize);
    }

    TrampolineIterator& operator++() {
      ++mCurTramp;
      return *this;
    }

    bool operator!=(const TrampolineIterator& aOther) const {
      return mCurTramp != aOther.mCurTramp;
    }

   private:
    explicit TrampolineIterator(
        const TrampolineCollection<MMPolicy>& aCollection,
        const uint32_t aCurTramp = 0)
        : mCollection(aCollection), mCurTramp(aCurTramp) {}

    const TrampolineCollection<MMPolicy>& mCollection;
    uint32_t mCurTramp;

    friend class TrampolineCollection<MMPolicy>;
  };

  explicit TrampolineCollection(const MMPolicy& aMMPolicy)
      : mMMPolicy(aMMPolicy),
        mLocalBase(0),
        mRemoteBase(0),
        mTrampSize(0),
        mNumTramps(0),
        mPrevProt(0),
        mCS(nullptr) {}

  TrampolineCollection(const MMPolicy& aMMPolicy, uint8_t* const aLocalBase,
                       const uintptr_t aRemoteBase, const uint32_t aTrampSize,
                       const uint32_t aNumTramps)
      : mMMPolicy(aMMPolicy),
        mLocalBase(aLocalBase),
        mRemoteBase(aRemoteBase),
        mTrampSize(aTrampSize),
        mNumTramps(aNumTramps),
        mPrevProt(0),
        mCS(nullptr) {
    if (!aNumTramps) {
      return;
    }

    BOOL ok = mMMPolicy.Protect(aLocalBase, aNumTramps * aTrampSize,
                                PAGE_EXECUTE_READWRITE, &mPrevProt);
    if (!ok) {
      // When destroying a sandboxed process that uses
      // MITIGATION_DYNAMIC_CODE_DISABLE, we won't be allowed to write to our
      // executable memory so we just do nothing.  If we fail to get access
      // to memory for any other reason, we still don't want to crash but we
      // do assert.
      MOZ_ASSERT(IsDynamicCodeDisabled());
      mNumTramps = 0;
      mPrevProt = 0;
    }
  }

  ~TrampolineCollection() {
    if (!mPrevProt) {
      return;
    }

    mMMPolicy.Protect(mLocalBase, mNumTramps * mTrampSize, mPrevProt,
                      &mPrevProt);

    if (mCS) {
      ::LeaveCriticalSection(mCS);
    }
  }

  void Lock(CRITICAL_SECTION& aCS) {
    if (!mPrevProt || mCS) {
      return;
    }

    mCS = &aCS;
    ::EnterCriticalSection(&aCS);
  }

  TrampolineIterator begin() const {
    if (!mPrevProt) {
      return end();
    }

    return TrampolineIterator(*this);
  }

  TrampolineIterator end() const {
    return TrampolineIterator(*this, mNumTramps);
  }

  TrampolineCollection(const TrampolineCollection&) = delete;
  TrampolineCollection& operator=(const TrampolineCollection&) = delete;
  TrampolineCollection& operator=(TrampolineCollection&&) = delete;

  TrampolineCollection(TrampolineCollection&& aOther)
      : mMMPolicy(aOther.mMMPolicy),
        mLocalBase(aOther.mLocalBase),
        mRemoteBase(aOther.mRemoteBase),
        mTrampSize(aOther.mTrampSize),
        mNumTramps(aOther.mNumTramps),
        mPrevProt(aOther.mPrevProt),
        mCS(aOther.mCS) {
    aOther.mPrevProt = 0;
    aOther.mCS = nullptr;
  }

 private:
  const MMPolicy& mMMPolicy;
  uint8_t* const mLocalBase;
  const uintptr_t mRemoteBase;
  const uint32_t mTrampSize;
  uint32_t mNumTramps;
  uint32_t mPrevProt;
  CRITICAL_SECTION* mCS;

  friend class TrampolineIterator;
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_Trampoline_h
