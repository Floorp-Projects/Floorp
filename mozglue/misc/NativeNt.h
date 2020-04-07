/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NativeNt_h
#define mozilla_NativeNt_h

#include <stdint.h>
#include <windows.h>
#include <winnt.h>
#include <winternl.h>

#include <algorithm>
#include <utility>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/Span.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/interceptor/MMPolicies.h"
#include "mozilla/interceptor/TargetFunction.h"

#if defined(MOZILLA_INTERNAL_API)
#  include "nsString.h"
#endif  // defined(MOZILLA_INTERNAL_API)

// The declarations within this #if block are intended to be used for initial
// process initialization ONLY. You probably don't want to be using these in
// normal Gecko code!
#if !defined(MOZILLA_INTERNAL_API)

extern "C" {

#  if !defined(STATUS_ACCESS_DENIED)
#    define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#  endif  // !defined(STATUS_ACCESS_DENIED)

#  if !defined(STATUS_DLL_NOT_FOUND)
#    define STATUS_DLL_NOT_FOUND ((NTSTATUS)0xC0000135L)
#  endif  // !defined(STATUS_DLL_NOT_FOUND)

#  if !defined(STATUS_UNSUCCESSFUL)
#    define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#  endif  // !defined(STATUS_UNSUCCESSFUL)

enum SECTION_INHERIT { ViewShare = 1, ViewUnmap = 2 };

NTSTATUS NTAPI NtMapViewOfSection(
    HANDLE aSection, HANDLE aProcess, PVOID* aBaseAddress, ULONG_PTR aZeroBits,
    SIZE_T aCommitSize, PLARGE_INTEGER aSectionOffset, PSIZE_T aViewSize,
    SECTION_INHERIT aInheritDisposition, ULONG aAllocationType,
    ULONG aProtectionFlags);

NTSTATUS NTAPI NtUnmapViewOfSection(HANDLE aProcess, PVOID aBaseAddress);

enum MEMORY_INFORMATION_CLASS {
  MemoryBasicInformation = 0,
  MemorySectionName = 2
};

// NB: When allocating, space for the buffer must also be included
typedef struct _MEMORY_SECTION_NAME {
  UNICODE_STRING mSectionFileName;
} MEMORY_SECTION_NAME, *PMEMORY_SECTION_NAME;

NTSTATUS NTAPI NtQueryVirtualMemory(HANDLE aProcess, PVOID aBaseAddress,
                                    MEMORY_INFORMATION_CLASS aMemInfoClass,
                                    PVOID aMemInfo, SIZE_T aMemInfoLen,
                                    PSIZE_T aReturnLen);

LONG NTAPI RtlCompareUnicodeString(PCUNICODE_STRING aStr1,
                                   PCUNICODE_STRING aStr2,
                                   BOOLEAN aCaseInsensitive);

BOOLEAN NTAPI RtlEqualUnicodeString(PCUNICODE_STRING aStr1,
                                    PCUNICODE_STRING aStr2,
                                    BOOLEAN aCaseInsensitive);

NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW aOutVersionInformation);

VOID NTAPI RtlAcquireSRWLockExclusive(PSRWLOCK aLock);
VOID NTAPI RtlAcquireSRWLockShared(PSRWLOCK aLock);

VOID NTAPI RtlReleaseSRWLockExclusive(PSRWLOCK aLock);
VOID NTAPI RtlReleaseSRWLockShared(PSRWLOCK aLock);

NTSTATUS NTAPI NtReadVirtualMemory(HANDLE aProcessHandle, PVOID aBaseAddress,
                                   PVOID aBuffer, SIZE_T aNumBytesToRead,
                                   PSIZE_T aNumBytesRead);

NTSTATUS NTAPI LdrLoadDll(PWCHAR aDllPath, PULONG aFlags,
                          PUNICODE_STRING aDllName, PHANDLE aOutHandle);

typedef ULONG(NTAPI* PRTL_RUN_ONCE_INIT_FN)(PRTL_RUN_ONCE, PVOID, PVOID*);
NTSTATUS NTAPI RtlRunOnceExecuteOnce(PRTL_RUN_ONCE aRunOnce,
                                     PRTL_RUN_ONCE_INIT_FN aInitFn,
                                     PVOID aContext, PVOID* aParameter);

}  // extern "C"

#endif  // !defined(MOZILLA_INTERNAL_API)

extern "C" {
PVOID NTAPI RtlAllocateHeap(PVOID aHeapHandle, ULONG aFlags, SIZE_T aSize);

PVOID NTAPI RtlReAllocateHeap(PVOID aHeapHandle, ULONG aFlags, LPVOID aMem,
                              SIZE_T aNewSize);

BOOLEAN NTAPI RtlFreeHeap(PVOID aHeapHandle, ULONG aFlags, PVOID aHeapBase);

BOOLEAN NTAPI RtlQueryPerformanceCounter(LARGE_INTEGER* aPerfCount);

#define RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE 1
#define RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING 2
NTSTATUS NTAPI RtlDuplicateUnicodeString(ULONG aFlags, PCUNICODE_STRING aSrc,
                                         PUNICODE_STRING aDest);

VOID NTAPI RtlFreeUnicodeString(PUNICODE_STRING aUnicodeString);
}  // extern "C"

namespace mozilla {
namespace nt {

/**
 * This class encapsulates a UNICODE_STRING that owns its own buffer. The
 * buffer is always NULL terminated, thus allowing us to cast to a wide C-string
 * without requiring any mutation.
 *
 * We only allow creation of this owned buffer from outside XUL.
 */
class AllocatedUnicodeString final {
 public:
  AllocatedUnicodeString() : mUnicodeString() {}

#if defined(MOZILLA_INTERNAL_API)
  AllocatedUnicodeString(const AllocatedUnicodeString& aOther) = delete;

  AllocatedUnicodeString& operator=(const AllocatedUnicodeString& aOther) =
      delete;
#else
  explicit AllocatedUnicodeString(PCUNICODE_STRING aSrc) {
    if (!aSrc) {
      mUnicodeString = {};
      return;
    }

    Duplicate(aSrc);
  }

