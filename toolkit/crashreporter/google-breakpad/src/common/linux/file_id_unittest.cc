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

// Unit tests for FileID

#include <stdlib.h>

#include "common/linux/file_id.h"
#include "breakpad_googletest_includes.h"

using namespace google_breakpad;


namespace {
typedef testing::Test FileIDTest;
}

TEST(FileIDTest, FileIDStrip) {
  // Calculate the File ID of our binary using
  // FileID::ElfFileIdentifier, then make a copy of our binary,
  // strip it, and ensure that we still get the same result.
  char exe_name[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_name, PATH_MAX - 1);
  ASSERT_NE(len, -1);
  exe_name[len] = '\0';

  // copy our binary to a temp file, and strip it
  char templ[] = "/tmp/file-id-unittest-XXXXXX";
  mktemp(templ);
  char cmdline[4096];
  sprintf(cmdline, "cp \"%s\" \"%s\"", exe_name, templ);
  ASSERT_EQ(system(cmdline), 0);
  sprintf(cmdline, "strip \"%s\"", templ);
  ASSERT_EQ(system(cmdline), 0);

  uint8_t identifier1[sizeof(MDGUID)];
  uint8_t identifier2[sizeof(MDGUID)];
  FileID fileid1(exe_name);
  EXPECT_TRUE(fileid1.ElfFileIdentifier(identifier1));
  FileID fileid2(templ);
  EXPECT_TRUE(fileid2.ElfFileIdentifier(identifier2));
  char identifier_string1[37];
  char identifier_string2[37];
  FileID::ConvertIdentifierToString(identifier1, identifier_string1,
                                    37);
  FileID::ConvertIdentifierToString(identifier2, identifier_string2,
                                    37);
  EXPECT_STREQ(identifier_string1, identifier_string2);
  unlink(templ);
}
