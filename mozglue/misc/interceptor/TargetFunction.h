/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_TargetFunction_h
#define mozilla_interceptor_TargetFunction_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"
#include "mozilla/Tuple.h"
#include "mozilla/Types.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"

#include <memory>
#include <type_traits>

namespace mozilla {
namespace interceptor {

#if defined(_M_IX86)

template <typename T>
bool CommitAndWriteShortInternal(const T& aMMPolicy, void* aDest,
                                 uint16_t aValue);

template <>
inline bool CommitAndWriteShortInternal<MMPolicyInProcess>(
    const MMPolicyInProcess& aMMPolicy, void* aDest, uint16_t aValue) {
  return aMMPolicy.WriteAtomic(aDest, aValue);
}

template <>
inline bool CommitAndWriteShortInternal<MMPolicyOutOfProcess>(
    const MMPolicyOutOfProcess& aMMPolicy, void* aDest, uint16_t aValue) {
  return aMMPolicy.Write(aDest, &aValue, sizeof(uint16_t));
}

#endif  // defined(_M_IX86)

// Forward declaration
template <typename MMPolicy>
class ReadOnlyTargetFunction;

template <typename MMPolicy>
class MOZ_STACK_CLASS WritableTargetFunction final {
  class AutoProtect final {
    using ProtectParams = Tuple<uintptr_t, uint32_t>;

   public:
    explicit AutoProtect(const MMPolicy& aMMPolicy) : mMMPolicy(aMMPolicy) {}

    AutoProtect(const MMPolicy& aMMPolicy, uintptr_t aAddr, size_t aNumBytes,
                uint32_t aNewProt)
        : mMMPolicy(aMMPolicy) {
      const uint32_t pageSize = mMMPolicy.GetPageSize();
      const uintptr_t limit = aAddr + aNumBytes - 1;
      const uintptr_t limitPageNum = limit / pageSize;
      const uintptr_t basePageNum = aAddr / pageSize;
      const uintptr_t numPagesToChange = limitPageNum - basePageNum + 1;

      // We'll use the base address of the page instead of aAddr
      uintptr_t curAddr = basePageNum * pageSize;

      // Now change the protection on each page
      for (uintptr_t curPage = 0; curPage < numPagesToChange;
           ++curPage, curAddr += pageSize) {
        uint32_t prevProt;
        if (!aMMPolicy.Protect(reinterpret_cast<void*>(curAddr), pageSize,
                               aNewProt, &prevProt)) {
          Clear();
          return;
        }

        // Save the previous protection for curAddr so that we can revert this
        // in the destructor.
        if (!mProtects.append(MakeTuple(curAddr, prevProt))) {
          Clear();
          return;
        }
      }
    }

    AutoProtect(AutoProtect&& aOther)
        : mMMPolicy(aOther.mMMPolicy), mProtects(std::move(aOther.mProtects)) {
      aOther.mProtects.clear();
    }

    ~AutoProtect() { Clear(); }

    explicit operator bool() const { return !mProtects.empty(); }

    AutoProtect(const AutoProtect&) = delete;
    AutoProtect& operator=(const AutoProtect&) = delete;
    AutoProtect& operator=(AutoProtect&&) = delete;

   private:
    void Clear() {
      const uint32_t pageSize = mMMPolicy.GetPageSize();
      for (auto&& entry : mProtects) {
        uint32_t prevProt;
        DebugOnly<bool> ok =
            mMMPolicy.Protect(reinterpret_cast<void*>(Get<0>(entry)), pageSize,
                              Get<1>(entry), &prevProt);
        MOZ_ASSERT(ok);
      }

      mProtects.clear();
    }

   private:
    const MMPolicy& mMMPolicy;
    // We include two entries of inline storage as that is most common in the
    // worst case.
    Vector<ProtectParams, 2> mProtects;
  };

 public:
  /**
   * Used to initialize an invalid WritableTargetFunction, thus signalling an
   * error.
   */
  explicit WritableTargetFunction(const MMPolicy& aMMPolicy)
      : mMMPolicy(aMMPolicy),
        mFunc(0),
        mNumBytes(0),
        mOffset(0),
        mStartWriteOffset(0),
        mAccumulatedStatus(false),
        mProtect(aMMPolicy) {}

