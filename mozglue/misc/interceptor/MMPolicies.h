/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_MMPolicies_h
#define mozilla_interceptor_MMPolicies_h

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/Types.h"
#include "mozilla/WindowsMapRemoteView.h"

#include <windows.h>

// MinGW does not have these definitions yet
#if defined(__MINGW32__)
typedef struct _MEM_ADDRESS_REQUIREMENTS {
  PVOID LowestStartingAddress;
  PVOID HighestEndingAddress;
  SIZE_T Alignment;
} MEM_ADDRESS_REQUIREMENTS, *PMEM_ADDRESS_REQUIREMENTS;

typedef enum MEM_EXTENDED_PARAMETER_TYPE {
  MemExtendedParameterInvalidType = 0,
  MemExtendedParameterAddressRequirements,
  MemExtendedParameterNumaNode,
  MemExtendedParameterPartitionHandle,
  MemExtendedParameterUserPhysicalHandle,
  MemExtendedParameterAttributeFlags,
  MemExtendedParameterMax
} MEM_EXTENDED_PARAMETER_TYPE,
    *PMEM_EXTENDED_PARAMETER_TYPE;

#  define MEM_EXTENDED_PARAMETER_TYPE_BITS 8

typedef struct DECLSPEC_ALIGN(8) MEM_EXTENDED_PARAMETER {
  struct {
    DWORD64 Type : MEM_EXTENDED_PARAMETER_TYPE_BITS;
    DWORD64 Reserved : 64 - MEM_EXTENDED_PARAMETER_TYPE_BITS;
  } DUMMYSTRUCTNAME;

  union {
    DWORD64 ULong64;
    PVOID Pointer;
    SIZE_T Size;
    HANDLE Handle;
    DWORD ULong;
  } DUMMYUNIONNAME;

} MEM_EXTENDED_PARAMETER, *PMEM_EXTENDED_PARAMETER;
#endif  // defined(__MINGW32__)

#if (NTDDI_VERSION < NTDDI_WIN10_RS4) || defined(__MINGW32__)
PVOID WINAPI VirtualAlloc2(HANDLE Process, PVOID BaseAddress, SIZE_T Size,
                           ULONG AllocationType, ULONG PageProtection,
                           MEM_EXTENDED_PARAMETER* ExtendedParameters,
                           ULONG ParameterCount);
PVOID WINAPI MapViewOfFile3(HANDLE FileMapping, HANDLE Process,
                            PVOID BaseAddress, ULONG64 Offset, SIZE_T ViewSize,
                            ULONG AllocationType, ULONG PageProtection,
                            MEM_EXTENDED_PARAMETER* ExtendedParameters,
                            ULONG ParameterCount);
#endif  // (NTDDI_VERSION < NTDDI_WIN10_RS4) || defined(__MINGW32__)

// _CRT_RAND_S is not defined everywhere, but we need it.
#if !defined(_CRT_RAND_S)
extern "C" errno_t rand_s(unsigned int* randomValue);
#endif  // !defined(_CRT_RAND_S)

namespace mozilla {
namespace interceptor {

class MOZ_TRIVIAL_CTOR_DTOR MMPolicyBase {
 protected:
  static uintptr_t AlignDown(const uintptr_t aUnaligned,
                             const uintptr_t aAlignTo) {
    MOZ_ASSERT(IsPowerOfTwo(aAlignTo));
#pragma warning(suppress : 4146)
    return aUnaligned & (-aAlignTo);
  }

  static uintptr_t AlignUp(const uintptr_t aUnaligned,
                           const uintptr_t aAlignTo) {
    MOZ_ASSERT(IsPowerOfTwo(aAlignTo));
#pragma warning(suppress : 4146)
    return aUnaligned + ((-aUnaligned) & (aAlignTo - 1));
  }

 public:
  static DWORD ComputeAllocationSize(const uint32_t aRequestedSize) {
    MOZ_ASSERT(aRequestedSize);
    DWORD result = aRequestedSize;

    const uint32_t granularity = GetAllocGranularity();

    uint32_t mod = aRequestedSize % granularity;
    if (mod) {
      result += (granularity - mod);
    }

    return result;
  }

