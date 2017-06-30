// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_syntax_parser.h"
#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/path_service.h"

// Provide a way to read test data from a buffer instead of a file.
class CFX_TestBufferRead : public IFX_SeekableReadStream {
 public:
  static CFX_RetainPtr<CFX_TestBufferRead> Create(
      const unsigned char* buffer_in,
      size_t buf_size) {
    return CFX_RetainPtr<CFX_TestBufferRead>(
        new CFX_TestBufferRead(buffer_in, buf_size));
  }

  // IFX_SeekableReadStream:
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override {
    if (offset < 0 || offset + size > total_size_)
      return false;

    memcpy(buffer, buffer_ + offset, size);
    return true;
  }

  FX_FILESIZE GetSize() override { return (FX_FILESIZE)total_size_; };

 protected:
  CFX_TestBufferRead(const unsigned char* buffer_in, size_t buf_size)
      : buffer_(buffer_in), total_size_(buf_size) {}

  const unsigned char* buffer_;
  size_t total_size_;
};

// A wrapper class to help test member functions of CPDF_Parser.
class CPDF_TestParser : public CPDF_Parser {
 public:
  CPDF_TestParser() {}
  ~CPDF_TestParser() {}

  // Setup reading from a file and initial states.
  bool InitTestFromFile(const FX_CHAR* path) {
    CFX_RetainPtr<IFX_SeekableReadStream> pFileAccess =
        IFX_SeekableReadStream::CreateFromFilename(path);
    if (!pFileAccess)
      return false;

    // For the test file, the header is set at the beginning.
    m_pSyntax->InitParser(pFileAccess, 0);
    return true;
  }

  // Setup reading from a buffer and initial states.
  bool InitTestFromBuffer(const unsigned char* buffer, size_t len) {
    CFX_RetainPtr<CFX_TestBufferRead> buffer_reader =
        CFX_TestBufferRead::Create(buffer, len);

    // For the test file, the header is set at the beginning.
    m_pSyntax->InitParser(buffer_reader, 0);
    return true;
  }

 private:
  // Add test cases here as private friend so that protected members in
  // CPDF_Parser can be accessed by test cases.
  // Need to access RebuildCrossRef.
  FRIEND_TEST(cpdf_parser, RebuildCrossRefCorrectly);
  FRIEND_TEST(cpdf_parser, RebuildCrossRefFailed);
  // Need to access LoadCrossRefV4.
  FRIEND_TEST(cpdf_parser, LoadCrossRefV4);
};

TEST(cpdf_parser, RebuildCrossRefCorrectly) {
  CPDF_TestParser parser;
  std::string test_file;
  ASSERT_TRUE(PathService::GetTestFilePath("parser_rebuildxref_correct.pdf",
                                           &test_file));
  ASSERT_TRUE(parser.InitTestFromFile(test_file.c_str())) << test_file;

  ASSERT_TRUE(parser.RebuildCrossRef());
  const FX_FILESIZE offsets[] = {0, 15, 61, 154, 296, 374, 450};
  const uint16_t versions[] = {0, 0, 2, 4, 6, 8, 0};
  for (size_t i = 0; i < FX_ArraySize(offsets); ++i)
    EXPECT_EQ(offsets[i], parser.m_ObjectInfo[i].pos);
  for (size_t i = 0; i < FX_ArraySize(versions); ++i)
    EXPECT_EQ(versions[i], parser.m_ObjectInfo[i].gennum);
}

TEST(cpdf_parser, RebuildCrossRefFailed) {
  CPDF_TestParser parser;
  std::string test_file;
  ASSERT_TRUE(PathService::GetTestFilePath(
      "parser_rebuildxref_error_notrailer.pdf", &test_file));
  ASSERT_TRUE(parser.InitTestFromFile(test_file.c_str())) << test_file;

  ASSERT_FALSE(parser.RebuildCrossRef());
}

TEST(cpdf_parser, LoadCrossRefV4) {
  {
    const unsigned char xref_table[] =
        "xref \n"
        "0 6 \n"
        "0000000003 65535 f \n"
        "0000000017 00000 n \n"
        "0000000081 00000 n \n"
        "0000000000 00007 f \n"
        "0000000331 00000 n \n"
        "0000000409 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(
        parser.InitTestFromBuffer(xref_table, FX_ArraySize(xref_table)));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, 0, false));
    const FX_FILESIZE offsets[] = {0, 17, 81, 0, 331, 409};
    const uint8_t types[] = {0, 1, 1, 0, 1, 1};
    for (size_t i = 0; i < FX_ArraySize(offsets); ++i) {
      EXPECT_EQ(offsets[i], parser.m_ObjectInfo[i].pos);
      EXPECT_EQ(types[i], parser.m_ObjectInfo[i].type);
    }
  }
  {
    const unsigned char xref_table[] =
        "xref \n"
        "0 1 \n"
        "0000000000 65535 f \n"
        "3 1 \n"
        "0000025325 00000 n \n"
        "8 2 \n"
        "0000025518 00002 n \n"
        "0000025635 00000 n \n"
        "12 1 \n"
        "0000025777 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(
        parser.InitTestFromBuffer(xref_table, FX_ArraySize(xref_table)));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, 0, false));
    const FX_FILESIZE offsets[] = {0, 0,     0,     25325, 0, 0,    0,
                                   0, 25518, 25635, 0,     0, 25777};
    const uint8_t types[] = {0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1};
    for (size_t i = 0; i < FX_ArraySize(offsets); ++i) {
      EXPECT_EQ(offsets[i], parser.m_ObjectInfo[i].pos);
      EXPECT_EQ(types[i], parser.m_ObjectInfo[i].type);
    }
  }
  {
    const unsigned char xref_table[] =
        "xref \n"
        "0 1 \n"
        "0000000000 65535 f \n"
        "3 1 \n"
        "0000025325 00000 n \n"
        "8 2 \n"
        "0000000000 65535 f \n"
        "0000025635 00000 n \n"
        "12 1 \n"
        "0000025777 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(
        parser.InitTestFromBuffer(xref_table, FX_ArraySize(xref_table)));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, 0, false));
    const FX_FILESIZE offsets[] = {0, 0, 0,     25325, 0, 0,    0,
                                   0, 0, 25635, 0,     0, 25777};
    const uint8_t types[] = {0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1};
    for (size_t i = 0; i < FX_ArraySize(offsets); ++i) {
      EXPECT_EQ(offsets[i], parser.m_ObjectInfo[i].pos);
      EXPECT_EQ(types[i], parser.m_ObjectInfo[i].type);
    }
  }
  {
    const unsigned char xref_table[] =
        "xref \n"
        "0 7 \n"
        "0000000002 65535 f \n"
        "0000000023 00000 n \n"
        "0000000003 65535 f \n"
        "0000000004 65535 f \n"
        "0000000000 65535 f \n"
        "0000000045 00000 n \n"
        "0000000179 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(
        parser.InitTestFromBuffer(xref_table, FX_ArraySize(xref_table)));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, 0, false));
    const FX_FILESIZE offsets[] = {0, 23, 0, 0, 0, 45, 179};
    const uint8_t types[] = {0, 1, 0, 0, 0, 1, 1};
    for (size_t i = 0; i < FX_ArraySize(offsets); ++i) {
      EXPECT_EQ(offsets[i], parser.m_ObjectInfo[i].pos);
      EXPECT_EQ(types[i], parser.m_ObjectInfo[i].type);
    }
  }
}
