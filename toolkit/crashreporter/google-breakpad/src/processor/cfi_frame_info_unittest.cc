// Copyright (c) 2010, Google Inc.
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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// cfi_frame_info_unittest.cc: Unit tests for CFIFrameInfo,
// CFIRuleParser, CFIFrameInfoParseHandler, and SimpleCFIWalker.

#include <string.h>

#include "breakpad_googletest_includes.h"
#include "common/module.h"
#include "common/using_std_string.h"
#include "processor/cfi_frame_info.h"
#include "google_breakpad/processor/memory_region.h"

using google_breakpad::CFIFrameInfo;
using google_breakpad::CFIFrameInfoParseHandler;
using google_breakpad::CFIRuleParser;
using google_breakpad::FromUniqueString;
using google_breakpad::MemoryRegion;
using google_breakpad::Module;
using google_breakpad::SimpleCFIWalker;
using google_breakpad::ToUniqueString;
using google_breakpad::UniqueString;
using google_breakpad::ustr__ZDcfa;
using google_breakpad::ustr__ZDra;
using testing::_;
using testing::A;
using testing::AtMost;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

class MockMemoryRegion: public MemoryRegion {
 public:
  MOCK_CONST_METHOD0(GetBase, uint64_t());
  MOCK_CONST_METHOD0(GetSize, uint32_t());
  MOCK_CONST_METHOD2(GetMemoryAtAddress, bool(uint64_t, uint8_t *));
  MOCK_CONST_METHOD2(GetMemoryAtAddress, bool(uint64_t, uint16_t *));
  MOCK_CONST_METHOD2(GetMemoryAtAddress, bool(uint64_t, uint32_t *));
  MOCK_CONST_METHOD2(GetMemoryAtAddress, bool(uint64_t, uint64_t *));
};

// Handy definitions for all tests.
struct CFIFixture {

  // Set up the mock memory object to expect no references.
  void ExpectNoMemoryReferences() {
    EXPECT_CALL(memory, GetBase()).Times(0);
    EXPECT_CALL(memory, GetSize()).Times(0);
    EXPECT_CALL(memory, GetMemoryAtAddress(_, A<uint8_t *>())).Times(0);
    EXPECT_CALL(memory, GetMemoryAtAddress(_, A<uint16_t *>())).Times(0);
    EXPECT_CALL(memory, GetMemoryAtAddress(_, A<uint32_t *>())).Times(0);
    EXPECT_CALL(memory, GetMemoryAtAddress(_, A<uint64_t *>())).Times(0);
  }

  CFIFrameInfo cfi;
  MockMemoryRegion memory;
  CFIFrameInfo::RegisterValueMap<uint64_t> registers, caller_registers;
};

class Simple: public CFIFixture, public Test { };