  static DWORD GetAllocGranularity() {
    static const DWORD kAllocGranularity = []() -> DWORD {
      SYSTEM_INFO sysInfo;
      ::GetSystemInfo(&sysInfo);
      return sysInfo.dwAllocationGranularity;
    }();

    return kAllocGranularity;
  }

  static DWORD GetPageSize() {
    static const DWORD kPageSize = []() -> DWORD {
      SYSTEM_INFO sysInfo;
      ::GetSystemInfo(&sysInfo);
      return sysInfo.dwPageSize;
    }();

    return kPageSize;
  }

  static uintptr_t GetMaxUserModeAddress() {
    static const uintptr_t kMaxUserModeAddr = []() -> uintptr_t {
      SYSTEM_INFO sysInfo;
      ::GetSystemInfo(&sysInfo);
      return reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);
    }();

    return kMaxUserModeAddr;
  }

  static const uint8_t* GetLowerBound(const Span<const uint8_t>& aBounds) {
    return &(*aBounds.cbegin());
  }

  static const uint8_t* GetUpperBoundIncl(const Span<const uint8_t>& aBounds) {
    // We return an upper bound that is inclusive.
    return &(*(aBounds.cend() - 1));
  }

  static const uint8_t* GetUpperBoundExcl(const Span<const uint8_t>& aBounds) {
    // We return an upper bound that is exclusive by adding 1 to the inclusive
    // upper bound.
    return GetUpperBoundIncl(aBounds) + 1;
  }

  /**
   * It is convenient for us to provide address range information based on a
   * "pivot" and a distance from that pivot, as branch instructions operate
   * within a range of the program counter. OTOH, to actually manage the
   * regions of memory, it is easier to think about them in terms of their
   * lower and upper bounds. This function converts from the former format to
   * the latter format.
   */
  static Maybe<Span<const uint8_t>> SpanFromPivotAndDistance(
      const uint32_t aSize, const uintptr_t aPivotAddr,
      const uint32_t aMaxDistanceFromPivot) {
    if (!aPivotAddr || !aMaxDistanceFromPivot) {
      return Nothing();
    }

    // We don't allow regions below 1MB so that we're not allocating near any
    // sensitive areas in our address space.
    const uintptr_t kMinAllowableAddress = 0x100000;

    const uintptr_t kGranularity(GetAllocGranularity());

    // We subtract the max distance from the pivot to determine our lower bound.
    CheckedInt<uintptr_t> lowerBound(aPivotAddr);
    lowerBound -= aMaxDistanceFromPivot;
    if (lowerBound.isValid()) {
      // In this case, the subtraction has not underflowed, but we still want
      // the lower bound to be at least kMinAllowableAddress.
      lowerBound = std::max(lowerBound.value(), kMinAllowableAddress);
    } else {
      // In this case, we underflowed. Forcibly set the lower bound to
      // kMinAllowableAddress.
      lowerBound = CheckedInt<uintptr_t>(kMinAllowableAddress);
    }

    // Align up to the next unit of allocation granularity when necessary.
    lowerBound = AlignUp(lowerBound.value(), kGranularity);
    MOZ_ASSERT(lowerBound.isValid());
    if (!lowerBound.isValid()) {
      return Nothing();
    }

    // We must ensure that our region is below the maximum allowable user-mode
    // address, or our reservation will fail.
    const uintptr_t kMaxUserModeAddr = GetMaxUserModeAddress();

    // We add the max distance from the pivot to determine our upper bound.
    CheckedInt<uintptr_t> upperBound(aPivotAddr);
    upperBound += aMaxDistanceFromPivot;
    if (upperBound.isValid()) {
      // In this case, the addition has not overflowed, but we still want
      // the upper bound to be at most kMaxUserModeAddr.
      upperBound = std::min(upperBound.value(), kMaxUserModeAddr);
    } else {
      // In this case, we overflowed. Forcibly set the upper bound to
      // kMaxUserModeAddr.
      upperBound = CheckedInt<uintptr_t>(kMaxUserModeAddr);
    }

    // Subtract the desired allocation size so that any chunk allocated in the
    // region will be reachable.
    upperBound -= aSize;
    if (!upperBound.isValid()) {
      return Nothing();
    }

    // Align down to the next unit of allocation granularity when necessary.
    upperBound = AlignDown(upperBound.value(), kGranularity);
    if (!upperBound.isValid()) {
      return Nothing();
    }

    MOZ_ASSERT(lowerBound.value() < upperBound.value());
    if (lowerBound.value() >= upperBound.value()) {
      return Nothing();
    }

    // Return the result as a Span
    return Some(MakeSpan(reinterpret_cast<const uint8_t*>(lowerBound.value()),
                         upperBound.value() - lowerBound.value()));
  }

