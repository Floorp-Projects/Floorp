/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "LulCommonExt.h"
#include "LulDwarfExt.h"
#include "LulDwarfInt.h"
#include "LulTestInfrastructure.h"

using testing::Test;
using testing::Return;
using testing::Sequence;
using testing::InSequence;
using testing::_;
using lul_test::CFISection;
using lul_test::test_assembler::kBigEndian;
using lul_test::test_assembler::kLittleEndian;
using lul_test::test_assembler::Label;

#define PERHAPS_WRITE_DEBUG_FRAME_FILE(name, section) /**/
#define PERHAPS_WRITE_EH_FRAME_FILE(name, section)    /**/

// Set this to 0 to make LUL be completely silent during tests.
// Set it to 1 to get logging output from LUL, presumably for
// the purpose of debugging it.
#define DEBUG_LUL_TEST_DWARF 0

// LUL needs a callback for its logging sink.
static void
gtest_logging_sink_for_LulTestDwarf(const char* str) {
  if (DEBUG_LUL_TEST_DWARF == 0) {
    return;
  }
  // Ignore any trailing \n, since LOG will add one anyway.
  size_t n = strlen(str);
  if (n > 0 && str[n-1] == '\n') {
    char* tmp = strdup(str);
    tmp[n-1] = 0;
    fprintf(stderr, "LUL-in-gtest: %s\n", tmp);
    free(tmp);
  } else {
    fprintf(stderr, "LUL-in-gtest: %s\n", str);
  }
}

namespace lul {

class MockCallFrameInfoHandler : public CallFrameInfo::Handler {
 public:
  MOCK_METHOD6(Entry, bool(size_t offset, uint64 address, uint64 length,
                           uint8 version, const std::string &augmentation,
                           unsigned return_address));
  MOCK_METHOD2(UndefinedRule, bool(uint64 address, int reg));
  MOCK_METHOD2(SameValueRule, bool(uint64 address, int reg));
  MOCK_METHOD4(OffsetRule, bool(uint64 address, int reg, int base_register,
                                long offset));
  MOCK_METHOD4(ValOffsetRule, bool(uint64 address, int reg, int base_register,
                                   long offset));
  MOCK_METHOD3(RegisterRule, bool(uint64 address, int reg, int base_register));
  MOCK_METHOD3(ExpressionRule, bool(uint64 address, int reg,
                                    const std::string &expression));
  MOCK_METHOD3(ValExpressionRule, bool(uint64 address, int reg,
                                       const std::string &expression));
  MOCK_METHOD0(End, bool());
  MOCK_METHOD2(PersonalityRoutine, bool(uint64 address, bool indirect));
  MOCK_METHOD2(LanguageSpecificDataArea, bool(uint64 address, bool indirect));
  MOCK_METHOD0(SignalHandler, bool());
};

class MockCallFrameErrorReporter : public CallFrameInfo::Reporter {
 public:
  MockCallFrameErrorReporter()
     : Reporter(gtest_logging_sink_for_LulTestDwarf,
                "mock filename", "mock section")
  { }
  MOCK_METHOD2(Incomplete, void(uint64, CallFrameInfo::EntryKind));
  MOCK_METHOD1(EarlyEHTerminator, void(uint64));
  MOCK_METHOD2(CIEPointerOutOfRange, void(uint64, uint64));
  MOCK_METHOD2(BadCIEId, void(uint64, uint64));
  MOCK_METHOD2(UnrecognizedVersion, void(uint64, int version));
  MOCK_METHOD2(UnrecognizedAugmentation, void(uint64, const string &));
  MOCK_METHOD2(InvalidPointerEncoding, void(uint64, uint8));
  MOCK_METHOD2(UnusablePointerEncoding, void(uint64, uint8));
  MOCK_METHOD2(RestoreInCIE, void(uint64, uint64));
  MOCK_METHOD3(BadInstruction, void(uint64, CallFrameInfo::EntryKind, uint64));
  MOCK_METHOD3(NoCFARule, void(uint64, CallFrameInfo::EntryKind, uint64));
  MOCK_METHOD3(EmptyStateStack, void(uint64, CallFrameInfo::EntryKind, uint64));
  MOCK_METHOD3(ClearingCFARule, void(uint64, CallFrameInfo::EntryKind, uint64));
};

struct CFIFixture {

  enum { kCFARegister = CallFrameInfo::Handler::kCFARegister };

  CFIFixture() {
    // Default expectations for the data handler.
    //
    // - Leave Entry and End without expectations, as it's probably a
    //   good idea to set those explicitly in each test.
    //
    // - Expect the *Rule functions to not be called,
    //   so that each test can simply list the calls they expect.
    //
    // I gather I could use StrictMock for this, but the manual seems
    // to suggest using that only as a last resort, and this isn't so
    // bad.
    EXPECT_CALL(handler, UndefinedRule(_, _)).Times(0);
    EXPECT_CALL(handler, SameValueRule(_, _)).Times(0);
    EXPECT_CALL(handler, OffsetRule(_, _, _, _)).Times(0);
    EXPECT_CALL(handler, ValOffsetRule(_, _, _, _)).Times(0);
    EXPECT_CALL(handler, RegisterRule(_, _, _)).Times(0);
    EXPECT_CALL(handler, ExpressionRule(_, _, _)).Times(0);
    EXPECT_CALL(handler, ValExpressionRule(_, _, _)).Times(0);
    EXPECT_CALL(handler, PersonalityRoutine(_, _)).Times(0);
    EXPECT_CALL(handler, LanguageSpecificDataArea(_, _)).Times(0);
    EXPECT_CALL(handler, SignalHandler()).Times(0);

    // Default expectations for the error/warning reporer.
    EXPECT_CALL(reporter, Incomplete(_, _)).Times(0);
    EXPECT_CALL(reporter, EarlyEHTerminator(_)).Times(0);
    EXPECT_CALL(reporter, CIEPointerOutOfRange(_, _)).Times(0);
    EXPECT_CALL(reporter, BadCIEId(_, _)).Times(0);
    EXPECT_CALL(reporter, UnrecognizedVersion(_, _)).Times(0);
    EXPECT_CALL(reporter, UnrecognizedAugmentation(_, _)).Times(0);
    EXPECT_CALL(reporter, InvalidPointerEncoding(_, _)).Times(0);
    EXPECT_CALL(reporter, UnusablePointerEncoding(_, _)).Times(0);
    EXPECT_CALL(reporter, RestoreInCIE(_, _)).Times(0);
    EXPECT_CALL(reporter, BadInstruction(_, _, _)).Times(0);
    EXPECT_CALL(reporter, NoCFARule(_, _, _)).Times(0);
    EXPECT_CALL(reporter, EmptyStateStack(_, _, _)).Times(0);
    EXPECT_CALL(reporter, ClearingCFARule(_, _, _)).Times(0);
  }

  MockCallFrameInfoHandler handler;
  MockCallFrameErrorReporter reporter;
};

class LulDwarfCFI: public CFIFixture, public Test { };

TEST_F(LulDwarfCFI, EmptyRegion) {
  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);
  static const char data[1] = { 42 };