  AllocatedUnicodeString(const AllocatedUnicodeString& aOther) {
    Duplicate(&aOther.mUnicodeString);
  }

  AllocatedUnicodeString& operator=(const AllocatedUnicodeString& aOther) {
    Clear();
    Duplicate(&aOther.mUnicodeString);
    return *this;
  }

  AllocatedUnicodeString& operator=(PCUNICODE_STRING aSrc) {
    Clear();
    Duplicate(aSrc);
    return *this;
  }
#endif  // defined(MOZILLA_INTERNAL_API)

  AllocatedUnicodeString(AllocatedUnicodeString&& aOther)
      : mUnicodeString(aOther.mUnicodeString) {
    aOther.mUnicodeString = {};
  }

  AllocatedUnicodeString& operator=(AllocatedUnicodeString&& aOther) {
    Clear();
    mUnicodeString = aOther.mUnicodeString;
    aOther.mUnicodeString = {};
    return *this;
  }

  ~AllocatedUnicodeString() { Clear(); }

  bool IsEmpty() const {
    return !mUnicodeString.Buffer || !mUnicodeString.Length;
  }

  operator PCUNICODE_STRING() const { return &mUnicodeString; }

  operator const WCHAR*() const { return mUnicodeString.Buffer; }

  USHORT CharLen() const { return mUnicodeString.Length / sizeof(WCHAR); }

#if defined(MOZILLA_INTERNAL_API)
  nsDependentString AsString() const {
    if (!mUnicodeString.Buffer) {
      return nsDependentString();
    }

    // We can use nsDependentString here as we guaranteed null termination
    // when we allocated the string.
    return nsDependentString(mUnicodeString.Buffer, CharLen());
  }
#endif  // defined(MOZILLA_INTERNAL_API)

 private:
#if !defined(MOZILLA_INTERNAL_API)
  void Duplicate(PCUNICODE_STRING aSrc) {
    MOZ_ASSERT(aSrc);

    // We duplicate with null termination so that this string may be used
    // as a wide C-string without any further manipulation.
    NTSTATUS ntStatus = ::RtlDuplicateUnicodeString(
        RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, aSrc, &mUnicodeString);
    MOZ_ASSERT(NT_SUCCESS(ntStatus));
    if (!NT_SUCCESS(ntStatus)) {
      // Make sure that mUnicodeString does not contain bogus data
      // (since not all callers zero it out before invoking)
      mUnicodeString = {};
    }
  }
#endif  // !defined(MOZILLA_INTERNAL_API)

  void Clear() {
    if (!mUnicodeString.Buffer) {
      return;
    }

    ::RtlFreeUnicodeString(&mUnicodeString);
    mUnicodeString = {};
  }

  UNICODE_STRING mUnicodeString;
};

#if !defined(MOZILLA_INTERNAL_API)

struct MemorySectionNameBuf : public _MEMORY_SECTION_NAME {
  MemorySectionNameBuf() {
    mSectionFileName.Length = 0;
    mSectionFileName.MaximumLength = sizeof(mBuf);
    mSectionFileName.Buffer = mBuf;
  }

  MemorySectionNameBuf(const MemorySectionNameBuf& aOther) { *this = aOther; }

  MemorySectionNameBuf(MemorySectionNameBuf&& aOther) {
    *this = std::move(aOther);
  }

  // We cannot use default copy here because mSectionFileName.Buffer needs to
  // be updated to point to |this->mBuf|, not |aOther.mBuf|.
  MemorySectionNameBuf& operator=(const MemorySectionNameBuf& aOther) {
    mSectionFileName.Length = aOther.mSectionFileName.Length;
    mSectionFileName.MaximumLength = sizeof(mBuf);
    MOZ_ASSERT(mSectionFileName.Length <= mSectionFileName.MaximumLength);
    mSectionFileName.Buffer = mBuf;
    memcpy(mBuf, aOther.mBuf, aOther.mSectionFileName.Length);
    return *this;
  }

  MemorySectionNameBuf& operator=(MemorySectionNameBuf&& aOther) {
    mSectionFileName.Length = aOther.mSectionFileName.Length;
    aOther.mSectionFileName.Length = 0;
    mSectionFileName.MaximumLength = sizeof(mBuf);
    MOZ_ASSERT(mSectionFileName.Length <= mSectionFileName.MaximumLength);
    aOther.mSectionFileName.MaximumLength = sizeof(aOther.mBuf);
    mSectionFileName.Buffer = mBuf;
    memmove(mBuf, aOther.mBuf, mSectionFileName.Length);
    return *this;
  }

  // Native NT paths, so we can't assume MAX_PATH. Use a larger buffer.
  WCHAR mBuf[2 * MAX_PATH];

  bool IsEmpty() const {
    return !mSectionFileName.Buffer || !mSectionFileName.Length;
  }