  /**
   * This function locates a virtual memory region of |aDesiredBytesLen| that
   * resides in the interval [aRangeMin, aRangeMax). We do this by scanning the
   * virtual memory space for a block of unallocated memory that is sufficiently
   * large.
   */
  static PVOID FindRegion(HANDLE aProcess, const size_t aDesiredBytesLen,
                          const uint8_t* aRangeMin, const uint8_t* aRangeMax) {
    const DWORD kGranularity = GetAllocGranularity();
    MOZ_ASSERT(aDesiredBytesLen >= kGranularity);
    if (!aDesiredBytesLen) {
      return nullptr;
    }

    MOZ_ASSERT(aRangeMin < aRangeMax);
    if (aRangeMin >= aRangeMax) {
      return nullptr;
    }

    // Generate a randomized base address that falls within the interval
    // [aRangeMin, aRangeMax - aDesiredBytesLen]
    unsigned int rnd = 0;
    rand_s(&rnd);

    // Reduce rnd to a value that falls within the acceptable range
    uintptr_t maxOffset =
        (aRangeMax - aRangeMin - aDesiredBytesLen) / kGranularity;
    uintptr_t offset = (uintptr_t(rnd) % maxOffset) * kGranularity;

    // Start searching at this address
    const uint8_t* address = aRangeMin + offset;
    // The max address needs to incorporate the desired length
    const uint8_t* const kMaxPtr = aRangeMax - aDesiredBytesLen;

    MOZ_DIAGNOSTIC_ASSERT(address <= kMaxPtr);

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T len = sizeof(mbi);

    // Scan the range for a free chunk that is at least as large as
    // aDesiredBytesLen
    while (address <= kMaxPtr &&
           ::VirtualQueryEx(aProcess, address, &mbi, len)) {
      if (mbi.State == MEM_FREE && mbi.RegionSize >= aDesiredBytesLen) {
        return mbi.BaseAddress;
      }

      address =
          reinterpret_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }

    return nullptr;
  }

  /**
   * This function reserves a |aSize| block of virtual memory.
   *
   * When |aBounds| is Nothing, it just calls |aReserveFn| and lets Windows
   * choose the base address.
   *
   * Otherwise, it tries to call |aReserveRangeFn| to reserve the memory within
   * the bounds provided by |aBounds|. It is advantageous to use this function
   * because the OS's VM manager has better information as to which base
   * addresses are the best to use.
   *
   * If |aReserveRangeFn| retuns Nothing, this means that the platform support
   * is not available. In that case, we fall back to manually computing a region
   * to use for reserving the memory by calling |FindRegion|.
   */
  template <typename ReserveFnT, typename ReserveRangeFnT>
  static PVOID Reserve(HANDLE aProcess, const uint32_t aSize,
                       const ReserveFnT& aReserveFn,
                       const ReserveRangeFnT& aReserveRangeFn,
                       const Maybe<Span<const uint8_t>>& aBounds) {
    if (!aBounds) {
      // No restrictions, let the OS choose the base address
      return aReserveFn(aProcess, nullptr, aSize);
    }

    const uint8_t* lowerBound = GetLowerBound(aBounds.ref());
    const uint8_t* upperBoundExcl = GetUpperBoundExcl(aBounds.ref());

    Maybe<PVOID> result =
        aReserveRangeFn(aProcess, aSize, lowerBound, upperBoundExcl);
    if (result) {
      return result.value();
    }

    // aReserveRangeFn is not available on this machine. We'll do a manual
    // search.

    size_t curAttempt = 0;
    const size_t kMaxAttempts = 8;

    // We loop here because |FindRegion| may return a base address that
    // is reserved elsewhere before we have had a chance to reserve it
    // ourselves.
    while (curAttempt < kMaxAttempts) {
      PVOID base = FindRegion(aProcess, aSize, lowerBound, upperBoundExcl);
      if (!base) {
        return nullptr;
      }

      result = Some(aReserveFn(aProcess, base, aSize));
      if (result.value()) {
        return result.value();
      }

      ++curAttempt;
    }

    // If we run out of attempts, we fall through to the default case where
    // the system chooses any base address it wants. In that case, the hook
    // will be set on a best-effort basis.

    return aReserveFn(aProcess, nullptr, aSize);
  }
};

class MOZ_TRIVIAL_CTOR_DTOR MMPolicyInProcess : public MMPolicyBase {
 public:
  typedef MMPolicyInProcess MMPolicyT;

