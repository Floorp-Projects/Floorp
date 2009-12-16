// Copyright (c) 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <unistd.h>

#include "client/linux/minidump_writer/linux_dumper.h"
#include "breakpad_googletest_includes.h"

using namespace google_breakpad;

namespace {
typedef testing::Test LinuxDumperTest;
}

TEST(LinuxDumperTest, Setup) {
  LinuxDumper dumper(getpid());
}

TEST(LinuxDumperTest, FindMappings) {
  LinuxDumper dumper(getpid());
  ASSERT_TRUE(dumper.Init());

  ASSERT_TRUE(dumper.FindMapping(reinterpret_cast<void*>(getpid)));
  ASSERT_TRUE(dumper.FindMapping(reinterpret_cast<void*>(printf)));
  ASSERT_FALSE(dumper.FindMapping(NULL));
}

TEST(LinuxDumperTest, ThreadList) {
  LinuxDumper dumper(getpid());
  ASSERT_TRUE(dumper.Init());

  ASSERT_GE(dumper.threads().size(), 1);
  bool found = false;
  for (size_t i = 0; i < dumper.threads().size(); ++i) {
    if (dumper.threads()[i] == getpid()) {
      found = true;
      break;
    }
  }
}

TEST(LinuxDumperTest, BuildProcPath) {
  const pid_t pid = getpid();
  LinuxDumper dumper(pid);

  char maps_path[256] = "dummymappath";
  char maps_path_expected[256];
  snprintf(maps_path_expected, sizeof(maps_path_expected),
           "/proc/%d/maps", pid);
  dumper.BuildProcPath(maps_path, pid, "maps");
  ASSERT_STREQ(maps_path, maps_path_expected);

  // In release mode, we expect BuildProcPath to handle the invalid
  // parameters correctly and fill map_path with an empty
  // NULL-terminated string.
#ifdef NDEBUG
  snprintf(maps_path, sizeof(maps_path), "dummymappath");
  dumper.BuildProcPath(maps_path, 0, "maps");
  EXPECT_STREQ(maps_path, "");

  snprintf(maps_path, sizeof(maps_path), "dummymappath");
  dumper.BuildProcPath(maps_path, getpid(), "");
  EXPECT_STREQ(maps_path, "");

  snprintf(maps_path, sizeof(maps_path), "dummymappath");
  dumper.BuildProcPath(maps_path, getpid(), NULL);
  EXPECT_STREQ(maps_path, "");
#endif
}

TEST(LinuxDumperTest, MappingsIncludeLinuxGate) {
  LinuxDumper dumper(getpid());
  ASSERT_TRUE(dumper.Init());

  void* linux_gate_loc = dumper.FindBeginningOfLinuxGateSharedLibrary(getpid());
  if (linux_gate_loc) {
    bool found_linux_gate = false;

    const wasteful_vector<MappingInfo*> mappings = dumper.mappings();
    const MappingInfo* mapping;
    for (unsigned i = 0; i < mappings.size(); ++i) {
      mapping = mappings[i];
      if (!strcmp(mapping->name, kLinuxGateLibraryName)) {
        found_linux_gate = true;
        break;
      }
    }
    EXPECT_TRUE(found_linux_gate);
    EXPECT_EQ(linux_gate_loc, reinterpret_cast<void*>(mapping->start_addr));
    EXPECT_EQ(0, memcmp(linux_gate_loc, ELFMAG, SELFMAG));
  }
}