  operator PCUNICODE_STRING() const { return &mSectionFileName; }
};

inline bool FindCharInUnicodeString(const UNICODE_STRING& aStr, WCHAR aChar,
                                    uint16_t& aPos, uint16_t aStartIndex = 0) {
  const uint16_t aMaxIndex = aStr.Length / sizeof(WCHAR);

  for (uint16_t curIndex = aStartIndex; curIndex < aMaxIndex; ++curIndex) {
    if (aStr.Buffer[curIndex] == aChar) {
      aPos = curIndex;
      return true;
    }
  }

  return false;
}

inline bool IsHexDigit(WCHAR aChar) {
  return (aChar >= L'0' && aChar <= L'9') || (aChar >= L'A' && aChar <= L'F') ||
         (aChar >= L'a' && aChar <= L'f');
}

inline bool MatchUnicodeString(const UNICODE_STRING& aStr,
                               bool (*aPredicate)(WCHAR)) {
  WCHAR* cur = aStr.Buffer;
  WCHAR* end = &aStr.Buffer[aStr.Length / sizeof(WCHAR)];
  while (cur < end) {
    if (!aPredicate(*cur)) {
      return false;
    }

    ++cur;
  }

  return true;
}

inline bool Contains12DigitHexString(const UNICODE_STRING& aLeafName) {
  // Quick check: If the string is too short, don't bother
  // (We need at least 12 hex digits, one char for '.', and 3 for extension)
  const USHORT kMinLen = (12 + 1 + 3) * sizeof(wchar_t);
  if (aLeafName.Length < kMinLen) {
    return false;
  }

  uint16_t start, end;
  if (!FindCharInUnicodeString(aLeafName, L'.', start)) {
    return false;
  }

  ++start;
  if (!FindCharInUnicodeString(aLeafName, L'.', end, start)) {
    return false;
  }

  if (end - start != 12) {
    return false;
  }

  UNICODE_STRING test;
  test.Buffer = &aLeafName.Buffer[start];
  test.Length = (end - start) * sizeof(WCHAR);
  test.MaximumLength = test.Length;

  return MatchUnicodeString(test, &IsHexDigit);
}

inline bool IsFileNameAtLeast16HexDigits(const UNICODE_STRING& aLeafName) {
  // Quick check: If the string is too short, don't bother
  // (We need 16 hex digits, one char for '.', and 3 for extension)
  const USHORT kMinLen = (16 + 1 + 3) * sizeof(wchar_t);
  if (aLeafName.Length < kMinLen) {
    return false;
  }

  uint16_t dotIndex;
  if (!FindCharInUnicodeString(aLeafName, L'.', dotIndex)) {
    return false;
  }

  if (dotIndex < 16) {
    return false;
  }

  UNICODE_STRING test;
  test.Buffer = aLeafName.Buffer;
  test.Length = dotIndex * sizeof(WCHAR);
  test.MaximumLength = aLeafName.MaximumLength;

  return MatchUnicodeString(test, &IsHexDigit);
}

inline void GetLeafName(PUNICODE_STRING aDestString,
                        PCUNICODE_STRING aSrcString) {
  WCHAR* buf = aSrcString->Buffer;
  WCHAR* end = &aSrcString->Buffer[(aSrcString->Length / sizeof(WCHAR)) - 1];
  WCHAR* cur = end;
  while (cur >= buf) {
    if (*cur == L'\\') {
      break;
    }

    --cur;
  }

  // At this point, either cur points to the final backslash, or it points to
  // buf - 1. Either way, we're interested in cur + 1 as the desired buffer.
  aDestString->Buffer = cur + 1;
  aDestString->Length = (end - aDestString->Buffer + 1) * sizeof(WCHAR);
  aDestString->MaximumLength = aDestString->Length;
}

#endif  // !defined(MOZILLA_INTERNAL_API)

inline char EnsureLowerCaseASCII(char aChar) {
  if (aChar >= 'A' && aChar <= 'Z') {
    aChar -= 'A' - 'a';
  }

  return aChar;
}

inline int StricmpASCII(const char* aLeft, const char* aRight) {
  char curLeft, curRight;

  do {
    curLeft = EnsureLowerCaseASCII(*(aLeft++));
    curRight = EnsureLowerCaseASCII(*(aRight++));
  } while (curLeft && curLeft == curRight);

  return curLeft - curRight;
}

inline int StrcmpASCII(const char* aLeft, const char* aRight) {
  char curLeft, curRight;

  do {
    curLeft = *(aLeft++);
    curRight = *(aRight++);
  } while (curLeft && curLeft == curRight);

  return curLeft - curRight;
}

inline size_t StrlenASCII(const char* aStr) {
  size_t len = 0;

  while (*(aStr++)) {
    ++len;
  }

  return len;
}

class MOZ_RAII PEHeaders final {
  /**
   * This structure is documented on MSDN as VS_VERSIONINFO, but is not present
   * in SDK headers because it cannot be specified as a C struct. The following
   * structure contains the fixed-length fields at the beginning of
   * VS_VERSIONINFO.
   */
  struct VS_VERSIONINFO_HEADER {
    WORD wLength;
    WORD wValueLength;
    WORD wType;
    WCHAR szKey[16];  // ArrayLength(L"VS_VERSION_INFO")
    // Additional data goes here, aligned on a 4-byte boundary
  };

 public:
  // The lowest two bits of an HMODULE are used as flags. Stripping those bits
  // from the HMODULE yields the base address of the binary's memory mapping.
  // (See LoadLibraryEx docs on MSDN)
  template <typename T>
  static T HModuleToBaseAddr(HMODULE aModule) {
    return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(aModule) &
                               ~uintptr_t(3));
  }

  explicit PEHeaders(void* aBaseAddress)
      : PEHeaders(reinterpret_cast<PIMAGE_DOS_HEADER>(aBaseAddress)) {}

  explicit PEHeaders(HMODULE aModule)
      : PEHeaders(HModuleToBaseAddr<PIMAGE_DOS_HEADER>(aModule)) {}

  explicit PEHeaders(PIMAGE_DOS_HEADER aMzHeader)
      : mMzHeader(aMzHeader),
        mPeHeader(nullptr),
        mImageLimit(nullptr),
        mIsImportDirectoryTampered(false) {
    if (!mMzHeader || mMzHeader->e_magic != IMAGE_DOS_SIGNATURE) {
      return;
    }

    mPeHeader = RVAToPtrUnchecked<PIMAGE_NT_HEADERS>(mMzHeader->e_lfanew);
    if (!mPeHeader || mPeHeader->Signature != IMAGE_NT_SIGNATURE) {
      return;
    }

    if (mPeHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) {
      return;
    }

    DWORD imageSize = mPeHeader->OptionalHeader.SizeOfImage;
    // This is a coarse-grained check to ensure that the image size is
    // reasonable. It we aren't big enough to contain headers, we have a
    // problem!
    if (imageSize < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS)) {
      return;
    }

    mImageLimit = RVAToPtrUnchecked<void*>(imageSize - 1UL);
  }

  explicit operator bool() const { return !!mImageLimit; }

  /**
   * This overload computes absolute virtual addresses relative to the base
   * address of the binary.
   */
  template <typename T, typename R>
  T RVAToPtr(R aRva) const {
    return RVAToPtr<T>(mMzHeader, aRva);
  }