  constexpr MMPolicyInProcess()
      : mBase(nullptr), mReservationSize(0), mCommitOffset(0) {}

  MMPolicyInProcess(const MMPolicyInProcess&) = delete;
  MMPolicyInProcess& operator=(const MMPolicyInProcess&) = delete;

  MMPolicyInProcess(MMPolicyInProcess&& aOther)
      : mBase(nullptr), mReservationSize(0), mCommitOffset(0) {
    *this = std::move(aOther);
  }

  MMPolicyInProcess& operator=(MMPolicyInProcess&& aOther) {
    mBase = aOther.mBase;
    aOther.mBase = nullptr;

    mCommitOffset = aOther.mCommitOffset;
    aOther.mCommitOffset = 0;

    mReservationSize = aOther.mReservationSize;
    aOther.mReservationSize = 0;

    return *this;
  }

  explicit operator bool() const { return !!mBase; }

  /**
   * Should we unhook everything upon destruction?
   */
  bool ShouldUnhookUponDestruction() const { return true; }

  bool Read(void* aToPtr, const void* aFromPtr, size_t aLen) const {
    ::memcpy(aToPtr, aFromPtr, aLen);
    return true;
  }

  bool Write(void* aToPtr, const void* aFromPtr, size_t aLen) const {
    ::memcpy(aToPtr, aFromPtr, aLen);
    return true;
  }

#if defined(_M_IX86)
  bool WriteAtomic(void* aDestPtr, const uint16_t aValue) const {
    *static_cast<uint16_t*>(aDestPtr) = aValue;
    return true;
  }
#endif  // defined(_M_IX86)

  bool Protect(void* aVAddress, size_t aSize, uint32_t aProtFlags,
               uint32_t* aPrevProtFlags) const {
    MOZ_ASSERT(aPrevProtFlags);
    BOOL ok = ::VirtualProtect(aVAddress, aSize, aProtFlags,
                               reinterpret_cast<PDWORD>(aPrevProtFlags));
    if (!ok && aPrevProtFlags) {
      // VirtualProtect can fail but still set valid protection flags.
      // Let's clear those upon failure.
      *aPrevProtFlags = 0;
    }

    return !!ok;
  }

  /**
   * @return true if the page that hosts aVAddress is accessible.
   */
  bool IsPageAccessible(void* aVAddress) const {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = ::VirtualQuery(aVAddress, &mbi, sizeof(mbi));

    return result && mbi.AllocationProtect && (mbi.Type & MEM_IMAGE) &&
           mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS;
  }

  bool FlushInstructionCache() const {
    return !!::FlushInstructionCache(::GetCurrentProcess(), nullptr, 0);
  }

  static DWORD GetTrampWriteProtFlags() { return PAGE_EXECUTE_READWRITE; }

#if defined(_M_X64)
  bool IsTrampolineSpaceInLowest2GB() const {
    return (mBase + mReservationSize) <=
           reinterpret_cast<uint8_t*>(0x0000000080000000ULL);
  }
#endif  // defined(_M_X64)

 protected:
  uint8_t* GetLocalView() const { return mBase; }

  uintptr_t GetRemoteView() const {
    // Same as local view for in-process
    return reinterpret_cast<uintptr_t>(mBase);
  }