  WritableTargetFunction(const MMPolicy& aMMPolicy, uintptr_t aFunc,
                         size_t aNumBytes)
      : mMMPolicy(aMMPolicy),
        mFunc(aFunc),
        mNumBytes(aNumBytes),
        mOffset(0),
        mStartWriteOffset(0),
        mAccumulatedStatus(true),
        mProtect(aMMPolicy, aFunc, aNumBytes, PAGE_EXECUTE_READWRITE) {}

  WritableTargetFunction(WritableTargetFunction&& aOther)
      : mMMPolicy(aOther.mMMPolicy),
        mFunc(aOther.mFunc),
        mNumBytes(aOther.mNumBytes),
        mOffset(aOther.mOffset),
        mStartWriteOffset(aOther.mStartWriteOffset),
        mLocalBytes(std::move(aOther.mLocalBytes)),
        mAccumulatedStatus(aOther.mAccumulatedStatus),
        mProtect(std::move(aOther.mProtect)) {
    aOther.mAccumulatedStatus = false;
  }

  ~WritableTargetFunction() {
    MOZ_ASSERT(mLocalBytes.empty(), "Did you forget to call Commit?");
  }

  WritableTargetFunction(const WritableTargetFunction&) = delete;
  WritableTargetFunction& operator=(const WritableTargetFunction&) = delete;
  WritableTargetFunction& operator=(WritableTargetFunction&&) = delete;

  /**
   * @return true if data was successfully committed.
   */
  bool Commit() {
    if (!(*this)) {
      return false;
    }

    if (mLocalBytes.empty()) {
      // Nothing to commit, treat like success
      return true;
    }

    bool ok =
        mMMPolicy.Write(reinterpret_cast<void*>(mFunc + mStartWriteOffset),
                        mLocalBytes.begin(), mLocalBytes.length());
    if (!ok) {
      return false;
    }

    mMMPolicy.FlushInstructionCache();

    mStartWriteOffset += mLocalBytes.length();

    mLocalBytes.clear();
    return true;
  }

  explicit operator bool() const { return mProtect && mAccumulatedStatus; }

  void WriteByte(const uint8_t& aValue) {
    if (!mLocalBytes.append(aValue)) {
      mAccumulatedStatus = false;
      return;
    }

    mOffset += sizeof(uint8_t);
  }

  Maybe<uint8_t> ReadByte() {
    // Reading is only permitted prior to any writing
    MOZ_ASSERT(mOffset == mStartWriteOffset);
    if (mOffset > mStartWriteOffset) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    uint8_t value;
    if (!mMMPolicy.Read(&value, reinterpret_cast<const void*>(mFunc + mOffset),
                        sizeof(uint8_t))) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    mOffset += sizeof(uint8_t);
    mStartWriteOffset += sizeof(uint8_t);
    return Some(value);
  }

  Maybe<uintptr_t> ReadEncodedPtr() {
    // Reading is only permitted prior to any writing
    MOZ_ASSERT(mOffset == mStartWriteOffset);
    if (mOffset > mStartWriteOffset) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    uintptr_t value;
    if (!mMMPolicy.Read(&value, reinterpret_cast<const void*>(mFunc + mOffset),
                        sizeof(uintptr_t))) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    mOffset += sizeof(uintptr_t);
    mStartWriteOffset += sizeof(uintptr_t);
    return Some(ReadOnlyTargetFunction<MMPolicy>::DecodePtr(value));
  }

  Maybe<uint32_t> ReadLong() {
    // Reading is only permitted prior to any writing
    MOZ_ASSERT(mOffset == mStartWriteOffset);
    if (mOffset > mStartWriteOffset) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    uint32_t value;
    if (!mMMPolicy.Read(&value, reinterpret_cast<const void*>(mFunc + mOffset),
                        sizeof(uint32_t))) {
      mAccumulatedStatus = false;
      return Nothing();
    }

    mOffset += sizeof(uint32_t);
    mStartWriteOffset += sizeof(uint32_t);
    return Some(value);
  }

  void WriteShort(const uint16_t& aValue) {
    if (!mLocalBytes.append(reinterpret_cast<const uint8_t*>(&aValue),
                            sizeof(uint16_t))) {
      mAccumulatedStatus = false;
      return;
    }

    mOffset += sizeof(uint16_t);
  }

#if defined(_M_IX86)
 public:
  /**
   * Commits any dirty writes, and then writes a short, atomically if possible.
   * This call may succeed in both inproc and outproc cases, but atomicity
   * is only guaranteed in the inproc case.
   */
  bool CommitAndWriteShort(const uint16_t aValue) {
    // First, commit everything that has been written until now
    if (!Commit()) {
      return false;
    }

    // Now immediately write the short, atomically if inproc
    bool ok = CommitAndWriteShortInternal(
        mMMPolicy, reinterpret_cast<void*>(mFunc + mStartWriteOffset), aValue);
    if (!ok) {
      return false;
    }

    mMMPolicy.FlushInstructionCache();
    mStartWriteOffset += sizeof(uint16_t);
    return true;
  }
#endif  // defined(_M_IX86)