  ByteReader reader(ENDIANNESS_BIG);
  CallFrameInfo parser(data, 0, &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

TEST_F(LulDwarfCFI, IncompleteLength32) {
  CFISection section(kBigEndian, 8);
  section
      // Not even long enough for an initial length.
      .D16(0xa0f)
      // Padding to keep valgrind happy. We subtract these off when we
      // construct the parser.
      .D16(0);

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, Incomplete(_, CallFrameInfo::kUnknown))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size() - 2,
                       &reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

TEST_F(LulDwarfCFI, IncompleteLength64) {
  CFISection section(kLittleEndian, 4);
  section
      // An incomplete 64-bit DWARF initial length.
      .D32(0xffffffff).D32(0x71fbaec2)
      // Padding to keep valgrind happy. We subtract these off when we
      // construct the parser.
      .D32(0);

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, Incomplete(_, CallFrameInfo::kUnknown))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader reader(ENDIANNESS_LITTLE);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size() - 4,
                       &reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

TEST_F(LulDwarfCFI, IncompleteId32) {
  CFISection section(kBigEndian, 8);
  section
      .D32(3)                      // Initial length, not long enough for id
      .D8(0xd7).D8(0xe5).D8(0xf1)  // incomplete id
      .CIEHeader(8727, 3983, 8889, 3, "")
      .FinishEntry();

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, Incomplete(_, CallFrameInfo::kUnknown))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

TEST_F(LulDwarfCFI, BadId32) {
  CFISection section(kBigEndian, 8);
  section
      .D32(0x100)                       // Initial length
      .D32(0xe802fade)                  // bogus ID
      .Append(0x100 - 4, 0x42);         // make the length true
  section
      .CIEHeader(1672, 9872, 8529, 3, "")
      .FinishEntry();

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, CIEPointerOutOfRange(_, 0xe802fade))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

// A lone CIE shouldn't cause any handler calls.
TEST_F(LulDwarfCFI, SingleCIE) {
  CFISection section(kLittleEndian, 4);
  section.CIEHeader(0xffe799a8, 0x3398dcdd, 0x6e9683de, 3, "");
  section.Append(10, lul::DW_CFA_nop);
  section.FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("SingleCIE", section);

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_LITTLE);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// One FDE, one CIE.
TEST_F(LulDwarfCFI, OneFDE) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(0x4be22f75, 0x2492236e, 0x6b6efb87, 3, "")
      .FinishEntry()
      .FDEHeader(cie, 0x7714740d, 0x3d5a10cd)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("OneFDE", section);

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0x7714740d, 0x3d5a10cd, 3, "", 0x6b6efb87))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// Two FDEs share a CIE.
TEST_F(LulDwarfCFI, TwoFDEsOneCIE) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // First FDE. readelf complains about this one because it makes
      // a forward reference to its CIE.
      .FDEHeader(cie, 0xa42744df, 0xa3b42121)
      .FinishEntry()
      // CIE.
      .Mark(&cie)
      .CIEHeader(0x04f7dc7b, 0x3d00c05f, 0xbd43cb59, 3, "")
      .FinishEntry()
      // Second FDE.
      .FDEHeader(cie, 0x6057d391, 0x700f608d)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("TwoFDEsOneCIE", section);

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0xa42744df, 0xa3b42121, 3, "", 0xbd43cb59))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0x6057d391, 0x700f608d, 3, "", 0xbd43cb59))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// Two FDEs, two CIEs.
TEST_F(LulDwarfCFI, TwoFDEsTwoCIEs) {
  CFISection section(kLittleEndian, 8);
  Label cie1, cie2;
  section
      // First CIE.
      .Mark(&cie1)
      .CIEHeader(0x694d5d45, 0x4233221b, 0xbf45e65a, 3, "")
      .FinishEntry()
      // First FDE which cites second CIE. readelf complains about
      // this one because it makes a forward reference to its CIE.
      .FDEHeader(cie2, 0x778b27dfe5871f05ULL, 0x324ace3448070926ULL)
      .FinishEntry()
      // Second FDE, which cites first CIE.
      .FDEHeader(cie1, 0xf6054ca18b10bf5fULL, 0x45fdb970d8bca342ULL)
      .FinishEntry()
      // Second CIE.
      .Mark(&cie2)
      .CIEHeader(0xfba3fad7, 0x6287e1fd, 0x61d2c581, 2, "")
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("TwoFDEsTwoCIEs", section);

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0x778b27dfe5871f05ULL, 0x324ace3448070926ULL, 2,
                      "", 0x61d2c581))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0xf6054ca18b10bf5fULL, 0x45fdb970d8bca342ULL, 3,
                      "", 0xbf45e65a))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_LITTLE);
  reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// An FDE whose CIE specifies a version we don't recognize.
TEST_F(LulDwarfCFI, BadVersion) {
  CFISection section(kBigEndian, 4);
  Label cie1, cie2;
  section
      .Mark(&cie1)
      .CIEHeader(0xca878cf0, 0x7698ec04, 0x7b616f54, 0x52, "")
      .FinishEntry()
      // We should skip this entry, as its CIE specifies a version we
      // don't recognize.
      .FDEHeader(cie1, 0x08852292, 0x2204004a)
      .FinishEntry()
      // Despite the above, we should visit this entry.
      .Mark(&cie2)
      .CIEHeader(0x7c3ae7c9, 0xb9b9a512, 0x96cb3264, 3, "")
      .FinishEntry()
      .FDEHeader(cie2, 0x2094735a, 0x6e875501)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("BadVersion", section);

  EXPECT_CALL(reporter, UnrecognizedVersion(_, 0x52))
    .WillOnce(Return());

  {
    InSequence s;
    // We should see no mention of the first FDE, but we should get
    // a call to Entry for the second.
    EXPECT_CALL(handler, Entry(_, 0x2094735a, 0x6e875501, 3, "",
                               0x96cb3264))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

// An FDE whose CIE specifies an augmentation we don't recognize.
TEST_F(LulDwarfCFI, BadAugmentation) {
  CFISection section(kBigEndian, 4);
  Label cie1, cie2;
  section
      .Mark(&cie1)
      .CIEHeader(0x4be22f75, 0x2492236e, 0x6b6efb87, 3, "spaniels!")
      .FinishEntry()
      // We should skip this entry, as its CIE specifies an
      // augmentation we don't recognize.
      .FDEHeader(cie1, 0x7714740d, 0x3d5a10cd)
      .FinishEntry()
      // Despite the above, we should visit this entry.
      .Mark(&cie2)
      .CIEHeader(0xf8bc4399, 0x8cf09931, 0xf2f519b2, 3, "")
      .FinishEntry()
      .FDEHeader(cie2, 0x7bf0fda0, 0xcbcd28d8)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("BadAugmentation", section);

  EXPECT_CALL(reporter, UnrecognizedAugmentation(_, "spaniels!"))
    .WillOnce(Return());

  {
    InSequence s;
    // We should see no mention of the first FDE, but we should get
    // a call to Entry for the second.
    EXPECT_CALL(handler, Entry(_, 0x7bf0fda0, 0xcbcd28d8, 3, "",
                               0xf2f519b2))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

// The return address column field is a byte in CFI version 1
// (DWARF2), but a ULEB128 value in version 3 (DWARF3).
TEST_F(LulDwarfCFI, CIEVersion1ReturnColumn) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // CIE, using the version 1 format: return column is a ubyte.
      .Mark(&cie)
      // Use a value for the return column that is parsed differently
      // as a ubyte and as a ULEB128.
      .CIEHeader(0xbcdea24f, 0x5be28286, 0x9f, 1, "")
      .FinishEntry()
      // FDE, citing that CIE.
      .FDEHeader(cie, 0xb8d347b5, 0x825e55dc)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("CIEVersion1ReturnColumn", section);

  {
    InSequence s;
    EXPECT_CALL(handler, Entry(_, 0xb8d347b5, 0x825e55dc, 1, "", 0x9f))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// The return address column field is a byte in CFI version 1
// (DWARF2), but a ULEB128 value in version 3 (DWARF3).
TEST_F(LulDwarfCFI, CIEVersion3ReturnColumn) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // CIE, using the version 3 format: return column is a ULEB128.
      .Mark(&cie)
      // Use a value for the return column that is parsed differently
      // as a ubyte and as a ULEB128.
      .CIEHeader(0x0ab4758d, 0xc010fdf7, 0x89, 3, "")
      .FinishEntry()
      // FDE, citing that CIE.
      .FDEHeader(cie, 0x86763f2b, 0x2a66dc23)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("CIEVersion3ReturnColumn", section);

  {
    InSequence s;
    EXPECT_CALL(handler, Entry(_, 0x86763f2b, 0x2a66dc23, 3, "", 0x89))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader reader(ENDIANNESS_BIG);
  reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

struct CFIInsnFixture: public CFIFixture {
  CFIInsnFixture() : CFIFixture() {
    data_factor = 0xb6f;
    return_register = 0x9be1ed9f;
    version = 3;
    cfa_base_register = 0x383a3aa;
    cfa_offset = 0xf748;
  }

  // Prepare SECTION to receive FDE instructions.
  //
  // - Append a stock CIE header that establishes the fixture's
  //   code_factor, data_factor, return_register, version, and
  //   augmentation values.
  // - Have the CIE set up a CFA rule using cfa_base_register and
  //   cfa_offset.
  // - Append a stock FDE header, referring to the above CIE, for the
  //   fde_size bytes at fde_start. Choose fde_start and fde_size
  //   appropriately for the section's address size.
  // - Set appropriate expectations on handler in sequence s for the
  //   frame description entry and the CIE's CFA rule.
  //
  // On return, SECTION is ready to have FDE instructions appended to
  // it, and its FinishEntry member called.
  void StockCIEAndFDE(CFISection *section) {
    // Choose appropriate constants for our address size.
    if (section->AddressSize() == 4) {
      fde_start = 0xc628ecfbU;
      fde_size = 0x5dee04a2;
      code_factor = 0x60b;
    } else {
      assert(section->AddressSize() == 8);
      fde_start = 0x0005c57ce7806bd3ULL;
      fde_size = 0x2699521b5e333100ULL;
      code_factor = 0x01008e32855274a8ULL;
    }

    // Create the CIE.
    (*section)
        .Mark(&cie_label)
        .CIEHeader(code_factor, data_factor, return_register, version,
                   "")
        .D8(lul::DW_CFA_def_cfa)
        .ULEB128(cfa_base_register)
        .ULEB128(cfa_offset)
        .FinishEntry();

    // Create the FDE.
    section->FDEHeader(cie_label, fde_start, fde_size);

    // Expect an Entry call for the FDE and a ValOffsetRule call for the
    // CIE's CFA rule.
    EXPECT_CALL(handler, Entry(_, fde_start, fde_size, version, "",
                               return_register))
        .InSequence(s)
        .WillOnce(Return(true));
    EXPECT_CALL(handler, ValOffsetRule(fde_start, kCFARegister,
                                       cfa_base_register, cfa_offset))
      .InSequence(s)
      .WillOnce(Return(true));
  }

  // Run the contents of SECTION through a CallFrameInfo parser,
  // expecting parser.Start to return SUCCEEDS.  Caller may optionally
  // supply, via READER, its own ByteReader.  If that's absent, a
  // local one is used.
  void ParseSection(CFISection *section,
                    bool succeeds = true, ByteReader* reader = nullptr) {
    string contents;
    EXPECT_TRUE(section->GetContents(&contents));
    lul::Endianness endianness;
    if (section->endianness() == kBigEndian)
      endianness = ENDIANNESS_BIG;
    else {
      assert(section->endianness() == kLittleEndian);
      endianness = ENDIANNESS_LITTLE;
    }
    ByteReader local_reader(endianness);
    ByteReader* reader_to_use = reader ? reader : &local_reader;
    reader_to_use->SetAddressSize(section->AddressSize());
    CallFrameInfo parser(contents.data(), contents.size(),
                         reader_to_use, &handler, &reporter);
    if (succeeds)
      EXPECT_TRUE(parser.Start());
    else
      EXPECT_FALSE(parser.Start());
  }

  Label cie_label;
  Sequence s;
  uint64 code_factor;
  int data_factor;
  unsigned return_register;
  unsigned version;
  unsigned cfa_base_register;
  int cfa_offset;
  uint64 fde_start, fde_size;
};

class LulDwarfCFIInsn: public CFIInsnFixture, public Test { };

TEST_F(LulDwarfCFIInsn, DW_CFA_set_loc) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_set_loc).D32(0xb1ee3e7a)
      // Use DW_CFA_def_cfa to force a handler call that we can use to
      // check the effect of the DW_CFA_set_loc.
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x4defb431).ULEB128(0x6d17b0ee)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_set_loc", section);

  EXPECT_CALL(handler,
              ValOffsetRule(0xb1ee3e7a, kCFARegister, 0x4defb431, 0x6d17b0ee))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_advance_loc) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_advance_loc | 0x2a)
      // Use DW_CFA_def_cfa to force a handler call that we can use to
      // check the effect of the DW_CFA_advance_loc.
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x5bbb3715).ULEB128(0x0186c7bf)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_advance_loc", section);

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start + 0x2a * code_factor,
                            kCFARegister, 0x5bbb3715, 0x0186c7bf))
        .InSequence(s)
        .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_advance_loc1) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_advance_loc1).D8(0xd8)
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x69d5696a).ULEB128(0x1eb7fc93)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_advance_loc1", section);

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0xd8 * code_factor),
                            kCFARegister, 0x69d5696a, 0x1eb7fc93))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_advance_loc2) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_advance_loc2).D16(0x3adb)
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x3a368bed).ULEB128(0x3194ee37)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_advance_loc2", section);

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0x3adb * code_factor),
                            kCFARegister, 0x3a368bed, 0x3194ee37))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_advance_loc4) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_advance_loc4).D32(0x15813c88)
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x135270c5).ULEB128(0x24bad7cb)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_advance_loc4", section);

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0x15813c88ULL * code_factor),
                            kCFARegister, 0x135270c5, 0x24bad7cb))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_MIPS_advance_loc8) {
  code_factor = 0x2d;
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_MIPS_advance_loc8).D64(0x3c4f3945b92c14ULL)
      .D8(lul::DW_CFA_def_cfa).ULEB128(0xe17ed602).ULEB128(0x3d162e7f)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_advance_loc8", section);

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0x3c4f3945b92c14ULL * code_factor),
                            kCFARegister, 0xe17ed602, 0x3d162e7f))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x4e363a85).ULEB128(0x815f9aa7)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("DW_CFA_def_cfa", section);

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x4e363a85, 0x815f9aa7))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_sf) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_sf).ULEB128(0x8ccb32b7).LEB128(0x9ea)
      .D8(lul::DW_CFA_def_cfa_sf).ULEB128(0x9b40f5da).LEB128(-0x40a2)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x8ccb32b7,
                            0x9ea * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x9b40f5da,
                            -0x40a2 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_register) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_register).ULEB128(0x3e7e9363)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x3e7e9363, cfa_offset))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

