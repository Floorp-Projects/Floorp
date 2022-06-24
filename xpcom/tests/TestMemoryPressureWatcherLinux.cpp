/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/AvailableMemoryWatcherUtils.h"

#include <fstream>

using namespace mozilla;

const char* kMemInfoPath = "/proc/meminfo";
const char* kTestfilePath = "testdata";

// Test that we are reading some value from /proc/meminfo.
// If the values are nonzero, the test is a success.
void TestFromProc() {
  MemoryInfo memInfo{0, 0};
  ReadMemoryFile(kMemInfoPath, memInfo);
  MOZ_RELEASE_ASSERT(memInfo.memTotal != 0);
  MOZ_RELEASE_ASSERT(memInfo.memAvailable != 0);
}

// Test a file using expected syntax.
void TestFromFile() {
  MemoryInfo memInfo{0, 0};
  std::ofstream aFile(kTestfilePath);
  aFile << "MemTotal:       12345 kB\n";
  aFile << "MemFree:        99999 kB\n";
  aFile << "MemAvailable:   54321 kB\n";
  aFile.close();

  ReadMemoryFile(kTestfilePath, memInfo);

  MOZ_RELEASE_ASSERT(memInfo.memTotal == 12345);
  MOZ_RELEASE_ASSERT(memInfo.memAvailable == 54321);

  // remove our dummy file
  remove(kTestfilePath);
}

// Test a file with useless data. Results should be
// the starting struct with {0,0}.
void TestInvalidFile() {
  MemoryInfo memInfo{0, 0};
  std::ofstream aFile(kTestfilePath);
  aFile << "foo:       12345 kB\n";
  aFile << "bar";
  aFile.close();

  ReadMemoryFile(kTestfilePath, memInfo);

  MOZ_RELEASE_ASSERT(memInfo.memTotal == 0);
  MOZ_RELEASE_ASSERT(memInfo.memAvailable == 0);

  // remove our dummy file
  remove(kTestfilePath);
}

int main() {
  TestFromProc();
  TestFromFile();
  TestInvalidFile();
  return 0;
}