  /**
   * @return the effective number of bytes reserved, or 0 on failure
   */
  uint32_t Reserve(const uint32_t aSize,
                   const Maybe<Span<const uint8_t>>& aBounds) {
    if (!aSize) {
      return 0;
    }

    if (mBase) {
      MOZ_ASSERT(mReservationSize >= aSize);
      return mReservationSize;
    }

    mReservationSize = ComputeAllocationSize(aSize);

    auto reserveFn = [](HANDLE aProcess, PVOID aBase, uint32_t aSize) -> PVOID {
      return ::VirtualAlloc(aBase, aSize, MEM_RESERVE, PAGE_NOACCESS);
    };

    auto reserveWithinRangeFn =
        [](HANDLE aProcess, uint32_t aSize, const uint8_t* aRangeMin,
           const uint8_t* aRangeMaxExcl) -> Maybe<PVOID> {
      static const StaticDynamicallyLinkedFunctionPtr<decltype(
          &::VirtualAlloc2)>
          pVirtualAlloc2(L"kernelbase.dll", "VirtualAlloc2");
      if (!pVirtualAlloc2) {
        return Nothing();
      }

      // NB: MEM_ADDRESS_REQUIREMENTS::HighestEndingAddress is *inclusive*
      MEM_ADDRESS_REQUIREMENTS memReq = {
          const_cast<uint8_t*>(aRangeMin),
          const_cast<uint8_t*>(aRangeMaxExcl - 1)};

      MEM_EXTENDED_PARAMETER memParam = {};
      memParam.Type = MemExtendedParameterAddressRequirements;
      memParam.Pointer = &memReq;

      return Some(pVirtualAlloc2(aProcess, nullptr, aSize, MEM_RESERVE,
                                 PAGE_NOACCESS, &memParam, 1));
    };

    mBase = static_cast<uint8_t*>(
        MMPolicyBase::Reserve(::GetCurrentProcess(), mReservationSize,
                              reserveFn, reserveWithinRangeFn, aBounds));

    if (!mBase) {
      return 0;
    }

    return mReservationSize;
  }

  bool MaybeCommitNextPage(const uint32_t aRequestedOffset,
                           const uint32_t aRequestedLength) {
    if (!(*this)) {
      return false;
    }

    uint32_t limit = aRequestedOffset + aRequestedLength - 1;
    if (limit < mCommitOffset) {
      // No commit required
      return true;
    }

    MOZ_DIAGNOSTIC_ASSERT(mCommitOffset < mReservationSize);
    if (mCommitOffset >= mReservationSize) {
      return false;
    }

    PVOID local = ::VirtualAlloc(mBase + mCommitOffset, GetPageSize(),
                                 MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!local) {
      return false;
    }

    mCommitOffset += GetPageSize();
    return true;
  }

 private:
  uint8_t* mBase;
  uint32_t mReservationSize;
  uint32_t mCommitOffset;
};

class MMPolicyOutOfProcess : public MMPolicyBase {
 public:
  typedef MMPolicyOutOfProcess MMPolicyT;

  explicit MMPolicyOutOfProcess(HANDLE aProcess)
      : mProcess(nullptr),
        mMapping(nullptr),
        mLocalView(nullptr),
        mRemoteView(nullptr),
        mReservationSize(0),
        mCommitOffset(0) {
    MOZ_ASSERT(aProcess);
    ::DuplicateHandle(::GetCurrentProcess(), aProcess, ::GetCurrentProcess(),
                      &mProcess, kAccessFlags, FALSE, 0);
    MOZ_ASSERT(mProcess);
  }

  explicit MMPolicyOutOfProcess(DWORD aPid)
      : mProcess(::OpenProcess(kAccessFlags, FALSE, aPid)),
        mMapping(nullptr),
        mLocalView(nullptr),
        mRemoteView(nullptr),
        mReservationSize(0),
        mCommitOffset(0) {
    MOZ_ASSERT(mProcess);
  }

  ~MMPolicyOutOfProcess() { Destroy(); }

  MMPolicyOutOfProcess(MMPolicyOutOfProcess&& aOther)
      : mProcess(nullptr),
        mMapping(nullptr),
        mLocalView(nullptr),
        mRemoteView(nullptr),
        mReservationSize(0),
        mCommitOffset(0) {
    *this = std::move(aOther);
  }

  MMPolicyOutOfProcess(const MMPolicyOutOfProcess& aOther) = delete;
  MMPolicyOutOfProcess& operator=(const MMPolicyOutOfProcess&) = delete;

