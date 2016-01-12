/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <iostream>
#include <fstream>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

// without this include linking fails
#include "FakeMediaStreamsImpl.h"

#include "signaling/src/sdp/SipccSdpParser.h"

namespace mozilla {

const std::string kDefaultFilename((char *)"/tmp/sdp.bin");
std::string filename(kDefaultFilename);

class SdpParseTest : public ::testing::Test
{
  public:
    SdpParseTest() {}

    void ParseSdp(const std::string &sdp) {
      mSdp = mParser.Parse(sdp);
    }

    void SerializeSdp() {
      if (mSdp) {
          mSdp->Serialize(os);
          std::cout << "Serialized SDP:" << std::endl <<
            os.str() << std::endl;;
      }
    }

    SipccSdpParser mParser;
    mozilla::UniquePtr<Sdp> mSdp;
    std::stringstream os;
}; // class SdpParseTest

TEST_F(SdpParseTest, parseSdpFromFile)
{
  std::ifstream file(filename.c_str(),
                     std::ios::in|std::ios::binary|std::ios::ate);
  ASSERT_TRUE(file.is_open());
  std::streampos size = file.tellg();
  size_t nsize = size;
  nsize+=1;
  char *memblock = new char [nsize];
  memset(memblock, '\0', nsize);
  file.seekg(0, std::ios::beg);
  file.read(memblock, size);
  file.close();
  std::cout << "Read file " << filename << std::endl;
  ParseSdp(memblock);
  std::cout << "Parsed SDP" << std::endl;
  SerializeSdp();
  delete[] memblock;
}

} // End namespace mozilla.

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  if (argc == 2) {
    mozilla::filename = argv[1];
  } else if (argc > 2) {
    std::cerr << "Usage: ./sdp_file_parser [filename]" << std::endl;
    return(1);
  }

  return RUN_ALL_TESTS();
}