  void WriteDisp32(const uintptr_t aAbsTarget) {
    intptr_t diff = static_cast<intptr_t>(aAbsTarget) -
                    static_cast<intptr_t>(mFunc + mOffset + sizeof(int32_t));

    CheckedInt<int32_t> checkedDisp(diff);
    MOZ_ASSERT(checkedDisp.isValid());
    if (!checkedDisp.isValid()) {
      mAccumulatedStatus = false;
      return;
    }

    int32_t disp = checkedDisp.value();
    if (!mLocalBytes.append(reinterpret_cast<uint8_t*>(&disp),
                            sizeof(int32_t))) {
      mAccumulatedStatus = false;
      return;
    }

    mOffset += sizeof(int32_t);
  }

#if defined(_M_X64) || defined(_M_ARM64)
  void WriteLong(const uint32_t aValue) {
    if (!mLocalBytes.append(reinterpret_cast<const uint8_t*>(&aValue),
                            sizeof(uint32_t))) {
      mAccumulatedStatus = false;
      return;
    }

    mOffset += sizeof(uint32_t);
  }
#endif  // defined(_M_X64)

  void WritePointer(const uintptr_t aAbsTarget) {
    if (!mLocalBytes.append(reinterpret_cast<const uint8_t*>(&aAbsTarget),
                            sizeof(uintptr_t))) {
      mAccumulatedStatus = false;
      return;
    }

    mOffset += sizeof(uintptr_t);
  }

  /**
   * @param aValues N-sized array of type T that specifies the set of values
   *                that are permissible in the first M bytes of the target
   *                function at aOffset.
   * @return true if M values of type T in the function are members of the
   *         set specified by aValues.
   */
  template <typename T, size_t M, size_t N>
  bool VerifyValuesAreOneOf(const T (&aValues)[N], const uint8_t aOffset = 0) {
    T buf[M];
    if (!mMMPolicy.Read(
            buf, reinterpret_cast<const void*>(mFunc + mOffset + aOffset),
            M * sizeof(T))) {
      return false;
    }

    for (auto&& fnValue : buf) {
      bool match = false;
      for (auto&& testValue : aValues) {
        match |= (fnValue == testValue);
      }

      if (!match) {
        return false;
      }
    }

    return true;
  }

  uintptr_t GetCurrentAddress() const { return mFunc + mOffset; }

 private:
  const MMPolicy& mMMPolicy;
  const uintptr_t mFunc;
  const size_t mNumBytes;
  uint32_t mOffset;
  uint32_t mStartWriteOffset;

  // In an ideal world, we'd only read 5 bytes on 32-bit and 13 bytes on 64-bit,
  // to match the minimum bytes that we need to write in in order to patch the
  // target function. Since the actual opcodes will often require us to pull in
  // extra bytes above that minimum, we set the inline storage to be larger than
  // those minima in an effort to give the Vector extra wiggle room before it
  // needs to touch the heap.
#if defined(_M_IX86)
  static const size_t kInlineStorage = 16;
#elif defined(_M_X64) || defined(_M_ARM64)
  static const size_t kInlineStorage = 32;
#endif
  Vector<uint8_t, kInlineStorage> mLocalBytes;
  bool mAccumulatedStatus;
  AutoProtect mProtect;
};

template <typename MMPolicy>
class ReadOnlyTargetBytes {
 public:
  ReadOnlyTargetBytes(const MMPolicy& aMMPolicy, const void* aBase)
      : mMMPolicy(aMMPolicy), mBase(reinterpret_cast<const uint8_t*>(aBase)) {}

  ReadOnlyTargetBytes(ReadOnlyTargetBytes&& aOther)
      : mMMPolicy(aOther.mMMPolicy), mBase(aOther.mBase) {}

  ReadOnlyTargetBytes(const ReadOnlyTargetBytes& aOther,
                      const uint32_t aOffsetFromOther = 0)
      : mMMPolicy(aOther.mMMPolicy), mBase(aOther.mBase + aOffsetFromOther) {}