  MMPolicyOutOfProcess& operator=(MMPolicyOutOfProcess&& aOther) {
    Destroy();

    mProcess = aOther.mProcess;
    aOther.mProcess = nullptr;

    mMapping = aOther.mMapping;
    aOther.mMapping = nullptr;

    mLocalView = aOther.mLocalView;
    aOther.mLocalView = nullptr;

    mRemoteView = aOther.mRemoteView;
    aOther.mRemoteView = nullptr;

    mReservationSize = aOther.mReservationSize;
    aOther.mReservationSize = 0;

    mCommitOffset = aOther.mCommitOffset;
    aOther.mCommitOffset = 0;

    return *this;
  }

  explicit operator bool() const {
    return mProcess && mMapping && mLocalView && mRemoteView;
  }

  bool ShouldUnhookUponDestruction() const {
    // We don't clean up hooks for remote processes; they are expected to
    // outlive our process.
    return false;
  }

  // This function reads as many bytes as |aLen| from the target process and
  // succeeds only when the entire area to be read is accessible.
  bool Read(void* aToPtr, const void* aFromPtr, size_t aLen) const {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return false;
    }

    SIZE_T numBytes = 0;
    BOOL ok = ::ReadProcessMemory(mProcess, aFromPtr, aToPtr, aLen, &numBytes);
    return ok && numBytes == aLen;
  }

  // This function reads as many bytes as possible from the target process up
  // to |aLen| bytes and returns the number of bytes which was actually read.
  size_t TryRead(void* aToPtr, const void* aFromPtr, size_t aLen) const {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return 0;
    }

    uint32_t pageSize = GetPageSize();
    uintptr_t pageMask = pageSize - 1;

    auto rangeStart = reinterpret_cast<uintptr_t>(aFromPtr);
    auto rangeEnd = rangeStart + aLen;

    while (rangeStart < rangeEnd) {
      SIZE_T numBytes = 0;
      BOOL ok = ::ReadProcessMemory(mProcess, aFromPtr, aToPtr,
                                    rangeEnd - rangeStart, &numBytes);
      if (ok) {
        return numBytes;
      }

      // If ReadProcessMemory fails, try to read up to each page boundary from
      // the end of the requested area one by one.
      if (rangeEnd & pageMask) {
        rangeEnd &= ~pageMask;
      } else {
        rangeEnd -= pageSize;
      }
    }

    return 0;
  }

  bool Write(void* aToPtr, const void* aFromPtr, size_t aLen) const {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return false;
    }

    SIZE_T numBytes = 0;
    BOOL ok = ::WriteProcessMemory(mProcess, aToPtr, aFromPtr, aLen, &numBytes);
    return ok && numBytes == aLen;
  }

  bool Protect(void* aVAddress, size_t aSize, uint32_t aProtFlags,
               uint32_t* aPrevProtFlags) const {
    MOZ_ASSERT(mProcess);
    if (!mProcess) {
      return false;
    }

    MOZ_ASSERT(aPrevProtFlags);
    BOOL ok = ::VirtualProtectEx(mProcess, aVAddress, aSize, aProtFlags,
                                 reinterpret_cast<PDWORD>(aPrevProtFlags));
    if (!ok && aPrevProtFlags) {
      // VirtualProtectEx can fail but still set valid protection flags.
      // Let's clear those upon failure.
      *aPrevProtFlags = 0;
    }

    return !!ok;
  }

  /**
   * @return true if the page that hosts aVAddress is accessible.
   */
  bool IsPageAccessible(void* aVAddress) const {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = ::VirtualQueryEx(mProcess, aVAddress, &mbi, sizeof(mbi));

    return result && mbi.AllocationProtect && (mbi.Type & MEM_IMAGE) &&
           mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS;
  }

  bool FlushInstructionCache() const {
    return !!::FlushInstructionCache(mProcess, nullptr, 0);
  }

  static DWORD GetTrampWriteProtFlags() { return PAGE_READWRITE; }

#if defined(_M_X64)
  bool IsTrampolineSpaceInLowest2GB() const {
    return (GetRemoteView() + mReservationSize) <= 0x0000000080000000ULL;
  }
