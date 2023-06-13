/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DynamicBlocklist_h
#define mozilla_DynamicBlocklist_h

#include <winternl.h>

#include "nsWindowsHelpers.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/NativeNt.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"

#if defined(MOZILLA_INTERNAL_API)
#  include "mozilla/dom/Promise.h"
#  include "nsIOutputStream.h"
#  include "nsTHashSet.h"
#endif  // defined(MOZILLA_INTERNAL_API)

#include "mozilla/WindowsDllBlocklistInfo.h"

#if !defined(MOZILLA_INTERNAL_API) && defined(ENABLE_TESTS)
#  include "mozilla/CmdLineAndEnvUtils.h"
#  define BLOCKLIST_INSERT_TEST_ENTRY
#endif  // !defined(MOZILLA_INTERNAL_API) && defined(ENABLE_TESTS)

namespace mozilla {
using DllBlockInfo = DllBlockInfoT<UNICODE_STRING>;

struct DynamicBlockListBase {
  static constexpr uint32_t kSignature = 'FFBL';  // Firefox Blocklist
  static constexpr uint32_t kCurrentVersion = 1;

  struct FileHeader {
    uint32_t mSignature;
    uint32_t mFileVersion;
    uint32_t mPayloadSize;
  };
};
// Define this class in a header so that TestCrossProcessWin
// can include and test it.
class DynamicBlockList final : public DynamicBlockListBase {
  uint32_t mPayloadSize;
  UniquePtr<uint8_t[]> mPayload;

#ifdef ENABLE_TESTS
  // These two definitions are needed for the DynamicBlocklistWriter to avoid
  // writing this test entry out to the blocklist file, so compile these in
  // even if MOZILLA_INTERNAL_API is defined.
 public:
  static constexpr wchar_t kTestDll[] = L"TestDllBlocklist_UserBlocked.dll";
  // kTestDll is null-terminated, but we don't want that for the blocklist
  // file
  static constexpr size_t kTestDllBytes = sizeof(kTestDll) - sizeof(wchar_t);

 private:
#  ifdef BLOCKLIST_INSERT_TEST_ENTRY
  // Set up a test entry in the blocklist, used for testing purposes.
  void AssignTestEntry(DllBlockInfo* testEntry, uint32_t nameOffset) {
    testEntry->mName.Length = kTestDllBytes;
    testEntry->mName.MaximumLength = kTestDllBytes;
    testEntry->mName.Buffer = reinterpret_cast<PWSTR>(nameOffset);
    testEntry->mMaxVersion = DllBlockInfo::ALL_VERSIONS;
    testEntry->mFlags = DllBlockInfoFlags::FLAGS_DEFAULT;
  }

  void CreateListWithTestEntry() {
    mPayloadSize = sizeof(DllBlockInfo) * 2 + kTestDllBytes;
    mPayload = MakeUnique<uint8_t[]>(mPayloadSize);
    DllBlockInfo* entry = reinterpret_cast<DllBlockInfo*>(mPayload.get());
    AssignTestEntry(entry, sizeof(DllBlockInfo) * 2);
    memcpy(mPayload.get() + sizeof(DllBlockInfo) * 2, kTestDll, kTestDllBytes);
    ++entry;
    entry->mName.Length = 0;
    entry->mName.MaximumLength = 0;
  }
#  endif  // BLOCKLIST_INSERT_TEST_ENTRY
#endif    // ENABLE_TESTS

