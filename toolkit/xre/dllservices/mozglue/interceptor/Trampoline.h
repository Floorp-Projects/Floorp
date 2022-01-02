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
        mMaxOffset(0),
        mAccumulatedStatus(false) {}

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

  void Rewind() { mOffset = 0; }

  uintptr_t GetCurrentRemoteAddress() const { return mRemoteBase + mOffset; }

  void StartExecutableCode() {
    MOZ_ASSERT(!mExeOffset);
    mExeOffset = mOffset;
  }

  void* EndExecutableCode() const {
    if (!mAccumulatedStatus || !mMMPolicy) {
      return nullptr;
    }

    // This must always return the start address the executable code
    // *in the target process*
    return reinterpret_cast<void*>(mRemoteBase + mExeOffset);
  }

  uint32_t GetCurrentExecutableCodeLen() const { return mOffset - mExeOffset; }

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
      return Trampoline<MMPolicy>(nullptr, mCollection.mLocalBase + offset,
                                  mCollection.mRemoteBase + offset,
                                  mCollection.mTrampSize);
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