// FindCallerRegs should fail if no .cfa rule is provided.
TEST_F(Simple, NoCFA) {
  ExpectNoMemoryReferences();

  cfi.SetRARule(Module::Expr("0"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
  ASSERT_EQ(".ra: 0", cfi.Serialize());
}

// FindCallerRegs should fail if no .ra rule is provided.
TEST_F(Simple, NoRA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("0"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
  ASSERT_EQ(".cfa: 0", cfi.Serialize());
}

TEST_F(Simple, SetCFAAndRARule) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("330903416631436410"));
  cfi.SetRARule(Module::Expr("5870666104170902211"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(330903416631436410ULL, caller_registers.get(ustr__ZDcfa()));
  ASSERT_EQ(5870666104170902211ULL, caller_registers.get(ustr__ZDra()));

  ASSERT_EQ(".cfa: 330903416631436410 .ra: 5870666104170902211",
            cfi.Serialize());
}

TEST_F(Simple, SetManyRules) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("$temp1 68737028 = $temp2 61072337 = $temp1 $temp2 -"));
  cfi.SetRARule(Module::Expr(".cfa 99804755 +"));

  const UniqueString* reg1 = ToUniqueString("register1");
  const UniqueString* reg2 = ToUniqueString("vodkathumbscrewingly");
  const UniqueString* reg3 = ToUniqueString("pubvexingfjordschmaltzy");
  const UniqueString* reg4 = ToUniqueString("uncopyrightables");

  cfi.SetRegisterRule(reg1, Module::Expr(".cfa 54370437 *"));
  cfi.SetRegisterRule(reg2, Module::Expr("24076308 .cfa +"));
  cfi.SetRegisterRule(reg3, Module::Expr(".cfa 29801007 -"));
  cfi.SetRegisterRule(reg4, Module::Expr("92642917 .cfa /"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(7664691U,           caller_registers.get(ustr__ZDcfa()));
  ASSERT_EQ(107469446U,         caller_registers.get(ustr__ZDra()));
  ASSERT_EQ(416732599139967ULL, caller_registers.get(reg1));
  ASSERT_EQ(31740999U,          caller_registers.get(reg2));
  ASSERT_EQ(-22136316ULL,       caller_registers.get(reg3));
  ASSERT_EQ(12U,                caller_registers.get(reg4));
  ASSERT_EQ(".cfa: $temp1 68737028 = $temp2 61072337 = $temp1 $temp2 - "
            ".ra: .cfa 99804755 + "
            "pubvexingfjordschmaltzy: .cfa 29801007 - "
            "register1: .cfa 54370437 * "
            "uncopyrightables: 92642917 .cfa / "
            "vodkathumbscrewingly: 24076308 .cfa +",
            cfi.Serialize());
}

TEST_F(Simple, RulesOverride) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("330903416631436410"));
  cfi.SetRARule(Module::Expr("5870666104170902211"));
  cfi.SetCFARule(Module::Expr("2828089117179001"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(2828089117179001ULL, caller_registers.get(ustr__ZDcfa()));
  ASSERT_EQ(5870666104170902211ULL, caller_registers.get(ustr__ZDra()));
  ASSERT_EQ(".cfa: 2828089117179001 .ra: 5870666104170902211",
            cfi.Serialize());
}

class Scope: public CFIFixture, public Test { };

// There should be no value for .cfa in scope when evaluating the CFA rule.
TEST_F(Scope, CFALacksCFA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr(".cfa"));
  cfi.SetRARule(Module::Expr("0"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
}

// There should be no value for .ra in scope when evaluating the CFA rule.
TEST_F(Scope, CFALacksRA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr(".ra"));
  cfi.SetRARule(Module::Expr("0"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
}

// The current frame's registers should be in scope when evaluating
// the CFA rule.
TEST_F(Scope, CFASeesCurrentRegs) {
  ExpectNoMemoryReferences();

  const UniqueString* reg1 = ToUniqueString(".baraminology");
  const UniqueString* reg2 = ToUniqueString(".ornithorhynchus");
  registers.set(reg1, 0x06a7bc63e4f13893ULL);
  registers.set(reg2, 0x5e0bf850bafce9d2ULL);
  cfi.SetCFARule(Module::Expr(".baraminology .ornithorhynchus +"));
  cfi.SetRARule(Module::Expr("0"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(0x06a7bc63e4f13893ULL + 0x5e0bf850bafce9d2ULL,
            caller_registers.get(ustr__ZDcfa()));
}

// .cfa should be in scope in the return address expression.
TEST_F(Scope, RASeesCFA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("48364076"));
  cfi.SetRARule(Module::Expr(".cfa"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(48364076U, caller_registers.get(ustr__ZDra()));
}

// There should be no value for .ra in scope when evaluating the CFA rule.
TEST_F(Scope, RALacksRA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("0"));
  cfi.SetRARule(Module::Expr(".ra"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
}

// The current frame's registers should be in scope in the return
// address expression.
TEST_F(Scope, RASeesCurrentRegs) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("10359370"));
  const UniqueString* reg1 = ToUniqueString("noachian");
  registers.set(reg1, 0x54dc4a5d8e5eb503ULL);
  cfi.SetRARule(Module::Expr(reg1, 0, false));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(0x54dc4a5d8e5eb503ULL, caller_registers.get(ustr__ZDra()));
}

// .cfa should be in scope for register rules.
TEST_F(Scope, RegistersSeeCFA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("6515179"));
  cfi.SetRARule(Module::Expr(".cfa"));
  const UniqueString* reg1 = ToUniqueString("rogerian");
  cfi.SetRegisterRule(reg1, Module::Expr(".cfa"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(6515179U, caller_registers.get(reg1));
}

// The return address should not be in scope for register rules.
TEST_F(Scope, RegsLackRA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("42740329"));
  cfi.SetRARule(Module::Expr("27045204"));
  const UniqueString* reg1 = ToUniqueString("$r1");
  cfi.SetRegisterRule(reg1, Module::Expr(".ra"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
}

// Register rules can see the current frame's register values.
TEST_F(Scope, RegsSeeRegs) {
  ExpectNoMemoryReferences();

  const UniqueString* reg1 = ToUniqueString("$r1");
  const UniqueString* reg2 = ToUniqueString("$r2");
  registers.set(reg1, 0x6ed3582c4bedb9adULL);
  registers.set(reg2, 0xd27d9e742b8df6d0ULL);
  cfi.SetCFARule(Module::Expr("88239303"));
  cfi.SetRARule(Module::Expr("30503835"));
  cfi.SetRegisterRule(reg1, Module::Expr("$r1 42175211 = $r2"));
  cfi.SetRegisterRule(reg2, Module::Expr("$r2 21357221 = $r1"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(0xd27d9e742b8df6d0ULL, caller_registers.get(reg1));
  ASSERT_EQ(0x6ed3582c4bedb9adULL, caller_registers.get(reg2));
}

// Each rule's temporaries are separate.
TEST_F(Scope, SeparateTempsRA) {
  ExpectNoMemoryReferences();

  cfi.SetCFARule(Module::Expr("$temp1 76569129 = $temp1"));
  cfi.SetRARule(Module::Expr("0"));
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));

  cfi.SetCFARule(Module::Expr("$temp1 76569129 = $temp1"));
  cfi.SetRARule(Module::Expr("$temp1"));
  ASSERT_FALSE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                             &caller_registers));
}

class MockCFIRuleParserHandler: public CFIRuleParser::Handler {
 public:
  MOCK_METHOD1(CFARule, void(const string &));
  MOCK_METHOD1(RARule,  void(const string &));
  MOCK_METHOD2(RegisterRule, void(const UniqueString*, const string &));
};

// A fixture class for testing CFIRuleParser.
class CFIParserFixture {
 public:
  CFIParserFixture() : parser(&mock_handler) {
    // Expect no parsing results to be reported to mock_handler. Individual
    // tests can override this.
    EXPECT_CALL(mock_handler, CFARule(_)).Times(0);
    EXPECT_CALL(mock_handler, RARule(_)).Times(0);
    EXPECT_CALL(mock_handler, RegisterRule(_, _)).Times(0);
  }

  MockCFIRuleParserHandler mock_handler;
  CFIRuleParser parser;
};

class Parser: public CFIParserFixture, public Test { };

TEST_F(Parser, Empty) {
  EXPECT_FALSE(parser.Parse(""));
}

TEST_F(Parser, LoneColon) {
  EXPECT_FALSE(parser.Parse(":"));
}

TEST_F(Parser, CFANoExpr) {
  EXPECT_FALSE(parser.Parse(".cfa:"));
}

TEST_F(Parser, CFANoColonNoExpr) {
  EXPECT_FALSE(parser.Parse(".cfa"));
}

TEST_F(Parser, RANoExpr) {
  EXPECT_FALSE(parser.Parse(".ra:"));
}

TEST_F(Parser, RANoColonNoExpr) {
  EXPECT_FALSE(parser.Parse(".ra"));
}

TEST_F(Parser, RegNoExpr) {
  EXPECT_FALSE(parser.Parse("reg:"));
}

TEST_F(Parser, NoName) {
  EXPECT_FALSE(parser.Parse("expr"));
}

TEST_F(Parser, NoNameTwo) {
  EXPECT_FALSE(parser.Parse("expr1 expr2"));
}

TEST_F(Parser, StartsWithExpr) {
  EXPECT_FALSE(parser.Parse("expr1 reg: expr2"));
}

TEST_F(Parser, CFA) {
  EXPECT_CALL(mock_handler, CFARule("spleen")).WillOnce(Return());
  EXPECT_TRUE(parser.Parse(".cfa: spleen"));
}

TEST_F(Parser, RA) {
  EXPECT_CALL(mock_handler, RARule("notoriety")).WillOnce(Return());
  EXPECT_TRUE(parser.Parse(".ra: notoriety"));
}

TEST_F(Parser, Reg) {
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("nemo"), "mellifluous"))
      .WillOnce(Return());
  EXPECT_TRUE(parser.Parse("nemo: mellifluous"));
}

TEST_F(Parser, CFARARegs) {
  EXPECT_CALL(mock_handler, CFARule("cfa expression")).WillOnce(Return());
  EXPECT_CALL(mock_handler, RARule("ra expression")).WillOnce(Return());
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("galba"), "praetorian"))
      .WillOnce(Return());
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("otho"), "vitellius"))
      .WillOnce(Return());
  EXPECT_TRUE(parser.Parse(".cfa: cfa expression .ra: ra expression "
                    "galba: praetorian otho: vitellius"));
}

TEST_F(Parser, Whitespace) {
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("r1"), "r1 expression"))
      .WillOnce(Return());
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("r2"), "r2 expression"))
      .WillOnce(Return());
  EXPECT_TRUE(parser.Parse(" r1:\tr1\nexpression \tr2:\t\rr2\r\n "
                           "expression  \n"));
}