// DW_CFA_def_cfa_register should have no effect when applied to a
// non-base/offset rule.
TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_registerBadRule) {
  ByteReader reader(ENDIANNESS_BIG);
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_expression).Block("needle in a haystack")
      .D8(lul::DW_CFA_def_cfa_register).ULEB128(0xf1b49e49)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValExpressionRule(fde_start, kCFARegister,
                                "needle in a haystack"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_offset) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_offset).ULEB128(0x1e8e3b9b)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, cfa_base_register,
                            0x1e8e3b9b))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_offset_sf) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_offset_sf).LEB128(0x970)
      .D8(lul::DW_CFA_def_cfa_offset_sf).LEB128(-0x2cd)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, cfa_base_register,
                            0x970 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, cfa_base_register,
                            -0x2cd * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

// DW_CFA_def_cfa_offset should have no effect when applied to a
// non-base/offset rule.
TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_offsetBadRule) {
  ByteReader reader(ENDIANNESS_BIG);
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_expression).Block("six ways to Sunday")
      .D8(lul::DW_CFA_def_cfa_offset).ULEB128(0x1e8e3b9b)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValExpressionRule(fde_start, kCFARegister,
                                "six ways to Sunday"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}


TEST_F(LulDwarfCFIInsn, DW_CFA_def_cfa_expression) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_def_cfa_expression).Block("eating crow")
      .FinishEntry();

  EXPECT_CALL(handler, ValExpressionRule(fde_start, kCFARegister,
                                         "eating crow"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_undefined) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_undefined).ULEB128(0x300ce45d)
      .FinishEntry();

  EXPECT_CALL(handler, UndefinedRule(fde_start, 0x300ce45d))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_same_value) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_same_value).ULEB128(0x3865a760)
      .FinishEntry();

  EXPECT_CALL(handler, SameValueRule(fde_start, 0x3865a760))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_offset) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_offset | 0x2c).ULEB128(0x9f6)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x2c, kCFARegister, 0x9f6 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_offset_extended) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_offset_extended).ULEB128(0x402b).ULEB128(0xb48)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start,
                         0x402b, kCFARegister, 0xb48 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_offset_extended_sf) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_offset_extended_sf)
          .ULEB128(0x997c23ee).LEB128(0x2d00)
      .D8(lul::DW_CFA_offset_extended_sf)
          .ULEB128(0x9519eb82).LEB128(-0xa77)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x997c23ee,
                         kCFARegister, 0x2d00 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x9519eb82,
                         kCFARegister, -0xa77 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_val_offset) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_offset).ULEB128(0x623562fe).ULEB128(0x673)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, 0x623562fe,
                            kCFARegister, 0x673 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_val_offset_sf) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_offset_sf).ULEB128(0x6f4f).LEB128(0xaab)
      .D8(lul::DW_CFA_val_offset_sf).ULEB128(0x2483).LEB128(-0x8a2)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, 0x6f4f,
                            kCFARegister, 0xaab * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, 0x2483,
                            kCFARegister, -0x8a2 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_register) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_register).ULEB128(0x278d18f9).ULEB128(0x1a684414)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0x278d18f9, 0x1a684414))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_expression) {
  ByteReader reader(ENDIANNESS_BIG);
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_expression).ULEB128(0xa1619fb2)
      .Block("plus ça change, plus c'est la même chose")
      .FinishEntry();

  EXPECT_CALL(handler,
              ExpressionRule(fde_start, 0xa1619fb2,
                             "plus ça change, plus c'est la même chose"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_val_expression) {
  ByteReader reader(ENDIANNESS_BIG);
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_expression).ULEB128(0xc5e4a9e3)
      .Block("he who has the gold makes the rules")
      .FinishEntry();

  EXPECT_CALL(handler,
              ValExpressionRule(fde_start, 0xc5e4a9e3,
                                "he who has the gold makes the rules"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_restore) {
  CFISection section(kLittleEndian, 8);
  code_factor = 0x01bd188a9b1fa083ULL;
  data_factor = -0x1ac8;
  return_register = 0x8c35b049;
  version = 2;
  fde_start = 0x2d70fe998298bbb1ULL;
  fde_size = 0x46ccc2e63cf0b108ULL;
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(code_factor, data_factor, return_register, version,
                 "")
      // Provide a CFA rule, because register rules require them.
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x6ca1d50e).ULEB128(0x372e38e8)
      // Provide an offset(N) rule for register 0x3c.
      .D8(lul::DW_CFA_offset | 0x3c).ULEB128(0xb348)
      .FinishEntry()
      // In the FDE...
      .FDEHeader(cie, fde_start, fde_size)
      // At a second address, provide a new offset(N) rule for register 0x3c.
      .D8(lul::DW_CFA_advance_loc | 0x13)
      .D8(lul::DW_CFA_offset | 0x3c).ULEB128(0x9a50)
      // At a third address, restore the original rule for register 0x3c.
      .D8(lul::DW_CFA_advance_loc | 0x01)
      .D8(lul::DW_CFA_restore | 0x3c)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, fde_start, fde_size, version, "", return_register))
        .WillOnce(Return(true));
    // CIE's CFA rule.
    EXPECT_CALL(handler,
                ValOffsetRule(fde_start,
                              kCFARegister, 0x6ca1d50e, 0x372e38e8))
        .WillOnce(Return(true));
    // CIE's rule for register 0x3c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start, 0x3c,
                           kCFARegister, 0xb348 * data_factor))
        .WillOnce(Return(true));
    // FDE's rule for register 0x3c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + 0x13 * code_factor, 0x3c,
                           kCFARegister, 0x9a50 * data_factor))
        .WillOnce(Return(true));
    // Restore CIE's rule for register 0x3c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + (0x13 + 0x01) * code_factor, 0x3c,
                           kCFARegister, 0xb348 * data_factor))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_restoreNoRule) {
  CFISection section(kBigEndian, 4);
  code_factor = 0x005f78143c1c3b82ULL;
  data_factor = 0x25d0;
  return_register = 0xe8;
  version = 1;
  fde_start = 0x4062e30f;
  fde_size = 0x5302a389;
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(code_factor, data_factor, return_register, version, "")
      // Provide a CFA rule, because register rules require them.
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x470aa334).ULEB128(0x099ef127)
      .FinishEntry()
      // In the FDE...
      .FDEHeader(cie, fde_start, fde_size)
      // At a second address, provide an offset(N) rule for register 0x2c.
      .D8(lul::DW_CFA_advance_loc | 0x7)
      .D8(lul::DW_CFA_offset | 0x2c).ULEB128(0x1f47)
      // At a third address, restore the (missing) CIE rule for register 0x2c.
      .D8(lul::DW_CFA_advance_loc | 0xb)
      .D8(lul::DW_CFA_restore | 0x2c)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, fde_start, fde_size, version, "", return_register))
        .WillOnce(Return(true));
    // CIE's CFA rule.
    EXPECT_CALL(handler,
                ValOffsetRule(fde_start,
                              kCFARegister, 0x470aa334, 0x099ef127))
        .WillOnce(Return(true));
    // FDE's rule for register 0x2c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + 0x7 * code_factor, 0x2c,
                           kCFARegister, 0x1f47 * data_factor))
        .WillOnce(Return(true));
    // Restore CIE's (missing) rule for register 0x2c.
    EXPECT_CALL(handler,
                SameValueRule(fde_start + (0x7 + 0xb) * code_factor, 0x2c))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_restore_extended) {
  CFISection section(kBigEndian, 4);
  code_factor = 0x126e;
  data_factor = -0xd8b;
  return_register = 0x77711787;
  version = 3;
  fde_start = 0x01f55a45;
  fde_size = 0x452adb80;
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(code_factor, data_factor, return_register, version,
                 "", true /* dwarf64 */ )
      // Provide a CFA rule, because register rules require them.
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x56fa0edd).ULEB128(0x097f78a5)
      // Provide an offset(N) rule for register 0x0f9b8a1c.
      .D8(lul::DW_CFA_offset_extended)
          .ULEB128(0x0f9b8a1c).ULEB128(0xc979)
      .FinishEntry()
      // In the FDE...
      .FDEHeader(cie, fde_start, fde_size)
      // At a second address, provide a new offset(N) rule for reg 0x0f9b8a1c.
      .D8(lul::DW_CFA_advance_loc | 0x3)
      .D8(lul::DW_CFA_offset_extended)
          .ULEB128(0x0f9b8a1c).ULEB128(0x3b7b)
      // At a third address, restore the original rule for register 0x0f9b8a1c.
      .D8(lul::DW_CFA_advance_loc | 0x04)
      .D8(lul::DW_CFA_restore_extended).ULEB128(0x0f9b8a1c)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, fde_start, fde_size, version, "", return_register))
        .WillOnce(Return(true));
    // CIE's CFA rule.
    EXPECT_CALL(handler,
                ValOffsetRule(fde_start, kCFARegister, 0x56fa0edd, 0x097f78a5))
        .WillOnce(Return(true));
    // CIE's rule for register 0x0f9b8a1c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start, 0x0f9b8a1c, kCFARegister,
                           0xc979 * data_factor))
        .WillOnce(Return(true));
    // FDE's rule for register 0x0f9b8a1c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + 0x3 * code_factor, 0x0f9b8a1c,
                           kCFARegister, 0x3b7b * data_factor))
        .WillOnce(Return(true));
    // Restore CIE's rule for register 0x0f9b8a1c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + (0x3 + 0x4) * code_factor, 0x0f9b8a1c,
                           kCFARegister, 0xc979 * data_factor))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_remember_and_restore_state) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);

  // We create a state, save it, modify it, and then restore. We
  // refer to the state that is overridden the restore as the
  // "outgoing" state, and the restored state the "incoming" state.
  //
  // Register         outgoing        incoming        expect
  // 1                offset(N)       no rule         new "same value" rule
  // 2                register(R)     offset(N)       report changed rule
  // 3                offset(N)       offset(M)       report changed offset
  // 4                offset(N)       offset(N)       no report
  // 5                offset(N)       no rule         new "same value" rule
  section
      // Create the "incoming" state, which we will save and later restore.
      .D8(lul::DW_CFA_offset | 2).ULEB128(0x9806)
      .D8(lul::DW_CFA_offset | 3).ULEB128(0x995d)
      .D8(lul::DW_CFA_offset | 4).ULEB128(0x7055)
      .D8(lul::DW_CFA_remember_state)
      // Advance to a new instruction; an implementation could legitimately
      // ignore all but the final rule for a given register at a given address.
      .D8(lul::DW_CFA_advance_loc | 1)
      // Create the "outgoing" state, which we will discard.
      .D8(lul::DW_CFA_offset | 1).ULEB128(0xea1a)
      .D8(lul::DW_CFA_register).ULEB128(2).ULEB128(0x1d2a3767)
      .D8(lul::DW_CFA_offset | 3).ULEB128(0xdd29)
      .D8(lul::DW_CFA_offset | 5).ULEB128(0xf1ce)
      // At a third address, restore the incoming state.
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  uint64 addr = fde_start;

  // Expect the incoming rules to be reported.
  EXPECT_CALL(handler, OffsetRule(addr, 2, kCFARegister, 0x9806 * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 3, kCFARegister, 0x995d * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 4, kCFARegister, 0x7055 * data_factor))
    .InSequence(s).WillOnce(Return(true));

  addr += code_factor;

  // After the save, we establish the outgoing rule set.
  EXPECT_CALL(handler, OffsetRule(addr, 1, kCFARegister, 0xea1a * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(addr, 2, 0x1d2a3767))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 3, kCFARegister, 0xdd29 * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 5, kCFARegister, 0xf1ce * data_factor))
    .InSequence(s).WillOnce(Return(true));

  addr += code_factor;

  // Finally, after the restore, expect to see the differences from
  // the outgoing to the incoming rules reported.
  EXPECT_CALL(handler, SameValueRule(addr, 1))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 2, kCFARegister, 0x9806 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 3, kCFARegister, 0x995d * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SameValueRule(addr, 5))
      .InSequence(s).WillOnce(Return(true));

  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