#endif  // defined(_M_X64)

 protected:
  uint8_t* GetLocalView() const { return mLocalView; }

  uintptr_t GetRemoteView() const {
    return reinterpret_cast<uintptr_t>(mRemoteView);
  }

  /**
   * @return the effective number of bytes reserved, or 0 on failure
   */
  uint32_t Reserve(const uint32_t aSize,
                   const Maybe<Span<const uint8_t>>& aBounds) {
    if (!aSize || !mProcess) {
      return 0;
    }

    if (mRemoteView) {
      MOZ_ASSERT(mReservationSize >= aSize);
      return mReservationSize;
    }

    mReservationSize = ComputeAllocationSize(aSize);

    mMapping = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr,
                                   PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0,
                                   mReservationSize, nullptr);
    if (!mMapping) {
      return 0;
    }

    mLocalView = static_cast<uint8_t*>(
        ::MapViewOfFile(mMapping, FILE_MAP_WRITE, 0, 0, 0));
    if (!mLocalView) {
      return 0;
    }

    auto reserveFn = [mapping = mMapping](HANDLE aProcess, PVOID aBase,
                                          uint32_t aSize) -> PVOID {
      return mozilla::MapRemoteViewOfFile(mapping, aProcess, 0ULL, aBase, 0, 0,
                                          PAGE_EXECUTE_READ);
    };

    auto reserveWithinRangeFn =
        [mapping = mMapping](HANDLE aProcess, uint32_t aSize,
                             const uint8_t* aRangeMin,
                             const uint8_t* aRangeMaxExcl) -> Maybe<PVOID> {
      static const StaticDynamicallyLinkedFunctionPtr<decltype(
          &::MapViewOfFile3)>
          pMapViewOfFile3(L"kernelbase.dll", "MapViewOfFile3");
      if (!pMapViewOfFile3) {
        return Nothing();
      }

      // NB: MEM_ADDRESS_REQUIREMENTS::HighestEndingAddress is *inclusive*
      MEM_ADDRESS_REQUIREMENTS memReq = {
          const_cast<uint8_t*>(aRangeMin),
          const_cast<uint8_t*>(aRangeMaxExcl - 1)};

      MEM_EXTENDED_PARAMETER memParam = {};
      memParam.Type = MemExtendedParameterAddressRequirements;
      memParam.Pointer = &memReq;

      return Some(pMapViewOfFile3(mapping, aProcess, nullptr, 0, aSize, 0,
                                  PAGE_EXECUTE_READ, &memParam, 1));
    };

    mRemoteView = MMPolicyBase::Reserve(mProcess, mReservationSize, reserveFn,
                                        reserveWithinRangeFn, aBounds);
    if (!mRemoteView) {
      return 0;
    }

    return mReservationSize;
  }

  bool MaybeCommitNextPage(const uint32_t aRequestedOffset,
                           const uint32_t aRequestedLength) {
    if (!(*this)) {
      return false;
    }

    uint32_t limit = aRequestedOffset + aRequestedLength - 1;
    if (limit < mCommitOffset) {
      // No commit required
      return true;
    }

    MOZ_DIAGNOSTIC_ASSERT(mCommitOffset < mReservationSize);
    if (mCommitOffset >= mReservationSize) {
      return false;
    }

    PVOID local = ::VirtualAlloc(mLocalView + mCommitOffset, GetPageSize(),
                                 MEM_COMMIT, PAGE_READWRITE);
    if (!local) {
      return false;
    }

    PVOID remote = ::VirtualAllocEx(
        mProcess, static_cast<uint8_t*>(mRemoteView) + mCommitOffset,
        GetPageSize(), MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!remote) {
      return false;
    }

    mCommitOffset += GetPageSize();
    return true;
  }

 private:
  void Destroy() {
    // We always leak the remote view
    if (mLocalView) {
      ::UnmapViewOfFile(mLocalView);
      mLocalView = nullptr;
    }

    if (mMapping) {
      ::CloseHandle(mMapping);
      mMapping = nullptr;
    }

    if (mProcess) {
      ::CloseHandle(mProcess);
      mProcess = nullptr;
    }
  }

 private:
  HANDLE mProcess;
  HANDLE mMapping;
  uint8_t* mLocalView;
  PVOID mRemoteView;
  uint32_t mReservationSize;
  uint32_t mCommitOffset;

  static const DWORD kAccessFlags = PROCESS_QUERY_INFORMATION |
                                    PROCESS_VM_OPERATION | PROCESS_VM_READ |
                                    PROCESS_VM_WRITE;
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_MMPolicies_h
