/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "gtest/gtest.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/NativeNt.h"
#include "nsWindowsHelpers.h"

using namespace mozilla;

class TempFile final {
  wchar_t mFullPath[MAX_PATH + 1];

 public:
  TempFile() : mFullPath{0} {
    wchar_t tempDir[MAX_PATH + 1];
    DWORD len = ::GetTempPathW(ArrayLength(tempDir), tempDir);
    if (!len) {
      return;
    }

    len = ::GetTempFileNameW(tempDir, L"poi", 0, mFullPath);
    if (!len) {
      return;
    }
  }

  operator const wchar_t*() const { return mFullPath[0] ? mFullPath : nullptr; }
};

class Overlapped final {
  nsAutoHandle mEvent;
  OVERLAPPED mOverlapped;

 public:
  Overlapped()
      : mEvent(::CreateEventW(nullptr, TRUE, FALSE, nullptr)), mOverlapped{} {
    mOverlapped.hEvent = mEvent.get();
  }

  operator OVERLAPPED*() { return &mOverlapped; }

  bool Wait(HANDLE aHandle) {
    DWORD numBytes;
    if (!::GetOverlappedResult(aHandle, &mOverlapped, &numBytes, TRUE)) {
      return false;
    }

    return true;
  }
};

const uint32_t kMagic = 0x12345678;

void FileOpSync(const wchar_t* aPath) {
  nsAutoHandle file(::CreateFileW(aPath, GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_NE(file.get(), INVALID_HANDLE_VALUE);

  DWORD buffer = kMagic, numBytes = 0;
  OVERLAPPED seek = {};
  EXPECT_TRUE(WriteFile(file.get(), &buffer, sizeof(buffer), &numBytes, &seek));
  EXPECT_TRUE(::FlushFileBuffers(file.get()));

  seek.Offset = 0;
  buffer = 0;
  EXPECT_TRUE(
      ::ReadFile(file.get(), &buffer, sizeof(buffer), &numBytes, &seek));
  EXPECT_EQ(buffer, kMagic);

  WIN32_FILE_ATTRIBUTE_DATA fullAttr = {};
  EXPECT_TRUE(::GetFileAttributesExW(aPath, GetFileExInfoStandard, &fullAttr));
}

void FileOpAsync(const wchar_t* aPath) {
  constexpr int kNumPages = 10;
  constexpr int kPageSize = 4096;

  Array<UniquePtr<void, VirtualFreeDeleter>, kNumPages> pages;
  Array<FILE_SEGMENT_ELEMENT, kNumPages + 1> segments;
  for (int i = 0; i < kNumPages; ++i) {
    auto p = reinterpret_cast<uint32_t*>(::VirtualAlloc(
        nullptr, kPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    ASSERT_TRUE(p);

    pages[i].reset(p);
    segments[i].Buffer = p;

    p[0] = kMagic;
  }

  nsAutoHandle file(::CreateFileW(
      aPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
      nullptr));
  ASSERT_NE(file.get(), INVALID_HANDLE_VALUE);

  Overlapped writeOp;
  if (!::WriteFileGather(file.get(), segments.begin(), kNumPages * kPageSize,
                         nullptr, writeOp)) {
    EXPECT_EQ(::GetLastError(), ERROR_IO_PENDING);
    EXPECT_TRUE(writeOp.Wait(file.get()));
  }

  for (int i = 0; i < kNumPages; ++i) {
    *reinterpret_cast<uint32_t*>(pages[i].get()) = 0;
  }

  Overlapped readOp;
  if (!::ReadFileScatter(file.get(), segments.begin(), kNumPages * kPageSize,
                         nullptr, readOp)) {
    EXPECT_EQ(::GetLastError(), ERROR_IO_PENDING);
    EXPECT_TRUE(readOp.Wait(file.get()));
  }

  for (int i = 0; i < kNumPages; ++i) {
    EXPECT_EQ(*reinterpret_cast<uint32_t*>(pages[i].get()), kMagic);
  }
}

TEST(PoisonIOInterposer, NormalThread)
{
  mozilla::IOInterposerInit ioInterposerGuard;
  TempFile tempFile;
  FileOpSync(tempFile);
  FileOpAsync(tempFile);
  EXPECT_TRUE(::DeleteFileW(tempFile));
}

TEST(PoisonIOInterposer, NullTlsPointer)
{
  void* originalTls = mozilla::nt::RtlGetThreadLocalStoragePointer();
  mozilla::IOInterposerInit ioInterposerGuard;

  // Simulate a loader worker thread (TEB::LoaderWorker = 1)
  // where ThreadLocalStorage is never allocated.
  mozilla::nt::RtlSetThreadLocalStoragePointerForTestingOnly(nullptr);

  TempFile tempFile;
  FileOpSync(tempFile);
  FileOpAsync(tempFile);
  EXPECT_TRUE(::DeleteFileW(tempFile));

  mozilla::nt::RtlSetThreadLocalStoragePointerForTestingOnly(originalTls);
}