// Check that restoring a rule set reports changes to the CFA rule.
TEST_F(LulDwarfCFIInsn, DW_CFA_remember_and_restore_stateCFA) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);

  section
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_def_cfa_offset).ULEB128(0x90481102)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start + code_factor, kCFARegister,
                                     cfa_base_register, 0x90481102))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + code_factor * 2, kCFARegister,
                                     cfa_base_register, cfa_offset))
      .InSequence(s).WillOnce(Return(true));

  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_nop) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_nop)
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x3fb8d4f1).ULEB128(0x078dc67b)
      .D8(lul::DW_CFA_nop)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x3fb8d4f1, 0x078dc67b))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_GNU_window_save) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_GNU_window_save)
      .FinishEntry();

  // Don't include all the rules in any particular sequence.

  // The caller's %o0-%o7 have become the callee's %i0-%i7. This is
  // the GCC register numbering.
  for (int i = 8; i < 16; i++)
    EXPECT_CALL(handler, RegisterRule(fde_start, i, i + 16))
        .WillOnce(Return(true));
  // The caller's %l0-%l7 and %i0-%i7 have been saved at the top of
  // its frame.
  for (int i = 16; i < 32; i++)
    EXPECT_CALL(handler, OffsetRule(fde_start, i, kCFARegister, (i-16) * 4))
        .WillOnce(Return(true));

  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_GNU_args_size) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_GNU_args_size).ULEB128(0xeddfa520)
      // Verify that we see this, meaning we parsed the above properly.
      .D8(lul::DW_CFA_offset | 0x23).ULEB128(0x269)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x23, kCFARegister, 0x269 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIInsn, DW_CFA_GNU_negative_offset_extended) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_GNU_negative_offset_extended)
      .ULEB128(0x430cc87a).ULEB128(0x613)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x430cc87a,
                         kCFARegister, -0x613 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