  /**
   * This overload computes a result by adding aRva to aBase, but also ensures
   * that the resulting pointer falls within the bounds of this binary's memory
   * mapping.
   */
  template <typename T, typename R>
  T RVAToPtr(void* aBase, R aRva) const {
    if (!mImageLimit) {
      return nullptr;
    }

    char* absAddress = reinterpret_cast<char*>(aBase) + aRva;
    if (absAddress < reinterpret_cast<char*>(mMzHeader) ||
        absAddress > reinterpret_cast<char*>(mImageLimit)) {
      return nullptr;
    }

    return reinterpret_cast<T>(absAddress);
  }

  Maybe<Range<const uint8_t>> GetBounds() const {
    if (!mImageLimit) {
      return Nothing();
    }

    auto base = reinterpret_cast<const uint8_t*>(mMzHeader);
    DWORD imageSize = mPeHeader->OptionalHeader.SizeOfImage;
    return Some(Range(base, imageSize));
  }

  PIMAGE_IMPORT_DESCRIPTOR GetImportDirectory() {
    // If the import directory is already tampered, we skip bounds check
    // because it could be located outside the mapped image.
    return mIsImportDirectoryTampered
               ? GetImageDirectoryEntry<PIMAGE_IMPORT_DESCRIPTOR,
                                        BoundsCheckPolicy::Skip>(
                     IMAGE_DIRECTORY_ENTRY_IMPORT)
               : GetImageDirectoryEntry<PIMAGE_IMPORT_DESCRIPTOR>(
                     IMAGE_DIRECTORY_ENTRY_IMPORT);
  }

  PIMAGE_RESOURCE_DIRECTORY GetResourceTable() {
    return GetImageDirectoryEntry<PIMAGE_RESOURCE_DIRECTORY>(
        IMAGE_DIRECTORY_ENTRY_RESOURCE);
  }

  PIMAGE_DATA_DIRECTORY GetImageDirectoryEntryPtr(
      const uint32_t aDirectoryIndex, uint32_t* aOutRva = nullptr) const {
    if (aOutRva) {
      *aOutRva = 0;
    }

    IMAGE_OPTIONAL_HEADER& optionalHeader = mPeHeader->OptionalHeader;

    const uint32_t maxIndex = std::min(optionalHeader.NumberOfRvaAndSizes,
                                       DWORD(IMAGE_NUMBEROF_DIRECTORY_ENTRIES));
    if (aDirectoryIndex >= maxIndex) {
      return nullptr;
    }

    PIMAGE_DATA_DIRECTORY dirEntry =
        &optionalHeader.DataDirectory[aDirectoryIndex];
    if (aOutRva) {
      *aOutRva = reinterpret_cast<char*>(dirEntry) -
                 reinterpret_cast<char*>(mMzHeader);
      MOZ_ASSERT(*aOutRva);
    }

    return dirEntry;
  }

  bool GetVersionInfo(uint64_t& aOutVersion) {
    // RT_VERSION == 16
    // Version resources require an id of 1
    auto root = FindResourceLeaf<VS_VERSIONINFO_HEADER*>(16, 1);
    if (!root) {
      return false;
    }

    VS_FIXEDFILEINFO* fixedInfo = GetFixedFileInfo(root);
    if (!fixedInfo) {
      return false;
    }

    aOutVersion = ((static_cast<uint64_t>(fixedInfo->dwFileVersionMS) << 32) |
                   static_cast<uint64_t>(fixedInfo->dwFileVersionLS));
    return true;
  }

  bool GetTimeStamp(DWORD& aResult) {
    if (!(*this)) {
      return false;
    }

    aResult = mPeHeader->FileHeader.TimeDateStamp;
    return true;
  }

  PIMAGE_IMPORT_DESCRIPTOR
  GetImportDescriptor(const char* aModuleNameASCII) {
    for (PIMAGE_IMPORT_DESCRIPTOR curImpDesc = GetImportDirectory();
         IsValid(curImpDesc); ++curImpDesc) {
      auto curName = mIsImportDirectoryTampered
                         ? RVAToPtrUnchecked<const char*>(curImpDesc->Name)
                         : RVAToPtr<const char*>(curImpDesc->Name);
      if (!curName) {
        return nullptr;
      }

      if (StricmpASCII(aModuleNameASCII, curName)) {
        continue;
      }

      // curImpDesc now points to the IAT for the module we're interested in
      return curImpDesc;
    }

    return nullptr;
  }

  /**
   * If |aBoundaries| is given, this method checks whether each IAT entry is
   * within the given range, and if any entry is out of the range, we return
   * Nothing().
   */
  Maybe<Span<IMAGE_THUNK_DATA>> GetIATThunksForModule(
      const char* aModuleNameASCII,
      const Range<const uint8_t>* aBoundaries = nullptr) {
    PIMAGE_IMPORT_DESCRIPTOR impDesc = GetImportDescriptor(aModuleNameASCII);
    if (!impDesc) {
      return Nothing();
    }

    auto firstIatThunk =
        this->template RVAToPtr<PIMAGE_THUNK_DATA>(impDesc->FirstThunk);
    if (!firstIatThunk) {
      return Nothing();
    }

    // Find the length by iterating through the table until we find a null entry
    PIMAGE_THUNK_DATA curIatThunk = firstIatThunk;
    while (IsValid(curIatThunk)) {
      if (aBoundaries) {
        auto iatEntry =
            reinterpret_cast<const uint8_t*>(curIatThunk->u1.Function);
        if (iatEntry < aBoundaries->begin().get() ||
            iatEntry >= aBoundaries->end().get()) {
          return Nothing();
        }
      }

      ++curIatThunk;
    }

    return Some(MakeSpan(firstIatThunk, curIatThunk));
  }