TEST_F(Parser, WhitespaceLoneColon) {
  EXPECT_FALSE(parser.Parse("  \n:\t  "));
}

TEST_F(Parser, EmptyName) {
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("reg"), _))
      .Times(AtMost(1))
      .WillRepeatedly(Return());
  EXPECT_FALSE(parser.Parse("reg: expr1 : expr2"));
}

TEST_F(Parser, RuleLoneColon) {
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("r1"), "expr"))
      .Times(AtMost(1))
      .WillRepeatedly(Return());
  EXPECT_FALSE(parser.Parse(" r1:   expr   :"));
}

TEST_F(Parser, RegNoExprRule) {
  EXPECT_CALL(mock_handler, RegisterRule(ToUniqueString("r1"), "expr"))
      .Times(AtMost(1))
      .WillRepeatedly(Return());
  EXPECT_FALSE(parser.Parse("r0: r1:   expr"));
}

class ParseHandlerFixture: public CFIFixture {
 public:
  ParseHandlerFixture() : CFIFixture(), handler(&cfi) { }
  CFIFrameInfoParseHandler handler;
};

class ParseHandler: public ParseHandlerFixture, public Test { };

TEST_F(ParseHandler, CFARARule) {
  handler.CFARule("reg-for-cfa");
  handler.RARule("reg-for-ra");
  registers.set(ToUniqueString("reg-for-cfa"), 0x268a9a4a3821a797ULL);
  registers.set(ToUniqueString("reg-for-ra"), 0x6301b475b8b91c02ULL);
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(0x268a9a4a3821a797ULL, caller_registers.get(ustr__ZDcfa()));
  ASSERT_EQ(0x6301b475b8b91c02ULL, caller_registers.get(ustr__ZDra()));
}