  void EnsureLimit(uint32_t aDesiredLimit) {
    // In the out-proc case we use this function to read the target function's
    // bytes in the other process into a local buffer. We don't need that for
    // the in-process case because we already have direct access to our target
    // function's bytes.
  }

  uint32_t TryEnsureLimit(uint32_t aDesiredLimit) {
    // Same as EnsureLimit above.  We don't need to ensure for the in-process.
    return aDesiredLimit;
  }

  bool IsValidAtOffset(const int8_t aOffset) const {
    if (!aOffset) {
      return true;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(mBase);
    uintptr_t adjusted = base + aOffset;
    uint32_t pageSize = mMMPolicy.GetPageSize();

    // If |adjusted| is within the same page as |mBase|, we're still valid
    if ((base / pageSize) == (adjusted / pageSize)) {
      return true;
    }

    // Otherwise, let's query |adjusted|
    return mMMPolicy.IsPageAccessible(adjusted);
  }

  /**
   * This returns a pointer to a *potentially local copy* of the target
   * function's bytes. The returned pointer should not be used for any
   * pointer arithmetic relating to the target function.
   */
  const uint8_t* GetLocalBytes() const { return mBase; }

  /**
   * This returns a pointer to the target function's bytes. The returned pointer
   * may possibly belong to another process, so while it should be used for
   * pointer arithmetic, it *must not* be dereferenced.
   */
  uintptr_t GetBase() const { return reinterpret_cast<uintptr_t>(mBase); }

  const MMPolicy& GetMMPolicy() const { return mMMPolicy; }

  ReadOnlyTargetBytes& operator=(const ReadOnlyTargetBytes&) = delete;
  ReadOnlyTargetBytes& operator=(ReadOnlyTargetBytes&&) = delete;

 private:
  const MMPolicy& mMMPolicy;
  uint8_t const* const mBase;
};

template <>
class ReadOnlyTargetBytes<MMPolicyOutOfProcess> {
 public:
  ReadOnlyTargetBytes(const MMPolicyOutOfProcess& aMMPolicy, const void* aBase)
      : mMMPolicy(aMMPolicy), mBase(reinterpret_cast<const uint8_t*>(aBase)) {}

  ReadOnlyTargetBytes(ReadOnlyTargetBytes&& aOther)
      : mMMPolicy(aOther.mMMPolicy),
        mLocalBytes(std::move(aOther.mLocalBytes)),
        mBase(aOther.mBase) {}

  ReadOnlyTargetBytes(const ReadOnlyTargetBytes& aOther)
      : mMMPolicy(aOther.mMMPolicy), mBase(aOther.mBase) {
    Unused << mLocalBytes.appendAll(aOther.mLocalBytes);
  }

  ReadOnlyTargetBytes(const ReadOnlyTargetBytes& aOther,
                      const uint32_t aOffsetFromOther)
      : mMMPolicy(aOther.mMMPolicy), mBase(aOther.mBase + aOffsetFromOther) {
    if (aOffsetFromOther >= aOther.mLocalBytes.length()) {
      return;
    }

    Unused << mLocalBytes.append(aOther.mLocalBytes.begin() + aOffsetFromOther,
                                 aOther.mLocalBytes.end());
  }

  void EnsureLimit(uint32_t aDesiredLimit) {
    size_t prevSize = mLocalBytes.length();
    if (aDesiredLimit < prevSize) {
      return;
    }

    size_t newSize = aDesiredLimit + 1;
    if (newSize < kInlineStorage) {
      // Always try to read as much memory as we can at once
      newSize = kInlineStorage;
    }

    bool resizeOk = mLocalBytes.resize(newSize);
    MOZ_RELEASE_ASSERT(resizeOk);

    bool ok = mMMPolicy.Read(&mLocalBytes[prevSize], mBase + prevSize,
                             newSize - prevSize);
    if (ok) {
      return;
    }

    // We couldn't pull more bytes than needed (which may happen if those extra
    // bytes are not accessible). In this case, we try just to get the bare
    // minimum.
    newSize = aDesiredLimit + 1;
    resizeOk = mLocalBytes.resize(newSize);
    MOZ_RELEASE_ASSERT(resizeOk);

    ok = mMMPolicy.Read(&mLocalBytes[prevSize], mBase + prevSize,
                        newSize - prevSize);
    MOZ_RELEASE_ASSERT(ok);
  }