  /**
   * Resources are stored in a three-level tree. To locate a particular entry,
   * you must supply a resource type, the resource id, and then the language id.
   * If aLangId == 0, we just resolve the first entry regardless of language.
   */
  template <typename T>
  T FindResourceLeaf(WORD aType, WORD aResId, WORD aLangId = 0) {
    PIMAGE_RESOURCE_DIRECTORY topLevel = GetResourceTable();
    if (!topLevel) {
      return nullptr;
    }

    PIMAGE_RESOURCE_DIRECTORY_ENTRY typeEntry =
        FindResourceEntry(topLevel, aType);
    if (!typeEntry || !typeEntry->DataIsDirectory) {
      return nullptr;
    }

    auto idDir = RVAToPtr<PIMAGE_RESOURCE_DIRECTORY>(
        topLevel, typeEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY idEntry = FindResourceEntry(idDir, aResId);
    if (!idEntry || !idEntry->DataIsDirectory) {
      return nullptr;
    }

    auto langDir = RVAToPtr<PIMAGE_RESOURCE_DIRECTORY>(
        topLevel, idEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY langEntry;
    if (aLangId) {
      langEntry = FindResourceEntry(langDir, aLangId);
    } else {
      langEntry = FindFirstResourceEntry(langDir);
    }

    if (!langEntry || langEntry->DataIsDirectory) {
      return nullptr;
    }

    auto dataEntry =
        RVAToPtr<PIMAGE_RESOURCE_DATA_ENTRY>(topLevel, langEntry->OffsetToData);
    return RVAToPtr<T>(dataEntry->OffsetToData);
  }

  template <size_t N>
  Maybe<Span<const uint8_t>> FindSection(const char (&aSecName)[N],
                                         DWORD aCharacteristicsMask) const {
    static_assert((N - 1) <= IMAGE_SIZEOF_SHORT_NAME,
                  "Section names must be at most 8 characters excluding null "
                  "terminator");

    if (!(*this)) {
      return Nothing();
    }

    Span<IMAGE_SECTION_HEADER> sectionTable = GetSectionTable();
    for (auto&& sectionHeader : sectionTable) {
      if (strncmp(reinterpret_cast<const char*>(sectionHeader.Name), aSecName,
                  IMAGE_SIZEOF_SHORT_NAME)) {
        continue;
      }

      if (!(sectionHeader.Characteristics & aCharacteristicsMask)) {
        // We found the section but it does not have the expected
        // characteristics
        return Nothing();
      }

      DWORD rva = sectionHeader.VirtualAddress;
      if (!rva) {
        return Nothing();
      }

      DWORD size = sectionHeader.Misc.VirtualSize;
      if (!size) {
        return Nothing();
      }

      auto base = RVAToPtr<const uint8_t*>(rva);
      return Some(MakeSpan(base, size));
    }

    return Nothing();
  }

  // There may be other code sections in the binary besides .text
  Maybe<Span<const uint8_t>> GetTextSectionInfo() const {
    return FindSection(".text", IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE |
                                    IMAGE_SCN_MEM_READ);
  }

  static bool IsValid(PIMAGE_IMPORT_DESCRIPTOR aImpDesc) {
    return aImpDesc && aImpDesc->OriginalFirstThunk != 0;
  }

  static bool IsValid(PIMAGE_THUNK_DATA aImgThunk) {
    return aImgThunk && aImgThunk->u1.Ordinal != 0;
  }

  void SetImportDirectoryTampered() { mIsImportDirectoryTampered = true; }

 private:
  enum class BoundsCheckPolicy { Default, Skip };

  template <typename T, BoundsCheckPolicy Policy = BoundsCheckPolicy::Default>
  T GetImageDirectoryEntry(const uint32_t aDirectoryIndex) {
    PIMAGE_DATA_DIRECTORY dirEntry = GetImageDirectoryEntryPtr(aDirectoryIndex);
    if (!dirEntry) {
      return nullptr;
    }

    return Policy == BoundsCheckPolicy::Skip
               ? RVAToPtrUnchecked<T>(dirEntry->VirtualAddress)
               : RVAToPtr<T>(dirEntry->VirtualAddress);
  }

  // This private variant does not have bounds checks, because we need to be
  // able to resolve the bounds themselves.
  template <typename T, typename R>
  T RVAToPtrUnchecked(R aRva) const {
    return reinterpret_cast<T>(reinterpret_cast<char*>(mMzHeader) + aRva);
  }

  Span<IMAGE_SECTION_HEADER> GetSectionTable() const {
    MOZ_ASSERT(*this);
    auto base = RVAToPtr<PIMAGE_SECTION_HEADER>(
        &mPeHeader->OptionalHeader, mPeHeader->FileHeader.SizeOfOptionalHeader);
    // The Windows loader has an internal limit of 96 sections (per PE spec)
    auto numSections =
        std::min(mPeHeader->FileHeader.NumberOfSections, WORD(96));
    return MakeSpan(base, numSections);
  }

  PIMAGE_RESOURCE_DIRECTORY_ENTRY
  FindResourceEntry(PIMAGE_RESOURCE_DIRECTORY aCurLevel, WORD aId) {
    // Immediately after the IMAGE_RESOURCE_DIRECTORY structure is an array
    // of IMAGE_RESOURCE_DIRECTORY_ENTRY structures. Since this function
    // searches by ID, we need to skip past any named entries before iterating.
    auto dirEnt =
        reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(aCurLevel + 1) +
        aCurLevel->NumberOfNamedEntries;
    for (WORD i = 0; i < aCurLevel->NumberOfIdEntries; ++i) {
      if (dirEnt[i].Id == aId) {
        return &dirEnt[i];
      }
    }

    return nullptr;
  }

  PIMAGE_RESOURCE_DIRECTORY_ENTRY
  FindFirstResourceEntry(PIMAGE_RESOURCE_DIRECTORY aCurLevel) {
    // Immediately after the IMAGE_RESOURCE_DIRECTORY structure is an array
    // of IMAGE_RESOURCE_DIRECTORY_ENTRY structures. We just return the first
    // entry, regardless of whether it is indexed by name or by id.
    auto dirEnt =
        reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(aCurLevel + 1);
    WORD numEntries =
        aCurLevel->NumberOfNamedEntries + aCurLevel->NumberOfIdEntries;
    if (!numEntries) {
      return nullptr;
    }

    return dirEnt;
  }

  VS_FIXEDFILEINFO* GetFixedFileInfo(VS_VERSIONINFO_HEADER* aVerInfo) {
    WORD length = aVerInfo->wLength;
    if (length < sizeof(VS_VERSIONINFO_HEADER)) {
      return nullptr;
    }

    const wchar_t kVersionInfoKey[] = L"VS_VERSION_INFO";
    if (::RtlCompareMemory(aVerInfo->szKey, kVersionInfoKey,
                           ArrayLength(kVersionInfoKey)) !=
        ArrayLength(kVersionInfoKey)) {
      return nullptr;
    }

    if (aVerInfo->wValueLength != sizeof(VS_FIXEDFILEINFO)) {
      // Fixed file info does not exist
      return nullptr;
    }

    WORD offset = sizeof(VS_VERSIONINFO_HEADER);

    uintptr_t base = reinterpret_cast<uintptr_t>(aVerInfo);
    // Align up to 4-byte boundary
#pragma warning(suppress : 4146)
    offset += (-(base + offset) & 3);

    if (offset >= length) {
      return nullptr;
    }

    auto result = reinterpret_cast<VS_FIXEDFILEINFO*>(base + offset);
    if (result->dwSignature != 0xFEEF04BD) {
      return nullptr;
    }

    return result;
  }

 private:
  PIMAGE_DOS_HEADER mMzHeader;
  PIMAGE_NT_HEADERS mPeHeader;
  void* mImageLimit;
  bool mIsImportDirectoryTampered;
};

// This class represents an export section of a local/remote process.
template <typename MMPolicy>
class MOZ_RAII PEExportSection {
  const MMPolicy& mMMPolicy;
  uintptr_t mImageBase;
  DWORD mOrdinalBase;
  DWORD mRvaDirStart;
  DWORD mRvaDirEnd;
  mozilla::interceptor::TargetObjectArray<MMPolicy, DWORD> mExportAddressTable;
  mozilla::interceptor::TargetObjectArray<MMPolicy, DWORD> mExportNameTable;
  mozilla::interceptor::TargetObjectArray<MMPolicy, WORD> mExportOrdinalTable;

  explicit PEExportSection(const MMPolicy& aMMPolicy)
      : mMMPolicy(aMMPolicy),
        mImageBase(0),
        mOrdinalBase(0),
        mRvaDirStart(0),
        mRvaDirEnd(0),
        mExportAddressTable(mMMPolicy),
        mExportNameTable(mMMPolicy),
        mExportOrdinalTable(mMMPolicy) {}

  PEExportSection(const MMPolicy& aMMPolicy, uintptr_t aImageBase,
                  DWORD aRvaDirStart, DWORD aRvaDirEnd,
                  const IMAGE_EXPORT_DIRECTORY& exportDir)
      : mMMPolicy(aMMPolicy),
        mImageBase(aImageBase),
        mOrdinalBase(exportDir.Base),
        mRvaDirStart(aRvaDirStart),
        mRvaDirEnd(aRvaDirEnd),
        mExportAddressTable(mMMPolicy,
                            mImageBase + exportDir.AddressOfFunctions,
                            exportDir.NumberOfFunctions),
        mExportNameTable(mMMPolicy, mImageBase + exportDir.AddressOfNames,
                         exportDir.NumberOfNames),
        mExportOrdinalTable(mMMPolicy,
                            mImageBase + exportDir.AddressOfNameOrdinals,
                            exportDir.NumberOfNames) {}

  static const PEExportSection Get(uintptr_t aImageBase,
                                   const MMPolicy& aMMPolicy) {
    mozilla::interceptor::TargetObject<MMPolicy, IMAGE_DOS_HEADER> mzHeader(
        aMMPolicy, aImageBase);
    if (!mzHeader || mzHeader->e_magic != IMAGE_DOS_SIGNATURE) {
      return PEExportSection(aMMPolicy);
    }

    mozilla::interceptor::TargetObject<MMPolicy, IMAGE_NT_HEADERS> peHeader(
        aMMPolicy, aImageBase + mzHeader->e_lfanew);
    if (!peHeader || peHeader->Signature != IMAGE_NT_SIGNATURE) {
      return PEExportSection(aMMPolicy);
    }

    if (peHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) {
      return PEExportSection(aMMPolicy);
    }

    const IMAGE_OPTIONAL_HEADER& optionalHeader = peHeader->OptionalHeader;

    DWORD imageSize = optionalHeader.SizeOfImage;
    // This is a coarse-grained check to ensure that the image size is
    // reasonable. It we aren't big enough to contain headers, we have a
    // problem!
    if (imageSize < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS)) {
      return PEExportSection(aMMPolicy);
    }

    if (optionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
      return PEExportSection(aMMPolicy);
    }

    const IMAGE_DATA_DIRECTORY& exportDirectoryEntry =
        optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!exportDirectoryEntry.VirtualAddress || !exportDirectoryEntry.Size) {
      return PEExportSection(aMMPolicy);
    }

    mozilla::interceptor::TargetObject<MMPolicy, IMAGE_EXPORT_DIRECTORY>
        exportDirectory(aMMPolicy,
                        aImageBase + exportDirectoryEntry.VirtualAddress);
    if (!exportDirectory || !exportDirectory->NumberOfFunctions) {
      return PEExportSection(aMMPolicy);
    }

    return PEExportSection(
        aMMPolicy, aImageBase, exportDirectoryEntry.VirtualAddress,
        exportDirectoryEntry.VirtualAddress + exportDirectoryEntry.Size,
        *exportDirectory.GetLocalBase());
  }