TEST_F(ParseHandler, RegisterRules) {
  handler.CFARule("reg-for-cfa");
  handler.RARule("reg-for-ra");
  handler.RegisterRule(ToUniqueString("reg1"), "reg-for-reg1");
  handler.RegisterRule(ToUniqueString("reg2"), "reg-for-reg2");
  registers.set(ToUniqueString("reg-for-cfa"), 0x268a9a4a3821a797ULL);
  registers.set(ToUniqueString("reg-for-ra"), 0x6301b475b8b91c02ULL);
  registers.set(ToUniqueString("reg-for-reg1"), 0x06cde8e2ff062481ULL);
  registers.set(ToUniqueString("reg-for-reg2"), 0xff0c4f76403173e2ULL);
  ASSERT_TRUE(cfi.FindCallerRegs<uint64_t>(registers, memory,
                                            &caller_registers));
  ASSERT_EQ(0x268a9a4a3821a797ULL, caller_registers.get(ustr__ZDcfa()));
  ASSERT_EQ(0x6301b475b8b91c02ULL, caller_registers.get(ustr__ZDra()));
  ASSERT_EQ(0x06cde8e2ff062481ULL, caller_registers.get(ToUniqueString("reg1")));
  ASSERT_EQ(0xff0c4f76403173e2ULL, caller_registers.get(ToUniqueString("reg2")));
}