  // This function tries to ensure as many bytes as possible up to
  // |aDesiredLimit| bytes, returning how many bytes were actually ensured.
  // As EnsureLimit does, we allocate an extra byte in local to make sure
  // mLocalBytes always has at least one byte even though the target memory
  // was inaccessible at all.
  uint32_t TryEnsureLimit(uint32_t aDesiredLimit) {
    size_t prevSize = mLocalBytes.length();
    if (aDesiredLimit < prevSize) {
      return aDesiredLimit;
    }

    size_t newSize = aDesiredLimit;
    if (newSize < kInlineStorage) {
      // Always try to read as much memory as we can at once
      newSize = kInlineStorage;
    }

    bool resizeOk = mLocalBytes.resize(newSize);
    MOZ_RELEASE_ASSERT(resizeOk);

    size_t bytesRead = mMMPolicy.TryRead(&mLocalBytes[prevSize],
                                         mBase + prevSize, newSize - prevSize);

    newSize = prevSize + bytesRead;

    resizeOk = mLocalBytes.resize(newSize + 1);
    MOZ_RELEASE_ASSERT(resizeOk);

    mLocalBytes[newSize] = 0;
    return newSize;
  }

  bool IsValidAtOffset(const int8_t aOffset) const {
    if (!aOffset) {
      return true;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(mBase);
    uintptr_t adjusted = base + aOffset;
    uint32_t pageSize = mMMPolicy.GetPageSize();

    // If |adjusted| is within the same page as |mBase|, we're still valid
    if ((base / pageSize) == (adjusted / pageSize)) {
      return true;
    }

    // Otherwise, let's query |adjusted|
    return mMMPolicy.IsPageAccessible(adjusted);
  }

  /**
   * This returns a pointer to a *potentially local copy* of the target
   * function's bytes. The returned pointer should not be used for any
   * pointer arithmetic relating to the target function.
   */
  const uint8_t* GetLocalBytes() const {
    if (mLocalBytes.empty()) {
      return nullptr;
    }

    return mLocalBytes.begin();
  }

  /**
   * This returns a pointer to the target function's bytes. The returned pointer
   * may possibly belong to another process, so while it should be used for
   * pointer arithmetic, it *must not* be dereferenced.
   */
  uintptr_t GetBase() const { return reinterpret_cast<uintptr_t>(mBase); }

  const MMPolicyOutOfProcess& GetMMPolicy() const { return mMMPolicy; }

  ReadOnlyTargetBytes& operator=(const ReadOnlyTargetBytes&) = delete;
  ReadOnlyTargetBytes& operator=(ReadOnlyTargetBytes&&) = delete;

 private:
  // In an ideal world, we'd only read 5 bytes on 32-bit and 13 bytes on 64-bit,
  // to match the minimum bytes that we need to write in in order to patch the
  // target function. Since the actual opcodes will often require us to pull in
  // extra bytes above that minimum, we set the inline storage to be larger than
  // those minima in an effort to give the Vector extra wiggle room before it
  // needs to touch the heap.
#if defined(_M_IX86)
  static const size_t kInlineStorage = 16;
#elif defined(_M_X64) || defined(_M_ARM64)
  static const size_t kInlineStorage = 32;
#endif

  const MMPolicyOutOfProcess& mMMPolicy;
  Vector<uint8_t, kInlineStorage> mLocalBytes;
  uint8_t const* const mBase;
};

template <typename MMPolicy>
class TargetBytesPtr {
 public:
  typedef TargetBytesPtr<MMPolicy> Type;

  static Type Make(const MMPolicy& aMMPolicy, const void* aFunc) {
    return TargetBytesPtr(aMMPolicy, aFunc);
  }

  static Type CopyFromOffset(const TargetBytesPtr& aOther,
                             const uint32_t aOffsetFromOther) {
    return TargetBytesPtr(aOther, aOffsetFromOther);
  }

  ReadOnlyTargetBytes<MMPolicy>* operator->() { return &mTargetBytes; }

  TargetBytesPtr(TargetBytesPtr&& aOther)
      : mTargetBytes(std::move(aOther.mTargetBytes)) {}

  TargetBytesPtr(const TargetBytesPtr& aOther)
      : mTargetBytes(aOther.mTargetBytes) {}

  TargetBytesPtr& operator=(const TargetBytesPtr&) = delete;
  TargetBytesPtr& operator=(TargetBytesPtr&&) = delete;

 private:
  TargetBytesPtr(const MMPolicy& aMMPolicy, const void* aFunc)
      : mTargetBytes(aMMPolicy, aFunc) {}