// Three FDEs: skip the second
TEST_F(LulDwarfCFIInsn, SkipFDE) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // CIE, used by all FDEs.
      .Mark(&cie)
      .CIEHeader(0x010269f2, 0x9177, 0xedca5849, 2, "")
      .D8(lul::DW_CFA_def_cfa).ULEB128(0x42ed390b).ULEB128(0x98f43aad)
      .FinishEntry()
      // First FDE.
      .FDEHeader(cie, 0xa870ebdd, 0x60f6aa4)
      .D8(lul::DW_CFA_register).ULEB128(0x3a860351).ULEB128(0x6c9a6bcf)
      .FinishEntry()
      // Second FDE.
      .FDEHeader(cie, 0xc534f7c0, 0xf6552e9, true /* dwarf64 */)
      .D8(lul::DW_CFA_register).ULEB128(0x1b62c234).ULEB128(0x26586b18)
      .FinishEntry()
      // Third FDE.
      .FDEHeader(cie, 0xf681cfc8, 0x7e4594e)
      .D8(lul::DW_CFA_register).ULEB128(0x26c53934).ULEB128(0x18eeb8a4)
      .FinishEntry();

  {
    InSequence s;

    // Process the first FDE.
    EXPECT_CALL(handler, Entry(_, 0xa870ebdd, 0x60f6aa4, 2, "", 0xedca5849))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, ValOffsetRule(0xa870ebdd, kCFARegister,
                                       0x42ed390b, 0x98f43aad))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, RegisterRule(0xa870ebdd, 0x3a860351, 0x6c9a6bcf))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));

    // Skip the second FDE.
    EXPECT_CALL(handler, Entry(_, 0xc534f7c0, 0xf6552e9, 2, "", 0xedca5849))
        .WillOnce(Return(false));

    // Process the third FDE.
    EXPECT_CALL(handler, Entry(_, 0xf681cfc8, 0x7e4594e, 2, "", 0xedca5849))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, ValOffsetRule(0xf681cfc8, kCFARegister,
                                       0x42ed390b, 0x98f43aad))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, RegisterRule(0xf681cfc8, 0x26c53934, 0x18eeb8a4))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));
  }

  ParseSection(&section);
}

// Quit processing in the middle of an entry's instructions.
TEST_F(LulDwarfCFIInsn, QuitMidentry) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_register).ULEB128(0xe0cf850d).ULEB128(0x15aab431)
      .D8(lul::DW_CFA_expression).ULEB128(0x46750aa5).Block("meat")
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0xe0cf850d, 0x15aab431))
      .InSequence(s).WillOnce(Return(false));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseSection(&section, false);
}

class LulDwarfCFIRestore: public CFIInsnFixture, public Test { };

TEST_F(LulDwarfCFIRestore, RestoreUndefinedRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_undefined).ULEB128(0x0bac878e)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, UndefinedRule(fde_start, 0x0bac878e))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreUndefinedRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_undefined).ULEB128(0x7dedff5f)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_same_value).ULEB128(0x7dedff5f)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, UndefinedRule(fde_start, 0x7dedff5f))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SameValueRule(fde_start + code_factor, 0x7dedff5f))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + 2 * code_factor, 0x7dedff5f))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreSameValueRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_same_value).ULEB128(0xadbc9b3a)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, SameValueRule(fde_start, 0xadbc9b3a))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreSameValueRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_same_value).ULEB128(0x3d90dcb5)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_undefined).ULEB128(0x3d90dcb5)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, SameValueRule(fde_start, 0x3d90dcb5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0x3d90dcb5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SameValueRule(fde_start + 2 * code_factor, 0x3d90dcb5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreOffsetRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_offset | 0x14).ULEB128(0xb6f)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, OffsetRule(fde_start, 0x14,
                                  kCFARegister, 0xb6f * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreOffsetRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_offset | 0x21).ULEB128(0xeb7)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_undefined).ULEB128(0x21)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, OffsetRule(fde_start, 0x21,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0x21))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(fde_start + 2 * code_factor, 0x21,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreOffsetRuleChangedOffset) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_offset | 0x21).ULEB128(0x134)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_offset | 0x21).ULEB128(0xf4f)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, OffsetRule(fde_start, 0x21,
                                  kCFARegister, 0x134 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(fde_start + code_factor, 0x21,
                                  kCFARegister, 0xf4f * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(fde_start + 2 * code_factor, 0x21,
                                  kCFARegister, 0x134 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreValOffsetRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_offset).ULEB128(0x829caee6).ULEB128(0xe4c)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start, 0x829caee6,
                                  kCFARegister, 0xe4c * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreValOffsetRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_offset).ULEB128(0xf17c36d6).ULEB128(0xeb7)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_undefined).ULEB128(0xf17c36d6)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start, 0xf17c36d6,
                                     kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xf17c36d6))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + 2 * code_factor, 0xf17c36d6,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreValOffsetRuleChangedValOffset) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_offset).ULEB128(0x2cf0ab1b).ULEB128(0x562)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_val_offset).ULEB128(0x2cf0ab1b).ULEB128(0xe88)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start, 0x2cf0ab1b,
                                  kCFARegister, 0x562 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + code_factor, 0x2cf0ab1b,
                                  kCFARegister, 0xe88 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + 2 * code_factor, 0x2cf0ab1b,
                                  kCFARegister, 0x562 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreRegisterRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_register).ULEB128(0x77514acc).ULEB128(0x464de4ce)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0x77514acc, 0x464de4ce))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreRegisterRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_register).ULEB128(0xe39acce5).ULEB128(0x095f1559)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_undefined).ULEB128(0xe39acce5)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0xe39acce5, 0x095f1559))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xe39acce5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(fde_start + 2 * code_factor, 0xe39acce5,
                                    0x095f1559))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreRegisterRuleChangedRegister) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_register).ULEB128(0xd40e21b1).ULEB128(0x16607d6a)
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_register).ULEB128(0xd40e21b1).ULEB128(0xbabb4742)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0xd40e21b1, 0x16607d6a))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(fde_start + code_factor, 0xd40e21b1,
                                    0xbabb4742))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(fde_start + 2 * code_factor, 0xd40e21b1,
                                    0x16607d6a))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(LulDwarfCFIRestore, RestoreExpressionRuleUnchanged) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_expression).ULEB128(0x666ae152).Block("dwarf")
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ExpressionRule(fde_start, 0x666ae152, "dwarf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIRestore, RestoreExpressionRuleChanged) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_expression).ULEB128(0xb5ca5c46).Block("elf")
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_undefined).ULEB128(0xb5ca5c46)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ExpressionRule(fde_start, 0xb5ca5c46, "elf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xb5ca5c46))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ExpressionRule(fde_start + 2 * code_factor, 0xb5ca5c46,
                                      "elf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIRestore, RestoreExpressionRuleChangedExpression) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_expression).ULEB128(0x500f5739).Block("smurf")
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_expression).ULEB128(0x500f5739).Block("orc")
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ExpressionRule(fde_start, 0x500f5739, "smurf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ExpressionRule(fde_start + code_factor, 0x500f5739,
                                      "orc"))
      .InSequence(s).WillOnce(Return(true));
  // Expectations are not wishes.
  EXPECT_CALL(handler, ExpressionRule(fde_start + 2 * code_factor, 0x500f5739,
                                      "smurf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIRestore, RestoreValExpressionRuleUnchanged) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_expression).ULEB128(0x666ae152)
      .Block("hideous")
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValExpressionRule(fde_start, 0x666ae152, "hideous"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIRestore, RestoreValExpressionRuleChanged) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_expression).ULEB128(0xb5ca5c46)
      .Block("revolting")
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_undefined).ULEB128(0xb5ca5c46)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("RestoreValExpressionRuleChanged", section);

  EXPECT_CALL(handler, ValExpressionRule(fde_start, 0xb5ca5c46, "revolting"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xb5ca5c46))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValExpressionRule(fde_start + 2 * code_factor, 0xb5ca5c46,
                                         "revolting"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

TEST_F(LulDwarfCFIRestore, RestoreValExpressionRuleChangedValExpression) {
  ByteReader reader(ENDIANNESS_LITTLE);
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(lul::DW_CFA_val_expression).ULEB128(0x500f5739)
      .Block("repulsive")
      .D8(lul::DW_CFA_remember_state)
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_val_expression).ULEB128(0x500f5739)
      .Block("nauseous")
      .D8(lul::DW_CFA_advance_loc | 1)
      .D8(lul::DW_CFA_restore_state)
      .FinishEntry();

  PERHAPS_WRITE_DEBUG_FRAME_FILE("RestoreValExpressionRuleChangedValExpression",
                                 section);

  EXPECT_CALL(handler, ValExpressionRule(fde_start, 0x500f5739, "repulsive"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValExpressionRule(fde_start + code_factor, 0x500f5739,
                                         "nauseous"))
      .InSequence(s).WillOnce(Return(true));
  // Expectations are not wishes.
  EXPECT_CALL(handler, ValExpressionRule(fde_start + 2 * code_factor, 0x500f5739,
                                         "repulsive"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section, true, &reader);
}

struct EHFrameFixture: public CFIInsnFixture {
  EHFrameFixture()
      : CFIInsnFixture(), section(kBigEndian, 4, true) {
    encoded_pointer_bases.cfi  = 0x7f496cb2;
    encoded_pointer_bases.text = 0x540f67b6;
    encoded_pointer_bases.data = 0xe3eab768;
    section.SetEncodedPointerBases(encoded_pointer_bases);
  }
  CFISection section;
  CFISection::EncodedPointerBases encoded_pointer_bases;

  // Parse CFIInsnFixture::ParseSection, but parse the section as
  // .eh_frame data, supplying stock base addresses.
  void ParseEHFrameSection(CFISection *section, bool succeeds = true) {
    EXPECT_TRUE(section->ContainsEHFrame());
    string contents;
    EXPECT_TRUE(section->GetContents(&contents));
    lul::Endianness endianness;
    if (section->endianness() == kBigEndian)
      endianness = ENDIANNESS_BIG;
    else {
      assert(section->endianness() == kLittleEndian);
      endianness = ENDIANNESS_LITTLE;
    }
    ByteReader reader(endianness);
    reader.SetAddressSize(section->AddressSize());
    reader.SetCFIDataBase(encoded_pointer_bases.cfi, contents.data());
    reader.SetTextBase(encoded_pointer_bases.text);
    reader.SetDataBase(encoded_pointer_bases.data);
    CallFrameInfo parser(contents.data(), contents.size(),
                         &reader, &handler, &reporter, true);
    if (succeeds)
      EXPECT_TRUE(parser.Start());
    else
      EXPECT_FALSE(parser.Start());
  }

};

class LulDwarfEHFrame: public EHFrameFixture, public Test { };

// A simple CIE, an FDE, and a terminator.
TEST_F(LulDwarfEHFrame, Terminator) {
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(9968, 2466, 67, 1, "")
      .D8(lul::DW_CFA_def_cfa).ULEB128(3772).ULEB128(1372)
      .FinishEntry()
      .FDEHeader(cie, 0x848037a1, 0x7b30475e)
      .D8(lul::DW_CFA_set_loc).D32(0x17713850)
      .D8(lul::DW_CFA_undefined).ULEB128(5721)
      .FinishEntry()
      .D32(0)                           // Terminate the sequence.
      // This FDE should be ignored.
      .FDEHeader(cie, 0xf19629fe, 0x439fb09b)
      .FinishEntry();

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.Terminator", section);

  EXPECT_CALL(handler, Entry(_, 0x848037a1, 0x7b30475e, 1, "", 67))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(0x848037a1, kCFARegister, 3772, 1372))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(0x17713850, 5721))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(reporter, EarlyEHTerminator(_))
      .InSequence(s).WillOnce(Return());

  ParseEHFrameSection(&section);
}

