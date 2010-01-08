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

// Unit test for Minidump.  Uses a pre-generated minidump and
// verifies that certain streams are correct.

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "breakpad_googletest_includes.h"
#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/processor/minidump.h"
#include "processor/logging.h"

namespace {

using google_breakpad::Minidump;
using std::ifstream;
using std::istringstream;
using std::string;
using std::vector;
using ::testing::Return;

class MinidumpTest : public ::testing::Test {
public:
  void SetUp() {
    minidump_file_ = string(getenv("srcdir") ? getenv("srcdir") : ".") +
      "/src/processor/testdata/minidump2.dmp";
  }
  string minidump_file_;
};

TEST_F(MinidumpTest, TestMinidumpFromFile) {
  Minidump minidump(minidump_file_);
  ASSERT_EQ(minidump.path(), minidump_file_);
  ASSERT_TRUE(minidump.Read());
  const MDRawHeader* header = minidump.header();
  ASSERT_NE(header, (MDRawHeader*)NULL);
  ASSERT_EQ(header->signature, MD_HEADER_SIGNATURE);
  //TODO: add more checks here
}

TEST_F(MinidumpTest, TestMinidumpFromStream) {
  // read minidump contents into memory, construct a stringstream around them
  ifstream file_stream(minidump_file_.c_str(), std::ios::in);
  ASSERT_TRUE(file_stream.good());
  vector<char> bytes;
  file_stream.seekg(0, std::ios_base::end);
  ASSERT_TRUE(file_stream.good());
  bytes.resize(file_stream.tellg());
  file_stream.seekg(0, std::ios_base::beg);
  ASSERT_TRUE(file_stream.good());
  file_stream.read(&bytes[0], bytes.size());
  ASSERT_TRUE(file_stream.good());
  string str(&bytes[0], bytes.size());
  istringstream stream(str);
  ASSERT_TRUE(stream.good());

  // now read minidump from stringstream
  Minidump minidump(stream);
  ASSERT_EQ(minidump.path(), "");
  ASSERT_TRUE(minidump.Read());
  const MDRawHeader* header = minidump.header();
  ASSERT_NE(header, (MDRawHeader*)NULL);
  ASSERT_EQ(header->signature, MD_HEADER_SIGNATURE);
  //TODO: add more checks here
}

}  // namespace

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