  TargetBytesPtr(const TargetBytesPtr& aOther, const uint32_t aOffsetFromOther)
      : mTargetBytes(aOther.mTargetBytes, aOffsetFromOther) {}

  ReadOnlyTargetBytes<MMPolicy> mTargetBytes;
};

template <>
class TargetBytesPtr<MMPolicyOutOfProcess> {
 public:
  typedef std::shared_ptr<ReadOnlyTargetBytes<MMPolicyOutOfProcess>> Type;

  static Type Make(const MMPolicyOutOfProcess& aMMPolicy, const void* aFunc) {
    return std::make_shared<ReadOnlyTargetBytes<MMPolicyOutOfProcess>>(
        aMMPolicy, aFunc);
  }

  static Type CopyFromOffset(const Type& aOther,
                             const uint32_t aOffsetFromOther) {
    return std::make_shared<ReadOnlyTargetBytes<MMPolicyOutOfProcess>>(
        *aOther, aOffsetFromOther);
  }
};

template <typename MMPolicy>
class MOZ_STACK_CLASS ReadOnlyTargetFunction final {
 public:
  ReadOnlyTargetFunction(const MMPolicy& aMMPolicy, const void* aFunc)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(aMMPolicy, aFunc)),
        mOffset(0) {}

  ReadOnlyTargetFunction(const MMPolicy& aMMPolicy, FARPROC aFunc)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(
            aMMPolicy, reinterpret_cast<const void*>(aFunc))),
        mOffset(0) {}

  ReadOnlyTargetFunction(const MMPolicy& aMMPolicy, uintptr_t aFunc)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(
            aMMPolicy, reinterpret_cast<const void*>(aFunc))),
        mOffset(0) {}

  ReadOnlyTargetFunction(ReadOnlyTargetFunction&& aOther)
      : mTargetBytes(std::move(aOther.mTargetBytes)), mOffset(aOther.mOffset) {}

  ReadOnlyTargetFunction& operator=(const ReadOnlyTargetFunction&) = delete;
  ReadOnlyTargetFunction& operator=(ReadOnlyTargetFunction&&) = delete;

  ~ReadOnlyTargetFunction() = default;

  ReadOnlyTargetFunction operator+(const uint32_t aOffset) const {
    return ReadOnlyTargetFunction(*this, mOffset + aOffset);
  }

  uintptr_t GetBaseAddress() const { return mTargetBytes->GetBase(); }

  uintptr_t GetAddress() const { return mTargetBytes->GetBase() + mOffset; }

  uintptr_t AsEncodedPtr() const {
    return EncodePtr(
        reinterpret_cast<void*>(mTargetBytes->GetBase() + mOffset));
  }

  static uintptr_t EncodePtr(void* aPtr) {
    return reinterpret_cast<uintptr_t>(::EncodePointer(aPtr));
  }

  static uintptr_t DecodePtr(uintptr_t aEncodedPtr) {
    return reinterpret_cast<uintptr_t>(
        ::DecodePointer(reinterpret_cast<PVOID>(aEncodedPtr)));
  }

  bool IsValidAtOffset(const int8_t aOffset) const {
    return mTargetBytes->IsValidAtOffset(aOffset);
  }

#if defined(_M_ARM64)

  uint32_t ReadNextInstruction() {
    mTargetBytes->EnsureLimit(mOffset + sizeof(uint32_t));
    uint32_t instruction = *reinterpret_cast<const uint32_t*>(
        mTargetBytes->GetLocalBytes() + mOffset);
    mOffset += sizeof(uint32_t);
    return instruction;
  }

  bool BackUpOneInstruction() {
    if (mOffset < sizeof(uint32_t)) {
      return false;
    }

    mOffset -= sizeof(uint32_t);
    return true;
  }

#else

  uint8_t const& operator*() const {
    mTargetBytes->EnsureLimit(mOffset);
    return *(mTargetBytes->GetLocalBytes() + mOffset);
  }

  uint8_t const& operator[](uint32_t aIndex) const {
    mTargetBytes->EnsureLimit(mOffset + aIndex);
    return *(mTargetBytes->GetLocalBytes() + mOffset + aIndex);
  }

  ReadOnlyTargetFunction& operator++() {
    ++mOffset;
    return *this;
  }

  ReadOnlyTargetFunction& operator+=(uint32_t aDelta) {
    mOffset += aDelta;
    return *this;
  }