  bool LoadFile(const wchar_t* aPath) {
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
    const bool shouldInsertBlocklistTestEntry =
        MOZ_UNLIKELY(mozilla::EnvHasValue("MOZ_DISABLE_NONLOCAL_CONNECTIONS") ||
                     mozilla::EnvHasValue("MOZ_RUN_GTEST"));
#endif

    nsAutoHandle file(
        ::CreateFileW(aPath, GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE,
                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
      if (shouldInsertBlocklistTestEntry) {
        // If the blocklist file doesn't exist, we still want to include a test
        // entry for testing purposes.
        CreateListWithTestEntry();
        return true;
      }
#endif
      return false;
    }

    DWORD bytesRead = 0;
    FileHeader header;
    BOOL ok =
        ::ReadFile(file.get(), &header, sizeof(header), &bytesRead, nullptr);
    if (!ok) {
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
      if (shouldInsertBlocklistTestEntry) {
        // If we have a problem reading the blocklist file, we still want to
        // include a test entry for testing purposes.
        CreateListWithTestEntry();
        return true;
      }
#endif
      return false;
    }
    if (bytesRead != sizeof(header)) {
      return false;
    }

    if (header.mSignature != kSignature ||
        header.mFileVersion != kCurrentVersion) {
      return false;
    }

    uint32_t destinationPayloadSize = header.mPayloadSize;
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
    if (shouldInsertBlocklistTestEntry) {
      // Write an extra entry for testing purposes
      destinationPayloadSize += sizeof(DllBlockInfo) + kTestDllBytes;
    }
#endif
    UniquePtr<uint8_t[]> payload =
        MakeUnique<uint8_t[]>(destinationPayloadSize);
    ok = ::ReadFile(file.get(), payload.get(), header.mPayloadSize, &bytesRead,
                    nullptr);
    if (!ok || bytesRead != header.mPayloadSize) {
      return false;
    }

    uint32_t sizeOfPayloadToIterateOver = header.mPayloadSize;
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
    bool haveWrittenTestEntry = false;
    UNICODE_STRING testUnicodeString;
    // Cast the const-ness away since we're only going to use
    // this to compare against strings.
    testUnicodeString.Buffer = const_cast<PWSTR>(kTestDll);
    testUnicodeString.Length = kTestDllBytes;
    testUnicodeString.MaximumLength = kTestDllBytes;
#endif
    for (uint32_t offset = 0; offset < sizeOfPayloadToIterateOver;
         offset += sizeof(DllBlockInfo)) {
      DllBlockInfo* entry =
          reinterpret_cast<DllBlockInfo*>(payload.get() + offset);
      if (!entry->mName.Length) {
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
        if (shouldInsertBlocklistTestEntry && !haveWrittenTestEntry) {
          // Shift everything forward
          memmove(payload.get() + offset + sizeof(DllBlockInfo),
                  payload.get() + offset, header.mPayloadSize - offset);
          AssignTestEntry(entry, destinationPayloadSize - kTestDllBytes);
          haveWrittenTestEntry = true;
        }
#endif
        break;
      }

      size_t stringOffset = reinterpret_cast<size_t>(entry->mName.Buffer);
      if (stringOffset + entry->mName.Length > header.mPayloadSize) {
        entry->mName.Length = 0;
        break;
      }
      entry->mName.MaximumLength = entry->mName.Length;
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
      if (shouldInsertBlocklistTestEntry && !haveWrittenTestEntry) {
        UNICODE_STRING currentUnicodeString;
        currentUnicodeString.Buffer =
            reinterpret_cast<PWSTR>(payload.get() + stringOffset);
        currentUnicodeString.Length = entry->mName.Length;
        currentUnicodeString.MaximumLength = entry->mName.Length;
        if (RtlCompareUnicodeString(&currentUnicodeString, &testUnicodeString,
                                    TRUE) > 0) {
          // Shift everything forward
          memmove(payload.get() + offset + sizeof(DllBlockInfo),
                  payload.get() + offset, header.mPayloadSize - offset);
          AssignTestEntry(entry, destinationPayloadSize - kTestDllBytes);
          haveWrittenTestEntry = true;
          // Now we have expanded the area of valid memory, so there is more
          // allowable space to iterate over.
          sizeOfPayloadToIterateOver = destinationPayloadSize;
          offset += sizeof(DllBlockInfo);
          ++entry;
        }
        // The start of the string section will be moving ahead (or has already
        // moved ahead) to make room for the test entry
        entry->mName.Buffer +=
            sizeof(DllBlockInfo) / sizeof(entry->mName.Buffer[0]);
      }
#endif
    }
#ifdef BLOCKLIST_INSERT_TEST_ENTRY
    if (shouldInsertBlocklistTestEntry) {
      memcpy(payload.get() + destinationPayloadSize - kTestDllBytes, kTestDll,
             kTestDllBytes);
    }
#endif

    mPayloadSize = destinationPayloadSize;
    mPayload = std::move(payload);
    return true;
  }

 public:
  DynamicBlockList() : mPayloadSize(0) {}
  explicit DynamicBlockList(const wchar_t* aPath) : mPayloadSize(0) {
    LoadFile(aPath);
  }

  DynamicBlockList(DynamicBlockList&&) = default;
  DynamicBlockList& operator=(DynamicBlockList&&) = default;
  DynamicBlockList(const DynamicBlockList&) = delete;
  DynamicBlockList& operator=(const DynamicBlockList&) = delete;

  constexpr uint32_t GetPayloadSize() const { return mPayloadSize; }

  // Return the number of bytes copied
  size_t CopyTo(void* aBuffer, size_t aBufferLength) const {
    if (mPayloadSize > aBufferLength) {
      return 0;
    }
    memcpy(aBuffer, mPayload.get(), mPayloadSize);
    return mPayloadSize;
  }
};

#if defined(MOZILLA_INTERNAL_API) && defined(MOZ_LAUNCHER_PROCESS)

class DynamicBlocklistWriter final : public DynamicBlockListBase {
  template <typename T>
  static nsresult WriteValue(nsIOutputStream* aOutputStream, const T& aValue) {
    uint32_t written;
    return aOutputStream->Write(reinterpret_cast<const char*>(&aValue),
                                sizeof(T), &written);
  }

  template <typename T>
  static nsresult WriteBuffer(nsIOutputStream* aOutputStream, const T* aBuffer,
                              uint32_t aBufferSize) {
    uint32_t written;
    return aOutputStream->Write(reinterpret_cast<const char*>(aBuffer),
                                aBufferSize, &written);
  }

  RefPtr<dom::Promise> mPromise;
  Vector<DllBlockInfo> mArray;
  // All strings are packed in this buffer without null characters
  UniquePtr<uint8_t[]> mStringBuffer;

  size_t mArraySize;
  size_t mStringBufferSize;

  nsresult WriteToFile(const nsAString& aName) const;

 public:
  DynamicBlocklistWriter(
      RefPtr<dom::Promise> aPromise,
      const nsTHashSet<nsStringCaseInsensitiveHashKey>& aBlocklist);
  ~DynamicBlocklistWriter() = default;

  DynamicBlocklistWriter(DynamicBlocklistWriter&&) = default;
  DynamicBlocklistWriter& operator=(DynamicBlocklistWriter&&) = default;
  DynamicBlocklistWriter(const DynamicBlocklistWriter&) = delete;
  DynamicBlocklistWriter& operator=(const DynamicBlocklistWriter&) = delete;

  bool IsReady() const { return mStringBuffer.get(); }

  void Run();
  void Cancel();
};

#endif  // defined(MOZILLA_INTERNAL_API) && defined(MOZ_LAUNCHER_PROCESS)

}  // namespace mozilla

#endif  //  mozilla_DynamicBlocklist_h