struct SimpleCFIWalkerFixture {
  struct RawContext {
    uint64_t r0, r1, r2, r3, r4, sp, pc;
  };
  enum Validity {
    R0_VALID = 0x01,
    R1_VALID = 0x02,
    R2_VALID = 0x04,
    R3_VALID = 0x08,
    R4_VALID = 0x10,
    SP_VALID = 0x20,
    PC_VALID = 0x40
  };
  typedef SimpleCFIWalker<uint64_t, RawContext> CFIWalker;

  SimpleCFIWalkerFixture()
      : walker(register_map,
               sizeof(register_map) / sizeof(register_map[0])) { }

  static CFIWalker::RegisterSet register_map[7];
  CFIFrameInfo call_frame_info;
  CFIWalker walker;
  MockMemoryRegion memory;
  RawContext callee_context, caller_context;
};

SimpleCFIWalkerFixture::CFIWalker::RegisterSet
SimpleCFIWalkerFixture::register_map[7] = {
  { ToUniqueString("r0"), NULL,   true,  R0_VALID, &RawContext::r0 },
  { ToUniqueString("r1"), NULL,   true,  R1_VALID, &RawContext::r1 },
  { ToUniqueString("r2"), NULL,   false, R2_VALID, &RawContext::r2 },
  { ToUniqueString("r3"), NULL,   false, R3_VALID, &RawContext::r3 },
  { ToUniqueString("r4"), NULL,   true,  R4_VALID, &RawContext::r4 },
  { ToUniqueString("sp"), ustr__ZDcfa(), true,  SP_VALID, &RawContext::sp },
  { ToUniqueString("pc"), ustr__ZDra(),  true,  PC_VALID, &RawContext::pc },
};

class SimpleWalker: public SimpleCFIWalkerFixture, public Test { };

TEST_F(SimpleWalker, Walk) {
  // Stack_top is the current stack pointer, pointing to the lowest
  // address of a frame that looks like this (all 64-bit words):
  //
  // sp ->  saved r0
  //        garbage
  //        return address
  // cfa -> 
  //
  // r0 has been saved on the stack.
  // r1 has been saved in r2.
  // r2 and r3 are not recoverable.
  // r4 is not recoverable, even though it is a callee-saves register.
  //    Some earlier frame's unwinder must have failed to recover it.

  uint64_t stack_top = 0x83254944b20d5512ULL;

  // Saved r0.
  EXPECT_CALL(memory,
              GetMemoryAtAddress(stack_top, A<uint64_t *>()))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(0xdc1975eba8602302ULL),
                            Return(true)));
  // Saved return address.
  EXPECT_CALL(memory,
              GetMemoryAtAddress(stack_top + 16, A<uint64_t *>()))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(0xba5ad6d9acce28deULL),
                            Return(true)));

  call_frame_info.SetCFARule(Module::Expr("sp 24 +"));
  call_frame_info.SetRARule(Module::Expr(".cfa 8 - ^"));
  call_frame_info.SetRegisterRule(ToUniqueString("r0"),
                                  Module::Expr(".cfa 24 - ^"));
  call_frame_info.SetRegisterRule(ToUniqueString("r1"),
                                  Module::Expr("r2"));

  callee_context.r0 = 0x94e030ca79edd119ULL;
  callee_context.r1 = 0x937b4d7e95ce52d9ULL;
  callee_context.r2 = 0x5fe0027416b8b62aULL; // caller's r1
  // callee_context.r3 is not valid in callee.
  // callee_context.r4 is not valid in callee.
  callee_context.sp = stack_top;
  callee_context.pc = 0x25b21b224311d280ULL;
  int callee_validity = R0_VALID | R1_VALID | R2_VALID | SP_VALID | PC_VALID;

  memset(&caller_context, 0, sizeof(caller_context));

  int caller_validity;
  EXPECT_TRUE(walker.FindCallerRegisters(memory, call_frame_info,
                                         callee_context, callee_validity,
                                         &caller_context, &caller_validity));
  EXPECT_EQ(R0_VALID | R1_VALID | SP_VALID | PC_VALID, caller_validity);
  EXPECT_EQ(0xdc1975eba8602302ULL, caller_context.r0);
  EXPECT_EQ(0x5fe0027416b8b62aULL, caller_context.r1);
  EXPECT_EQ(stack_top + 24,        caller_context.sp);
  EXPECT_EQ(0xba5ad6d9acce28deULL, caller_context.pc);
}