  FARPROC GetProcAddressByOrdinal(WORD aOrdinal) const {
    if (aOrdinal < mOrdinalBase) {
      return nullptr;
    }

    auto rvaToFunction = mExportAddressTable[aOrdinal - mOrdinalBase];
    if (!rvaToFunction) {
      return nullptr;
    }
    return reinterpret_cast<FARPROC>(mImageBase + *rvaToFunction);
  }

 public:
  static const PEExportSection Get(HMODULE aModule, const MMPolicy& aMMPolicy) {
    return Get(PEHeaders::HModuleToBaseAddr<uintptr_t>(aModule), aMMPolicy);
  }

  explicit operator bool() const {
    // Because PEExportSection doesn't use MMPolicy::Reserve(), a boolified
    // mMMPolicy is expected to be false.  We don't check mMMPolicy here.
    return mImageBase && mRvaDirStart && mRvaDirEnd && mExportAddressTable &&
           mExportNameTable && mExportOrdinalTable;
  }

  template <typename T>
  T RVAToPtr(uint32_t aRva) const {
    return reinterpret_cast<T>(mImageBase + aRva);
  }

  PIMAGE_EXPORT_DIRECTORY GetExportDirectory() const {
    if (!*this) {
      return nullptr;
    }

    return RVAToPtr<PIMAGE_EXPORT_DIRECTORY>(mRvaDirStart);
  }