// The parser should recognize the Linux Standards Base 'z' augmentations.
TEST_F(LulDwarfEHFrame, SimpleFDE) {
   lul::DwarfPointerEncoding lsda_encoding =
      lul::DwarfPointerEncoding(lul::DW_EH_PE_indirect
                                | lul::DW_EH_PE_datarel
                                | lul::DW_EH_PE_sdata2);
  lul::DwarfPointerEncoding fde_encoding =
      lul::DwarfPointerEncoding(lul::DW_EH_PE_textrel
                                | lul::DW_EH_PE_udata2);

  section.SetPointerEncoding(fde_encoding);
  section.SetEncodedPointerBases(encoded_pointer_bases);
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(4873, 7012, 100, 1, "zSLPR")
      .ULEB128(7)                                // Augmentation data length
      .D8(lsda_encoding)                         // LSDA pointer format
      .D8(lul::DW_EH_PE_pcrel)                   // personality pointer format
      .EncodedPointer(0x97baa00, lul::DW_EH_PE_pcrel) // and value
      .D8(fde_encoding)                          // FDE pointer format
      .D8(lul::DW_CFA_def_cfa).ULEB128(6706).ULEB128(31)
      .FinishEntry()
      .FDEHeader(cie, 0x540f6b56, 0xf686)
      .ULEB128(2)                                // Augmentation data length
      .EncodedPointer(0xe3eab475, lsda_encoding) // LSDA pointer, signed
      .D8(lul::DW_CFA_set_loc)
      .EncodedPointer(0x540fa4ce, fde_encoding)
      .D8(lul::DW_CFA_undefined).ULEB128(0x675e)
      .FinishEntry()
      .D32(0);                                   // terminator

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.SimpleFDE", section);

  EXPECT_CALL(handler, Entry(_, 0x540f6b56, 0xf686, 1, "zSLPR", 100))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, PersonalityRoutine(0x97baa00, false))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, LanguageSpecificDataArea(0xe3eab475, true))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SignalHandler())
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(0x540f6b56, kCFARegister, 6706, 31))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(0x540fa4ce, 0x675e))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseEHFrameSection(&section);
}

// Check that we can handle an empty 'z' augmentation.
TEST_F(LulDwarfEHFrame, EmptyZ) {
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(5955, 5805, 228, 1, "z")
      .ULEB128(0)                                // Augmentation data length
      .D8(lul::DW_CFA_def_cfa).ULEB128(3629).ULEB128(247)
      .FinishEntry()
      .FDEHeader(cie, 0xda007738, 0xfb55c641)
      .ULEB128(0)                                // Augmentation data length
      .D8(lul::DW_CFA_advance_loc1).D8(11)
      .D8(lul::DW_CFA_undefined).ULEB128(3769)
      .FinishEntry();

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.EmptyZ", section);

  EXPECT_CALL(handler, Entry(_, 0xda007738, 0xfb55c641, 1, "z", 228))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(0xda007738, kCFARegister, 3629, 247))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(0xda007738 + 11 * 5955, 3769))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseEHFrameSection(&section);
}

// Check that we recognize bad 'z' augmentation characters.
TEST_F(LulDwarfEHFrame, BadZ) {
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(6937, 1045, 142, 1, "zQ")
      .ULEB128(0)                                // Augmentation data length
      .D8(lul::DW_CFA_def_cfa).ULEB128(9006).ULEB128(7725)
      .FinishEntry()
      .FDEHeader(cie, 0x1293efa8, 0x236f53f2)
      .ULEB128(0)                                // Augmentation data length
      .D8(lul::DW_CFA_advance_loc | 12)
      .D8(lul::DW_CFA_register).ULEB128(5667).ULEB128(3462)
      .FinishEntry();

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.BadZ", section);

  EXPECT_CALL(reporter, UnrecognizedAugmentation(_, "zQ"))
      .WillOnce(Return());

  ParseEHFrameSection(&section, false);
}

TEST_F(LulDwarfEHFrame, zL) {
  Label cie;
  lul::DwarfPointerEncoding lsda_encoding =
      lul::DwarfPointerEncoding(lul::DW_EH_PE_funcrel | lul::DW_EH_PE_udata2);
  section
      .Mark(&cie)
      .CIEHeader(9285, 9959, 54, 1, "zL")
      .ULEB128(1)                       // Augmentation data length
      .D8(lsda_encoding)                // encoding for LSDA pointer in FDE

      .FinishEntry()
      .FDEHeader(cie, 0xd40091aa, 0x9aa6e746)
      .ULEB128(2)                       // Augmentation data length
      .EncodedPointer(0xd40099cd, lsda_encoding) // LSDA pointer
      .FinishEntry()
      .D32(0);                                   // terminator

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.zL", section);

  EXPECT_CALL(handler, Entry(_, 0xd40091aa, 0x9aa6e746, 1, "zL", 54))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, LanguageSpecificDataArea(0xd40099cd, false))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseEHFrameSection(&section);
}