  uintptr_t ReadDisp32AsAbsolute() {
    mTargetBytes->EnsureLimit(mOffset + sizeof(int32_t));
    int32_t disp = *reinterpret_cast<const int32_t*>(
        mTargetBytes->GetLocalBytes() + mOffset);
    uintptr_t result =
        mTargetBytes->GetBase() + mOffset + sizeof(int32_t) + disp;
    mOffset += sizeof(int32_t);
    return result;
  }

  bool IsRelativeShortJump(uintptr_t* aOutTarget) {
    if ((*this)[0] == 0xeb) {
      int8_t offset = static_cast<int8_t>((*this)[1]);
      *aOutTarget = GetAddress() + 2 + offset;
      return true;
    }
    return false;
  }

#  if defined(_M_X64)
  // Currently this function is used only in x64.
  bool IsRelativeNearJump(uintptr_t* aOutTarget) {
    if ((*this)[0] == 0xe9) {
      *aOutTarget = (*this + 1).ReadDisp32AsAbsolute();
      return true;
    }
    return false;
  }
#  endif  // defined(_M_X64)

  bool IsIndirectNearJump(uintptr_t* aOutTarget) {
    if ((*this)[0] == 0xff && (*this)[1] == 0x25) {
#  if defined(_M_X64)
      *aOutTarget = (*this + 2).ChasePointerFromDisp();
#  else
      *aOutTarget = (*this + 2).template ChasePointer<uintptr_t*>();
#  endif  // defined(_M_X64)
      return true;
    }
#  if defined(_M_X64)
    else if ((*this)[0] == 0x48 && (*this)[1] == 0xff && (*this)[2] == 0x25) {
      // According to Intel SDM, JMP does not have REX.W except JMP m16:64,
      // but CPU can execute JMP r/m32 with REX.W.  We handle it just in case.
      *aOutTarget = (*this + 3).ChasePointerFromDisp();
      return true;
    }
#  endif  // defined(_M_X64)
    return false;
  }

#endif  // defined(_M_ARM64)

  void Rewind() { mOffset = 0; }

  uint32_t GetOffset() const { return mOffset; }

  uintptr_t OffsetToAbsolute(const uint8_t aOffset) const {
    return mTargetBytes->GetBase() + mOffset + aOffset;
  }

  uintptr_t GetCurrentAbsolute() const { return OffsetToAbsolute(0); }

  /**
   * This method promotes the code referenced by this object to be writable.
   *
   * @param aLen    The length of the function's code to make writable. If set
   *                to zero, this object's current offset is used as the length.
   * @param aOffset The result's base address will be offset from this
   *                object's base address by |aOffset| bytes. This value may be
   *                negative.
   */
  WritableTargetFunction<MMPolicy> Promote(const uint32_t aLen = 0,
                                           const int8_t aOffset = 0) const {
    const uint32_t effectiveLength = aLen ? aLen : mOffset;
    MOZ_RELEASE_ASSERT(effectiveLength,
                       "Cannot Promote a zero-length function");

    if (!mTargetBytes->IsValidAtOffset(aOffset)) {
      return WritableTargetFunction<MMPolicy>(mTargetBytes->GetMMPolicy());
    }

    WritableTargetFunction<MMPolicy> result(mTargetBytes->GetMMPolicy(),
                                            mTargetBytes->GetBase() + aOffset,
                                            effectiveLength);

    return result;
  }

 private:
  template <typename T>
  struct ChasePointerHelper {
    template <typename MMPolicy_>
    static T Result(const MMPolicy_&, T aValue) {
      return aValue;
    }
  };

  template <typename T>
  struct ChasePointerHelper<T*> {
    template <typename MMPolicy_>
    static auto Result(const MMPolicy_& aPolicy, T* aValue) {
      ReadOnlyTargetFunction<MMPolicy_> ptr(aPolicy, aValue);
      return ptr.template ChasePointer<T>();
    }
  };

 public:
  // Keep chasing pointers until T is not a pointer type anymore
  template <typename T>
  auto ChasePointer() {
    mTargetBytes->EnsureLimit(mOffset + sizeof(T));
    const std::remove_cv_t<T> result =
        *reinterpret_cast<const std::remove_cv_t<T>*>(
            mTargetBytes->GetLocalBytes() + mOffset);
    return ChasePointerHelper<std::remove_cv_t<T>>::Result(
        mTargetBytes->GetMMPolicy(), result);
  }