  /**
   * This functions searches the export table for a given string as
   * GetProcAddress does, but this returns a matched entry of the Export
   * Address Table i.e. a pointer to an RVA of a matched function instead
   * of a function address.  If the entry is forwarded, this function
   * returns nullptr.
   */
  const DWORD* FindExportAddressTableEntry(
      const char* aFunctionNameASCII) const {
    if (!*this || !aFunctionNameASCII) {
      return nullptr;
    }

    struct NameTableComparator {
      NameTableComparator(const PEExportSection<MMPolicy>& aExportSection,
                          const char* aTarget)
          : mExportSection(aExportSection),
            mTargetName(aTarget),
            mTargetNamelength(StrlenASCII(aTarget)) {}

      int operator()(DWORD aRVAToString) const {
        mozilla::interceptor::TargetObjectArray<MMPolicy, char> itemString(
            mExportSection.mMMPolicy, mExportSection.mImageBase + aRVAToString,
            mTargetNamelength + 1);
        return StrcmpASCII(mTargetName, itemString[0]);
      }

      const PEExportSection<MMPolicy>& mExportSection;
      const char* mTargetName;
      size_t mTargetNamelength;
    };

    const NameTableComparator comp(*this, aFunctionNameASCII);

    size_t match;
    if (!mExportNameTable.BinarySearchIf(comp, &match)) {
      return nullptr;
    }

    const WORD* index = mExportOrdinalTable[match];
    if (!index) {
      return nullptr;
    }

    const DWORD* rvaToFunction = mExportAddressTable[*index];
    if (!rvaToFunction) {
      return nullptr;
    }

    if (*rvaToFunction >= mRvaDirStart && *rvaToFunction < mRvaDirEnd) {
      // If an entry points to an address within the export section, the
      // field is a forwarder RVA.  We return nullptr because the entry is
      // not a function address but a null-terminated string used for export
      // forwarding.
      return nullptr;
    }

    return rvaToFunction;
  }

  /**
   * This functions behaves the same as the native ::GetProcAddress except
   * the following cases:
   * - Returns nullptr if a target entry is forwarded to another dll.
   */
  FARPROC GetProcAddress(const char* aFunctionNameASCII) const {
    uintptr_t maybeOdrinal = reinterpret_cast<uintptr_t>(aFunctionNameASCII);
    // When the high-order word of |aFunctionNameASCII| is zero, it's not
    // a string but an ordinal value.
    if (maybeOdrinal < 0x10000) {
      return GetProcAddressByOrdinal(static_cast<WORD>(maybeOdrinal));
    }

    auto rvaToFunction = FindExportAddressTableEntry(aFunctionNameASCII);
    if (!rvaToFunction) {
      return nullptr;
    }
    return reinterpret_cast<FARPROC>(mImageBase + *rvaToFunction);
  }
};

inline HANDLE RtlGetProcessHeap() {
  PTEB teb = ::NtCurrentTeb();
  PPEB peb = teb->ProcessEnvironmentBlock;
  return peb->Reserved4[1];
}

inline PVOID RtlGetThreadLocalStoragePointer() {
  return ::NtCurrentTeb()->Reserved1[11];
}

inline void RtlSetThreadLocalStoragePointerForTestingOnly(PVOID aNewValue) {
  ::NtCurrentTeb()->Reserved1[11] = aNewValue;
}

inline DWORD RtlGetCurrentThreadId() {
  PTEB teb = ::NtCurrentTeb();
  CLIENT_ID* cid = reinterpret_cast<CLIENT_ID*>(&teb->Reserved1[8]);
  return static_cast<DWORD>(reinterpret_cast<uintptr_t>(cid->UniqueThread) &
                            0xFFFFFFFFUL);
}

inline LauncherResult<DWORD> GetParentProcessId() {
  struct PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PPEB PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
  };

  const HANDLE kCurrentProcess = reinterpret_cast<HANDLE>(-1);
  ULONG returnLength;
  PROCESS_BASIC_INFORMATION pbi = {};
  NTSTATUS status =
      ::NtQueryInformationProcess(kCurrentProcess, ProcessBasicInformation,
                                  &pbi, sizeof(pbi), &returnLength);
  if (!NT_SUCCESS(status)) {
    return LAUNCHER_ERROR_FROM_NTSTATUS(status);
  }

  return static_cast<DWORD>(pbi.InheritedFromUniqueProcessId & 0xFFFFFFFF);
}

struct DataDirectoryEntry : public _IMAGE_DATA_DIRECTORY {
  DataDirectoryEntry() : _IMAGE_DATA_DIRECTORY() {}

  MOZ_IMPLICIT DataDirectoryEntry(const _IMAGE_DATA_DIRECTORY& aOther)
      : _IMAGE_DATA_DIRECTORY(aOther) {}

  DataDirectoryEntry(const DataDirectoryEntry& aOther) = default;

  bool operator==(const DataDirectoryEntry& aOther) const {
    return VirtualAddress == aOther.VirtualAddress && Size == aOther.Size;
  }

  bool operator!=(const DataDirectoryEntry& aOther) const {
    return !(*this == aOther);
  }
};

inline LauncherResult<void*> GetProcessPebPtr(HANDLE aProcess) {
  ULONG returnLength;
  PROCESS_BASIC_INFORMATION pbi;
  NTSTATUS status = ::NtQueryInformationProcess(
      aProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);
  if (!NT_SUCCESS(status)) {
    return LAUNCHER_ERROR_FROM_NTSTATUS(status);
  }

  return pbi.PebBaseAddress;
}