TEST_F(LulDwarfEHFrame, zP) {
  Label cie;
  lul::DwarfPointerEncoding personality_encoding =
      lul::DwarfPointerEncoding(lul::DW_EH_PE_datarel | lul::DW_EH_PE_udata2);
  section
      .Mark(&cie)
      .CIEHeader(1097, 6313, 17, 1, "zP")
      .ULEB128(3)                  // Augmentation data length
      .D8(personality_encoding)    // encoding for personality routine
      .EncodedPointer(0xe3eaccac, personality_encoding) // value
      .FinishEntry()
      .FDEHeader(cie, 0x0c8350c9, 0xbef11087)
      .ULEB128(0)                       // Augmentation data length
      .FinishEntry()
      .D32(0);                                   // terminator

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.zP", section);

  EXPECT_CALL(handler, Entry(_, 0x0c8350c9, 0xbef11087, 1, "zP", 17))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, PersonalityRoutine(0xe3eaccac, false))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseEHFrameSection(&section);
}

TEST_F(LulDwarfEHFrame, zR) {
  Label cie;
  lul::DwarfPointerEncoding pointer_encoding =
      lul::DwarfPointerEncoding(lul::DW_EH_PE_textrel | lul::DW_EH_PE_sdata2);
  section.SetPointerEncoding(pointer_encoding);
  section
      .Mark(&cie)
      .CIEHeader(8011, 5496, 75, 1, "zR")
      .ULEB128(1)                       // Augmentation data length
      .D8(pointer_encoding)             // encoding for FDE addresses
      .FinishEntry()
      .FDEHeader(cie, 0x540f9431, 0xbd0)
      .ULEB128(0)                       // Augmentation data length
      .FinishEntry()
      .D32(0);                          // terminator

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.zR", section);

  EXPECT_CALL(handler, Entry(_, 0x540f9431, 0xbd0, 1, "zR", 75))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseEHFrameSection(&section);
}

TEST_F(LulDwarfEHFrame, zS) {
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(9217, 7694, 57, 1, "zS")
      .ULEB128(0)                                // Augmentation data length
      .FinishEntry()
      .FDEHeader(cie, 0xd40091aa, 0x9aa6e746)
      .ULEB128(0)                                // Augmentation data length
      .FinishEntry()
      .D32(0);                                   // terminator

  PERHAPS_WRITE_EH_FRAME_FILE("EHFrame.zS", section);

  EXPECT_CALL(handler, Entry(_, 0xd40091aa, 0x9aa6e746, 1, "zS", 57))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SignalHandler())
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));

  ParseEHFrameSection(&section);
}

// These tests require manual inspection of the test output.
struct CFIReporterFixture {
  CFIReporterFixture() : reporter(gtest_logging_sink_for_LulTestDwarf,
                                  "test file name", "test section name") { }
  CallFrameInfo::Reporter reporter;
};

class LulDwarfCFIReporter: public CFIReporterFixture, public Test { };

TEST_F(LulDwarfCFIReporter, Incomplete) {
  reporter.Incomplete(0x0102030405060708ULL, CallFrameInfo::kUnknown);
}

TEST_F(LulDwarfCFIReporter, EarlyEHTerminator) {
  reporter.EarlyEHTerminator(0x0102030405060708ULL);
}

TEST_F(LulDwarfCFIReporter, CIEPointerOutOfRange) {
  reporter.CIEPointerOutOfRange(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
}

TEST_F(LulDwarfCFIReporter, BadCIEId) {
  reporter.BadCIEId(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
}

TEST_F(LulDwarfCFIReporter, UnrecognizedVersion) {
  reporter.UnrecognizedVersion(0x0123456789abcdefULL, 43);
}

TEST_F(LulDwarfCFIReporter, UnrecognizedAugmentation) {
  reporter.UnrecognizedAugmentation(0x0123456789abcdefULL, "poodles");
}

TEST_F(LulDwarfCFIReporter, InvalidPointerEncoding) {
  reporter.InvalidPointerEncoding(0x0123456789abcdefULL, 0x42);
}

TEST_F(LulDwarfCFIReporter, UnusablePointerEncoding) {
  reporter.UnusablePointerEncoding(0x0123456789abcdefULL, 0x42);
}

TEST_F(LulDwarfCFIReporter, RestoreInCIE) {
  reporter.RestoreInCIE(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
}

TEST_F(LulDwarfCFIReporter, BadInstruction) {
  reporter.BadInstruction(0x0123456789abcdefULL, CallFrameInfo::kFDE,
                          0xfedcba9876543210ULL);
}

TEST_F(LulDwarfCFIReporter, NoCFARule) {
  reporter.NoCFARule(0x0123456789abcdefULL, CallFrameInfo::kCIE,
                     0xfedcba9876543210ULL);
}

TEST_F(LulDwarfCFIReporter, EmptyStateStack) {
  reporter.EmptyStateStack(0x0123456789abcdefULL, CallFrameInfo::kTerminator,
                           0xfedcba9876543210ULL);
}

TEST_F(LulDwarfCFIReporter, ClearingCFARule) {
  reporter.ClearingCFARule(0x0123456789abcdefULL, CallFrameInfo::kFDE,
                           0xfedcba9876543210ULL);
}
class LulDwarfExpr : public Test { };

class MockSummariser : public Summariser {
public:
  MockSummariser() : Summariser(nullptr, 0, nullptr) {}
  MOCK_METHOD2(Entry, void(uintptr_t, uintptr_t));
  MOCK_METHOD0(End, void());
  MOCK_METHOD5(Rule, void(uintptr_t, int, LExprHow, int16_t, int64_t));
  MOCK_METHOD1(AddPfxInstr, uint32_t(PfxInstr));
};

TEST_F(LulDwarfExpr, SimpleTransliteration) {
  MockSummariser summ;
  ByteReader reader(ENDIANNESS_LITTLE);

  CFISection section(kLittleEndian, 8);
  section
     .D8(DW_OP_lit0)
     .D8(DW_OP_lit31)
     .D8(DW_OP_breg0 + 17).LEB128(-1234)
     .D8(DW_OP_const4s).D32(0xFEDC9876)
     .D8(DW_OP_deref)
     .D8(DW_OP_and)
     .D8(DW_OP_plus)
     .D8(DW_OP_minus)
     .D8(DW_OP_shl)
     .D8(DW_OP_ge);
  string expr;
  bool ok = section.GetContents(&expr);
  EXPECT_TRUE(ok);

  {
    InSequence s;
    // required start marker
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Start, 0)));
    // DW_OP_lit0
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_SImm32, 0)));
    // DW_OP_lit31
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_SImm32, 31)));
    // DW_OP_breg17 -1234
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_DwReg, 17)));
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_SImm32, -1234)));
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Add)));
    // DW_OP_const4s 0xFEDC9876
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_SImm32, 0xFEDC9876)));
    // DW_OP_deref
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Deref)));
    // DW_OP_and
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_And)));
    // DW_OP_plus
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Add)));
    // DW_OP_minus
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Sub)));
    // DW_OP_shl
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Shl)));
    // DW_OP_ge
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_CmpGES)));
    // required end marker
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_End)));
  }

  int32_t ix = parseDwarfExpr(&summ, &reader, expr, false, false, false);
  EXPECT_TRUE(ix >= 0);
}

TEST_F(LulDwarfExpr, UnknownOpcode) {
  MockSummariser summ;
  ByteReader reader(ENDIANNESS_LITTLE);

  CFISection section(kLittleEndian, 8);
  section
     .D8(DW_OP_lo_user - 1);
  string expr;
  bool ok = section.GetContents(&expr);
  EXPECT_TRUE(ok);

  {
    InSequence s;
    // required start marker
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Start, 0)));
  }

  int32_t ix = parseDwarfExpr(&summ, &reader, expr, false, false, false);
  EXPECT_TRUE(ix == -1);
}

TEST_F(LulDwarfExpr, ExpressionOverrun) {
  MockSummariser summ;
  ByteReader reader(ENDIANNESS_LITTLE);

  CFISection section(kLittleEndian, 8);
  section
     .D8(DW_OP_const4s).D8(0x12).D8(0x34).D8(0x56);
  string expr;
  bool ok = section.GetContents(&expr);
  EXPECT_TRUE(ok);

  {
    InSequence s;
    // required start marker
    EXPECT_CALL(summ, AddPfxInstr(PfxInstr(PX_Start, 0)));
    // DW_OP_const4s followed by 3 (a.k.a. not enough) bytes
    // We expect PfxInstr(PX_Simm32, not-known-for-sure-32-bit-immediate)
    // Hence must use _ as the argument.
    EXPECT_CALL(summ, AddPfxInstr(_));
  }

  int32_t ix = parseDwarfExpr(&summ, &reader, expr, false, false, false);
  EXPECT_TRUE(ix == -1);
}