  uintptr_t ChasePointerFromDisp() {
    uintptr_t ptrFromDisp = ReadDisp32AsAbsolute();
    ReadOnlyTargetFunction<MMPolicy> ptr(
        mTargetBytes->GetMMPolicy(),
        reinterpret_cast<const void*>(ptrFromDisp));
    return ptr.template ChasePointer<uintptr_t>();
  }

 private:
  ReadOnlyTargetFunction(const ReadOnlyTargetFunction& aOther)
      : mTargetBytes(aOther.mTargetBytes), mOffset(aOther.mOffset) {}

  ReadOnlyTargetFunction(const ReadOnlyTargetFunction& aOther,
                         const uint32_t aOffsetFromOther)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::CopyFromOffset(
            aOther.mTargetBytes, aOffsetFromOther)),
        mOffset(0) {}

 private:
  mutable typename TargetBytesPtr<MMPolicy>::Type mTargetBytes;
  uint32_t mOffset;
};

template <typename MMPolicy, typename T>
class MOZ_STACK_CLASS TargetObject {
  mutable typename TargetBytesPtr<MMPolicy>::Type mTargetBytes;

  TargetObject(const MMPolicy& aMMPolicy, const void* aBaseAddress)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(aMMPolicy, aBaseAddress)) {
    mTargetBytes->EnsureLimit(sizeof(T));
  }

 public:
  explicit TargetObject(const MMPolicy& aMMPolicy)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(aMMPolicy, nullptr)) {}

  TargetObject(const MMPolicy& aMMPolicy, uintptr_t aBaseAddress)
      : TargetObject(aMMPolicy, reinterpret_cast<const void*>(aBaseAddress)) {}

  TargetObject(const TargetObject&) = delete;
  TargetObject(TargetObject&&) = delete;
  TargetObject& operator=(const TargetObject&) = delete;
  TargetObject& operator=(TargetObject&&) = delete;

  explicit operator bool() const {
    return mTargetBytes->GetBase() && mTargetBytes->GetLocalBytes();
  }

  const T* operator->() const {
    return reinterpret_cast<const T*>(mTargetBytes->GetLocalBytes());
  }

  const T* GetLocalBase() const {
    return reinterpret_cast<const T*>(mTargetBytes->GetLocalBytes());
  }
};

template <typename MMPolicy, typename T>
class MOZ_STACK_CLASS TargetObjectArray {
  mutable typename TargetBytesPtr<MMPolicy>::Type mTargetBytes;
  size_t mNumOfItems;

  TargetObjectArray(const MMPolicy& aMMPolicy, const void* aBaseAddress,
                    size_t aNumOfItems)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(aMMPolicy, aBaseAddress)),
        mNumOfItems(aNumOfItems) {
    uint32_t itemsRead =
        mTargetBytes->TryEnsureLimit(sizeof(T) * mNumOfItems) / sizeof(T);
    // itemsRead may be bigger than the requested amount because of buffering,
    // but mNumOfItems should not include extra bytes of buffering.
    if (itemsRead < mNumOfItems) {
      mNumOfItems = itemsRead;
    }
  }

  const T* GetLocalBase() const {
    return reinterpret_cast<const T*>(mTargetBytes->GetLocalBytes());
  }

 public:
  explicit TargetObjectArray(const MMPolicy& aMMPolicy)
      : mTargetBytes(TargetBytesPtr<MMPolicy>::Make(aMMPolicy, nullptr)),
        mNumOfItems(0) {}

  TargetObjectArray(const MMPolicy& aMMPolicy, uintptr_t aBaseAddress,
                    size_t aNumOfItems)
      : TargetObjectArray(aMMPolicy,
                          reinterpret_cast<const void*>(aBaseAddress),
                          aNumOfItems) {}

  TargetObjectArray(const TargetObjectArray&) = delete;
  TargetObjectArray(TargetObjectArray&&) = delete;
  TargetObjectArray& operator=(const TargetObjectArray&) = delete;
  TargetObjectArray& operator=(TargetObjectArray&&) = delete;

  explicit operator bool() const {
    return mTargetBytes->GetBase() && mNumOfItems;
  }

  const T* operator[](size_t aIndex) const {
    if (aIndex >= mNumOfItems) {
      return nullptr;
    }

    return &GetLocalBase()[aIndex];
  }

  template <typename Comparator>
  bool BinarySearchIf(const Comparator& aCompare,
                      size_t* aMatchOrInsertionPoint) const {
    return mozilla::BinarySearchIf(GetLocalBase(), 0, mNumOfItems, aCompare,
                                   aMatchOrInsertionPoint);
  }
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_TargetFunction_h