/**
 * This function relies on a specific offset into the mostly-undocumented PEB
 * structure. The risk is reduced thanks to the fact that the Chromium sandbox
 * relies on the location of this field. It is unlikely to change at this point.
 * To further reduce the risk, we also check for the magic 'MZ' signature that
 * should indicate the beginning of a PE image.
 */
inline LauncherResult<HMODULE> GetProcessExeModule(HANDLE aProcess) {
  LauncherResult<void*> ppeb = GetProcessPebPtr(aProcess);
  if (ppeb.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(ppeb);
  }

  PEB peb;
  SIZE_T bytesRead;

#if defined(MOZILLA_INTERNAL_API)
  if (!::ReadProcessMemory(aProcess, ppeb.unwrap(), &peb, sizeof(peb),
                           &bytesRead) ||
      bytesRead != sizeof(peb)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }
#else
  NTSTATUS ntStatus = ::NtReadVirtualMemory(aProcess, ppeb.unwrap(), &peb,
                                            sizeof(peb), &bytesRead);
  if (!NT_SUCCESS(ntStatus) || bytesRead != sizeof(peb)) {
    return LAUNCHER_ERROR_FROM_NTSTATUS(ntStatus);
  }
#endif

  // peb.ImageBaseAddress
  void* baseAddress = peb.Reserved3[1];

  char mzMagic[2];
#if defined(MOZILLA_INTERNAL_API)
  if (!::ReadProcessMemory(aProcess, baseAddress, mzMagic, sizeof(mzMagic),
                           &bytesRead) ||
      bytesRead != sizeof(mzMagic)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }
#else
  ntStatus = ::NtReadVirtualMemory(aProcess, baseAddress, mzMagic,
                                   sizeof(mzMagic), &bytesRead);
  if (!NT_SUCCESS(ntStatus) || bytesRead != sizeof(mzMagic)) {
    return LAUNCHER_ERROR_FROM_NTSTATUS(ntStatus);
  }
#endif

  MOZ_ASSERT(mzMagic[0] == 'M' && mzMagic[1] == 'Z');
  if (mzMagic[0] != 'M' || mzMagic[1] != 'Z') {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  }

  return static_cast<HMODULE>(baseAddress);
}

#if !defined(MOZILLA_INTERNAL_API)

class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS SRWLock final {
 public:
  constexpr SRWLock() : mLock(SRWLOCK_INIT) {}

  void LockShared() { ::RtlAcquireSRWLockShared(&mLock); }

  void LockExclusive() { ::RtlAcquireSRWLockExclusive(&mLock); }

  void UnlockShared() { ::RtlReleaseSRWLockShared(&mLock); }

  void UnlockExclusive() { ::RtlReleaseSRWLockExclusive(&mLock); }

  SRWLock(const SRWLock&) = delete;
  SRWLock(SRWLock&&) = delete;
  SRWLock& operator=(const SRWLock&) = delete;
  SRWLock& operator=(SRWLock&&) = delete;

  SRWLOCK* operator&() { return &mLock; }

 private:
  SRWLOCK mLock;
};

class MOZ_RAII AutoExclusiveLock final {
 public:
  explicit AutoExclusiveLock(SRWLock& aLock) : mLock(aLock) {
    aLock.LockExclusive();
  }

  ~AutoExclusiveLock() { mLock.UnlockExclusive(); }

  AutoExclusiveLock(const AutoExclusiveLock&) = delete;
  AutoExclusiveLock(AutoExclusiveLock&&) = delete;
  AutoExclusiveLock& operator=(const AutoExclusiveLock&) = delete;
  AutoExclusiveLock& operator=(AutoExclusiveLock&&) = delete;

 private:
  SRWLock& mLock;
};

class MOZ_RAII AutoSharedLock final {
 public:
  explicit AutoSharedLock(SRWLock& aLock) : mLock(aLock) { aLock.LockShared(); }

  ~AutoSharedLock() { mLock.UnlockShared(); }

  AutoSharedLock(const AutoSharedLock&) = delete;
  AutoSharedLock(AutoSharedLock&&) = delete;
  AutoSharedLock& operator=(const AutoSharedLock&) = delete;
  AutoSharedLock& operator=(AutoSharedLock&&) = delete;

 private:
  SRWLock& mLock;
};

#endif  // !defined(MOZILLA_INTERNAL_API)

class RtlAllocPolicy {
 public:
  template <typename T>
  T* maybe_pod_malloc(size_t aNumElems) {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      return nullptr;
    }

    return static_cast<T*>(
        ::RtlAllocateHeap(RtlGetProcessHeap(), 0, aNumElems * sizeof(T)));
  }

  template <typename T>
  T* maybe_pod_calloc(size_t aNumElems) {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      return nullptr;
    }

    return static_cast<T*>(::RtlAllocateHeap(
        RtlGetProcessHeap(), HEAP_ZERO_MEMORY, aNumElems * sizeof(T)));
  }

  template <typename T>
  T* maybe_pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    if (aNewSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      return nullptr;
    }

    return static_cast<T*>(::RtlReAllocateHeap(RtlGetProcessHeap(), 0, aPtr,
                                               aNewSize * sizeof(T)));
  }

  template <typename T>
  T* pod_malloc(size_t aNumElems) {
    return maybe_pod_malloc<T>(aNumElems);
  }

  template <typename T>
  T* pod_calloc(size_t aNumElems) {
    return maybe_pod_calloc<T>(aNumElems);
  }

  template <typename T>
  T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    return maybe_pod_realloc<T>(aPtr, aOldSize, aNewSize);
  }

  template <typename T>
  void free_(T* aPtr, size_t aNumElems = 0) {
    ::RtlFreeHeap(RtlGetProcessHeap(), 0, aPtr);
  }

  void reportAllocOverflow() const {}

  MOZ_MUST_USE bool checkSimulatedOOM() const { return true; }
};

}  // namespace nt
}  // namespace mozilla

#endif  // mozilla_NativeNt_h