// We'll need to mention specific Dwarf registers in the EvaluatePfxExpr tests,
// and those names are arch-specific, so a bit of macro magic is helpful.
#if defined(GP_ARCH_arm)
# define TESTED_REG_STRUCT_NAME  r11
# define TESTED_REG_DWARF_NAME   DW_REG_ARM_R11
#elif defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
# define TESTED_REG_STRUCT_NAME  xbp
# define TESTED_REG_DWARF_NAME   DW_REG_INTEL_XBP
#else
# error "Unknown plat"
#endif

struct EvaluatePfxExprFixture {
  // Creates:
  // initial stack, AVMA 0x12345678, at offset 4 bytes = 0xdeadbeef
  // initial regs, with XBP = 0x14141356
  // initial CFA = 0x5432ABCD
  EvaluatePfxExprFixture() {
    // The test stack.
    si.mStartAvma = 0x12345678;
    si.mLen = 0;
#   define XX(_byte) do { si.mContents[si.mLen++] = (_byte); } while (0)
    XX(0x55); XX(0x55); XX(0x55); XX(0x55);
    if (sizeof(void*) == 8) {
      // le64
      XX(0xEF); XX(0xBE); XX(0xAD); XX(0xDE); XX(0); XX(0); XX(0); XX(0);
    } else {
      // le32
      XX(0xEF); XX(0xBE); XX(0xAD); XX(0xDE);
    }
    XX(0xAA); XX(0xAA); XX(0xAA); XX(0xAA);
#   undef XX
    // The initial CFA.
    initialCFA = TaggedUWord(0x5432ABCD);
    // The initial register state.
    memset(&regs, 0, sizeof(regs));
    regs.TESTED_REG_STRUCT_NAME = TaggedUWord(0x14141356);
  }

  StackImage  si;
  TaggedUWord initialCFA;
  UnwindRegs  regs;
};

class LulDwarfEvaluatePfxExpr : public EvaluatePfxExprFixture, public Test { };

TEST_F(LulDwarfEvaluatePfxExpr, NormalEvaluation) {
  vector<PfxInstr> instrs;
  // Put some junk at the start of the insn sequence.
  instrs.push_back(PfxInstr(PX_End));
  instrs.push_back(PfxInstr(PX_End));

  // Now the real sequence
  // stack is empty
  instrs.push_back(PfxInstr(PX_Start, 1));
  // 0x5432ABCD
  instrs.push_back(PfxInstr(PX_SImm32, 0x31415927));
  // 0x5432ABCD 0x31415927
  instrs.push_back(PfxInstr(PX_DwReg, TESTED_REG_DWARF_NAME));
  // 0x5432ABCD 0x31415927 0x14141356
  instrs.push_back(PfxInstr(PX_SImm32, 42));
  // 0x5432ABCD 0x31415927 0x14141356 42
  instrs.push_back(PfxInstr(PX_Sub));
  // 0x5432ABCD 0x31415927 0x1414132c
  instrs.push_back(PfxInstr(PX_Add));
  // 0x5432ABCD 0x45556c53
  instrs.push_back(PfxInstr(PX_SImm32, si.mStartAvma + 4));
  // 0x5432ABCD 0x45556c53 0x1234567c
  instrs.push_back(PfxInstr(PX_Deref));
  // 0x5432ABCD 0x45556c53 0xdeadbeef
  instrs.push_back(PfxInstr(PX_SImm32, 0xFE01DC23));
  // 0x5432ABCD 0x45556c53 0xdeadbeef 0xFE01DC23
  instrs.push_back(PfxInstr(PX_And));
  // 0x5432ABCD 0x45556c53 0xde019c23
  instrs.push_back(PfxInstr(PX_SImm32, 7));
  // 0x5432ABCD 0x45556c53 0xde019c23 7
  instrs.push_back(PfxInstr(PX_Shl));
  // 0x5432ABCD 0x45556c53 0x6f00ce1180
  instrs.push_back(PfxInstr(PX_SImm32, 0x7fffffff));
  // 0x5432ABCD 0x45556c53 0x6f00ce1180 7fffffff
  instrs.push_back(PfxInstr(PX_And));
  // 0x5432ABCD 0x45556c53 0x00ce1180
  instrs.push_back(PfxInstr(PX_Add));
  // 0x5432ABCD 0x46237dd3
  instrs.push_back(PfxInstr(PX_Sub));
  // 0xe0f2dfa

  instrs.push_back(PfxInstr(PX_End));

  TaggedUWord res = EvaluatePfxExpr(2/*offset of start insn*/,
                                    &regs, initialCFA, &si, instrs);
  EXPECT_TRUE(res.Valid());
  EXPECT_TRUE(res.Value() == 0xe0f2dfa);
}

TEST_F(LulDwarfEvaluatePfxExpr, EmptySequence) {
  vector<PfxInstr> instrs;
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_FALSE(res.Valid());
}

TEST_F(LulDwarfEvaluatePfxExpr, BogusStartPoint) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_SImm32, 42));
  instrs.push_back(PfxInstr(PX_SImm32, 24));
  instrs.push_back(PfxInstr(PX_SImm32, 4224));
  TaggedUWord res = EvaluatePfxExpr(1, &regs, initialCFA, &si, instrs);
  EXPECT_FALSE(res.Valid());
}

TEST_F(LulDwarfEvaluatePfxExpr, MissingEndMarker) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 0));
  instrs.push_back(PfxInstr(PX_SImm32, 24));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_FALSE(res.Valid());
}

TEST_F(LulDwarfEvaluatePfxExpr, StackUnderflow) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 0));
  instrs.push_back(PfxInstr(PX_End));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_FALSE(res.Valid());
}

TEST_F(LulDwarfEvaluatePfxExpr, StackNoUnderflow) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 1/*push the initial CFA*/));
  instrs.push_back(PfxInstr(PX_End));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_TRUE(res.Valid());
  EXPECT_TRUE(res == initialCFA);
}

TEST_F(LulDwarfEvaluatePfxExpr, StackOverflow) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 0));
  for (int i = 0; i < 10+1; i++) {
     instrs.push_back(PfxInstr(PX_SImm32, i + 100));
  }
  instrs.push_back(PfxInstr(PX_End));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_FALSE(res.Valid());
}

TEST_F(LulDwarfEvaluatePfxExpr, StackNoOverflow) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 0));
  for (int i = 0; i < 10+0; i++) {
     instrs.push_back(PfxInstr(PX_SImm32, i + 100));
  }
  instrs.push_back(PfxInstr(PX_End));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_TRUE(res.Valid());
  EXPECT_TRUE(res == TaggedUWord(109));
}

TEST_F(LulDwarfEvaluatePfxExpr, OutOfRangeShl) {
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 0));
  instrs.push_back(PfxInstr(PX_SImm32, 1234));
  instrs.push_back(PfxInstr(PX_SImm32, 5678));
  instrs.push_back(PfxInstr(PX_Shl));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_TRUE(!res.Valid());
}

TEST_F(LulDwarfEvaluatePfxExpr, TestCmpGES) {
  const int32_t argsL[6] = { 0, 0, 1, -2, -1, -2 };
  const int32_t argsR[6] = { 0, 1, 0, -2, -2, -1 };
  // expecting:              t  f  t  t   t    f   = 101110 = 0x2E
  vector<PfxInstr> instrs;
  instrs.push_back(PfxInstr(PX_Start, 0));
  // The "running total"
  instrs.push_back(PfxInstr(PX_SImm32, 0));
  for (unsigned int i = 0; i < sizeof(argsL)/sizeof(argsL[0]); i++) {
     // Shift the "running total" at the bottom of the stack left by one bit
     instrs.push_back(PfxInstr(PX_SImm32, 1));
     instrs.push_back(PfxInstr(PX_Shl));
     // Push both test args and do the comparison
     instrs.push_back(PfxInstr(PX_SImm32, argsL[i]));
     instrs.push_back(PfxInstr(PX_SImm32, argsR[i]));
     instrs.push_back(PfxInstr(PX_CmpGES));
     // Or the result into the running total
     instrs.push_back(PfxInstr(PX_Or));
  }
  instrs.push_back(PfxInstr(PX_End));
  TaggedUWord res = EvaluatePfxExpr(0, &regs, initialCFA, &si, instrs);
  EXPECT_TRUE(res.Valid());
  EXPECT_TRUE(res == TaggedUWord(0x2E));
}

} // namespace lul
