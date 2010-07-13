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

// Unit tests for NetworkSourceLineServer.

#include <ios>
#include <set>
#include <string>

#include "breakpad_googletest_includes.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/source_line_resolver_interface.h"
#include "google_breakpad/processor/stack_frame.h"
#include "google_breakpad/processor/symbol_supplier.h"
#include "processor/binarystream.h"
#include "processor/cfi_frame_info.h"
#include "processor/network_source_line_server.h"
#include "processor/network_source_line_protocol.h"
#include "processor/windows_frame_info.h"

namespace {
using std::ios_base;
using std::set;
using std::string;
using google_breakpad::CFIFrameInfo;
using google_breakpad::CodeModule;
using google_breakpad::binarystream;
using google_breakpad::NetworkInterface;
using google_breakpad::NetworkSourceLineServer;
using google_breakpad::SourceLineResolverInterface;
using google_breakpad::StackFrame;
using google_breakpad::SymbolSupplier;
using google_breakpad::SystemInfo;
using google_breakpad::WindowsFrameInfo;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::SetArgumentPointee;
// Style guide forbids "using namespace", so at least shorten it.
namespace P = google_breakpad::source_line_protocol;

class MockNetwork : public NetworkInterface {
 public:
  MockNetwork() {}

  MOCK_METHOD1(Init, bool(bool listen));
  MOCK_METHOD2(Send, bool(const char *data, size_t length));
  MOCK_METHOD1(WaitToReceive, bool(int timeout));
  MOCK_METHOD3(Receive, bool(char *buffer, size_t buffer_size,
                             ssize_t &received));
};

class MockSymbolSupplier : public SymbolSupplier {
public:
  MockSymbolSupplier() {}

  MOCK_METHOD3(GetSymbolFile, SymbolResult(const CodeModule *module,
                                           const SystemInfo *system_info,
                                           string *symbol_file));
  MOCK_METHOD4(GetSymbolFile, SymbolResult(const CodeModule *module,
                                           const SystemInfo *system_info,
                                           string *symbol_file,
                                           string *symbol_data));
};

class MockSourceLineResolver : public SourceLineResolverInterface {
 public:
  MockSourceLineResolver() {}
  virtual ~MockSourceLineResolver() {}

  MOCK_METHOD2(LoadModule, bool(const CodeModule *module,
                                const string &map_file));
  MOCK_METHOD2(LoadModuleUsingMapBuffer, bool(const CodeModule *module,
                                              const string &map_buffer));
  MOCK_METHOD1(UnloadModule, void(const CodeModule *module));
  MOCK_METHOD1(HasModule, bool(const CodeModule *module));
  MOCK_METHOD1(FillSourceLineInfo, void(StackFrame *frame));
  MOCK_METHOD1(FindWindowsFrameInfo,
               WindowsFrameInfo*(const StackFrame *frame));
  MOCK_METHOD1(FindCFIFrameInfo,
               CFIFrameInfo*(const StackFrame *frame));
};

class TestNetworkSourceLineServer : public NetworkSourceLineServer {
 public:
  // Override visibility for testing. It's a lot easier to just
  // call into this method and verify the result than it would be
  // to mock out the calls to the NetworkInterface, even though
  // that would ostensibly be more correct and test the code more
  // thoroughly. Perhaps if someone has time and figures out a
  // clean way to do it this could be changed.
  using NetworkSourceLineServer::HandleRequest;

  TestNetworkSourceLineServer(SymbolSupplier *supplier,
                              SourceLineResolverInterface *resolver,
                              NetworkInterface *net,
                              u_int64_t max_symbol_lines = 0)
    : NetworkSourceLineServer(supplier, resolver, net, max_symbol_lines)

  {}
};

class NetworkSourceLineServerTest : public ::testing::Test {
 public:
  MockSymbolSupplier supplier;
  MockSourceLineResolver resolver;
  MockNetwork net;
  TestNetworkSourceLineServer *server;

  NetworkSourceLineServerTest() : server(NULL) {}

  void SetUp() {
    server = new TestNetworkSourceLineServer(&supplier, &resolver, &net);
  }
};

TEST_F(NetworkSourceLineServerTest, TestInit) {
  EXPECT_CALL(net, Init(true)).WillOnce(Return(true));
  EXPECT_CALL(net, WaitToReceive(0)).WillOnce(Return(false));
  ASSERT_TRUE(server->Initialize());
  EXPECT_FALSE(server->RunOnce(0));
}

TEST_F(NetworkSourceLineServerTest, TestMalformedRequest) {
  binarystream request;
  // send a request without a full sequence number
  request << u_int8_t(1);
  binarystream response;
  EXPECT_FALSE(server->HandleRequest(request, response));
  request.rewind();
  // send a request without a command
  request << u_int16_t(1);
  EXPECT_FALSE(server->HandleRequest(request, response));
}

TEST_F(NetworkSourceLineServerTest, TestUnknownCommand) {
  binarystream request;
  // send a request with an unknown command
  request << u_int16_t(1) << u_int8_t(100);
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status;
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(u_int16_t(1), response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
}

TEST_F(NetworkSourceLineServerTest, TestHasBasic) {
  EXPECT_CALL(resolver, HasModule(_))
    .WillOnce(Return(false))
    .WillOnce(Return(true));

  binarystream request;
  const u_int16_t sequence = 0xA0A0;
  // first request should come back as not loaded
  request << sequence << P::HAS << string("test.dll") << string("test.pdb")
          << string("ABCD1234");
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(P::MODULE_NOT_LOADED, int(response_data));
  // second request should come back as loaded
  binarystream request2;
  request2 << sequence << P::HAS << string("loaded.dll") << string("loaded.pdb")
           << string("ABCD1234");
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(P::MODULE_LOADED, int(response_data));
}

TEST_F(NetworkSourceLineServerTest, TestMalformedHasRequest) {
  binarystream request;
  // send request with just command, missing all data
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::HAS;
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status;
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
  // send request with just module name
  binarystream request2;
  request2 << sequence << P::HAS << string("test.dll");
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
  // send request with module name, debug file, missing debug id
  binarystream request3;
  request3 << sequence << P::HAS << string("test.dll") << string("test.pdb");
  ASSERT_TRUE(server->HandleRequest(request3, response));
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
}

TEST_F(NetworkSourceLineServerTest, TestHasLoad) {
  EXPECT_CALL(resolver, HasModule(_))
    .WillOnce(Return(false))
    .WillOnce(Return(false))
    .WillOnce(Return(true));
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .WillOnce(Return(true));
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .WillOnce(Return(SymbolSupplier::FOUND));

  // verify that the module is not loaded, with a HAS request
  binarystream request;
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::HAS << string("found.dll") << string("found.pdb")
          << string("ABCD1234");
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(P::MODULE_NOT_LOADED, int(response_data));
  // now send a load request for this module
  binarystream request2;
  const u_int16_t sequence2 = 0xB0B0;
  request2 << sequence2 << P::LOAD << string("found.dll") << string("found.pdb")
           << string("ABCD1234");
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));
  // sending another HAS message should now show it as loaded
  binarystream request3;
  const u_int16_t sequence3 = 0xC0C0;
  request3 << sequence3 << P::HAS << string("found.dll") << string("found.pdb")
           << string("ABCD1234");
  ASSERT_TRUE(server->HandleRequest(request3, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(P::MODULE_LOADED, int(response_data));
}

TEST_F(NetworkSourceLineServerTest, TestLoad) {
  EXPECT_CALL(resolver, HasModule(_))
    .Times(3)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .WillOnce(Return(false));
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .WillOnce(Return(SymbolSupplier::NOT_FOUND))
    .WillOnce(Return(SymbolSupplier::INTERRUPT))
    .WillOnce(Return(SymbolSupplier::FOUND));

  // notfound.dll should return LOAD_NOT_FOUND
  binarystream request;
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::LOAD << string("notfound.dll")
          << string("notfound.pdb") << string("ABCD1234");
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(int(P::LOAD_NOT_FOUND), int(response_data));
  // interrupt.dll should return LOAD_INTERRUPT
  binarystream request2;
  const u_int16_t sequence2 = 0xB0B0;
  request2 << sequence2 << P::LOAD << string("interrupt.dll")
           << string("interrupt.pdb") << string("0000");
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(int(P::LOAD_INTERRUPT), int(response_data));
  // fail.dll should return LOAD_FAIL
  binarystream request3;
  const u_int16_t sequence3 = 0xC0C0;
  request3 << sequence3 << P::LOAD << string("fail.dll") << string("fail.pdb")
           << string("FFFFFFFF");
  ASSERT_TRUE(server->HandleRequest(request3, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(int(P::LOAD_FAIL), int(response_data));
}

TEST_F(NetworkSourceLineServerTest, TestMalformedLoadRequest) {
  binarystream request;
  // send request with just command, missing all data
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::LOAD;
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status;
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
  // send request with just module name
  binarystream request2;
  request2 << sequence << P::LOAD << string("test.dll");
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
  // send request with module name, debug file, missing debug id
  binarystream request3;
  request3 << sequence << P::LOAD << string("test.dll") << string("test.pdb");
  ASSERT_TRUE(server->HandleRequest(request3, response));
  response >> response_sequence >> response_status;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::ERROR, int(response_status));
}

void FillFullSourceLineInfo(StackFrame *frame) {
  frame->function_name = "function1";
  frame->function_base = 0x1200;
  frame->source_file_name = "function1.cc";
  frame->source_line = 1;
  frame->source_line_base = 0x1230;
}

void FillPartialSourceLineInfo(StackFrame *frame) {
  frame->function_name = "function2";
  frame->function_base = 0xFFF0;
}

TEST_F(NetworkSourceLineServerTest, TestGet) {
  EXPECT_CALL(resolver, FillSourceLineInfo(_))
    .WillOnce(Invoke(FillFullSourceLineInfo))
    .WillOnce(Invoke(FillPartialSourceLineInfo));

  binarystream request;
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::GET << string("loaded.dll")
          << string("loaded.pdb") << string("ABCD1234")
          << u_int64_t(0x1000) << u_int64_t(0x1234);
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status;
  string function, source_file;
  u_int32_t source_line;
  u_int64_t function_base, source_line_base;
  response >> response_sequence >> response_status >> function
           >> function_base >> source_file >> source_line >> source_line_base;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("function1", function);
  EXPECT_EQ(0x1200, function_base);
  EXPECT_EQ("function1.cc", source_file);
  EXPECT_EQ(1, source_line);
  EXPECT_EQ(0x1230, source_line_base);

  binarystream request2;
  const u_int16_t sequence2 = 0xC0C0;
  request2 << sequence2 << P::GET << string("loaded.dll")
           << string("loaded.pdb") << string("ABCD1234")
           << u_int64_t(0x1000) << u_int64_t(0xFFFF);
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status >> function
           >> function_base >> source_file >> source_line >> source_line_base;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("function2", function);
  EXPECT_EQ(0xFFF0, function_base);
  EXPECT_EQ("", source_file);
  EXPECT_EQ(0, source_line);
  EXPECT_EQ(0, source_line_base);
}

WindowsFrameInfo* GetFullWindowsFrameInfo(const StackFrame *frame) {
  // return frame info with program string
  return new WindowsFrameInfo(1, 2, 3, 0xA, 0xFF, 0xF00,
                              true,
                              "x y =");
}

WindowsFrameInfo* GetPartialWindowsFrameInfo(const StackFrame *frame) {
  // return frame info, no program string
  return new WindowsFrameInfo(1, 2, 3, 4, 5, 6, true, "");
}

TEST_F(NetworkSourceLineServerTest, TestGetStackWin) {
  EXPECT_CALL(resolver, FindWindowsFrameInfo(_))
    .WillOnce(Invoke(GetFullWindowsFrameInfo))
    .WillOnce(Invoke(GetPartialWindowsFrameInfo))
    .WillOnce(Return((WindowsFrameInfo*)NULL));

  binarystream request;
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::GETSTACKWIN << string("loaded.dll")
          << string("loaded.pdb") << string("ABCD1234")
          << u_int64_t(0x1000) << u_int64_t(0x1234);
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status;
  string stack_info;
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("0 0 0 1 2 3 a ff f00 1 x y =", stack_info);

  binarystream request2;
  const u_int16_t sequence2 = 0xB0B0;
  request2 << sequence2 << P::GETSTACKWIN << string("loaded.dll")
           << string("loaded.pdb") << string("ABCD1234")
           << u_int64_t(0x1000) << u_int64_t(0xABCD);
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("0 0 0 1 2 3 4 5 6 0 1", stack_info);

  binarystream request3;
  const u_int16_t sequence3 = 0xC0C0;
  request3 << sequence3 << P::GETSTACKWIN << string("loaded.dll")
           << string("loaded.pdb") << string("ABCD1234")
           << u_int64_t(0x1000) << u_int64_t(0xFFFF);
  ASSERT_TRUE(server->HandleRequest(request3, response));
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("", stack_info);
}


CFIFrameInfo* GetCFIFrameInfoJustCFA(const StackFrame *frame) {
  CFIFrameInfo* cfi = new CFIFrameInfo();
  cfi->SetCFARule("12345678");
  return cfi;
}

CFIFrameInfo* GetCFIFrameInfoCFARA(const StackFrame *frame) {
  CFIFrameInfo* cfi = new CFIFrameInfo();
  cfi->SetCFARule("12345678");
  cfi->SetRARule("abcdefgh");
  return cfi;
}

CFIFrameInfo* GetCFIFrameInfoLots(const StackFrame *frame) {
  CFIFrameInfo* cfi = new CFIFrameInfo();
  cfi->SetCFARule("12345678");
  cfi->SetRARule("abcdefgh");
  cfi->SetRegisterRule("r0", "foo bar");
  cfi->SetRegisterRule("b0", "123 abc +");
  return cfi;
}

TEST_F(NetworkSourceLineServerTest, TestGetStackCFI) {
  EXPECT_CALL(resolver, FindCFIFrameInfo(_))
    .WillOnce(Return((CFIFrameInfo*)NULL))
    .WillOnce(Invoke(GetCFIFrameInfoJustCFA))
    .WillOnce(Invoke(GetCFIFrameInfoCFARA))
    .WillOnce(Invoke(GetCFIFrameInfoLots));

  binarystream request;
  const u_int16_t sequence = 0xA0A0;
  request << sequence << P::GETSTACKCFI << string("loaded.dll")
          << string("loaded.pdb") << string("ABCD1234")
          << u_int64_t(0x1000) << u_int64_t(0x1234);
  binarystream response;
  ASSERT_TRUE(server->HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status;
  string stack_info;
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("", stack_info);
  
  binarystream request2;
  const u_int16_t sequence2 = 0xB0B0;
  request2 << sequence2 << P::GETSTACKCFI << string("loaded.dll")
           << string("loaded.pdb") << string("ABCD1234")
           << u_int64_t(0x1000) << u_int64_t(0xABCD);
  ASSERT_TRUE(server->HandleRequest(request2, response));
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(".cfa: 12345678", stack_info);

  binarystream request3;
  const u_int16_t sequence3 = 0xC0C0;
  request3 << sequence3 << P::GETSTACKCFI << string("loaded.dll")
           << string("loaded.pdb") << string("ABCD1234")
           << u_int64_t(0x1000) << u_int64_t(0xFFFF);
  ASSERT_TRUE(server->HandleRequest(request3, response));
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(".cfa: 12345678 .ra: abcdefgh", stack_info);

  binarystream request4;
  const u_int16_t sequence4 = 0xD0D0;
  request4 << sequence4 << P::GETSTACKCFI << string("loaded.dll")
           << string("loaded.pdb") << string("ABCD1234")
           << u_int64_t(0x1000) << u_int64_t(0xFFFF);
  ASSERT_TRUE(server->HandleRequest(request4, response));
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence4, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ(".cfa: 12345678 .ra: abcdefgh b0: 123 abc + r0: foo bar",
            stack_info);
}

TEST_F(NetworkSourceLineServerTest, TestMalformedGetRequest) {
  //TODO
}

TEST(TestMissingMembers, TestServerWithoutSymbolSupplier) {
  // Should provide reasonable responses without a SymbolSupplier
  MockSourceLineResolver resolver;
  MockNetwork net;
  TestNetworkSourceLineServer server(NULL, &resolver, &net);
  
  // All LOAD requests should return LOAD_NOT_FOUND
  binarystream request;
  binarystream response;
  const u_int16_t sequence = 0xB0B0;
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  request << sequence << P::LOAD << string("found.dll") << string("found.pdb")
          << string("ABCD1234");
  ASSERT_TRUE(server.HandleRequest(request, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_NOT_FOUND), int(response_data));
}

TEST(TestMissingMembers, TestServerWithoutResolver) {
  // Should provide reasonable responses without a SourceLineResolver
  MockSymbolSupplier supplier;
  MockNetwork net;
  TestNetworkSourceLineServer server(&supplier, NULL, &net);

  // GET requests should return empty info
  binarystream request;
  binarystream response;
  const u_int16_t sequence = 0xA0A0;
  u_int16_t response_sequence;
  u_int8_t response_status;
  request << sequence << P::GET << string("loaded.dll")
          << string("loaded.pdb") << string("ABCD1234")
          << u_int64_t(0x1000) << u_int64_t(0x1234);
  ASSERT_TRUE(server.HandleRequest(request, response));
  string function, source_file;
  u_int32_t source_line;
  u_int64_t function_base, source_line_base;
  response >> response_sequence >> response_status >> function
           >> function_base >> source_file >> source_line >> source_line_base;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("", function);
  EXPECT_EQ(0x0, function_base);
  EXPECT_EQ("", source_file);
  EXPECT_EQ(0, source_line);
  EXPECT_EQ(0x0, source_line_base);

  // GETSTACKWIN requests should return an empty string
  binarystream request2;
  const u_int16_t sequence2 = 0xB0B0;
  request << sequence2 << P::GETSTACKWIN << string("loaded.dll")
          << string("loaded.pdb") << string("ABCD1234")
          << u_int64_t(0x1000) << u_int64_t(0x1234);
  ASSERT_TRUE(server.HandleRequest(request, response));
  string response_string;
  response >> response_sequence >> response_status >> response_string;
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  EXPECT_EQ("", response_string);
}

class TestModuleManagement  : public ::testing::Test {
public:
  MockSymbolSupplier supplier;
  MockSourceLineResolver resolver;
  MockNetwork net;
  TestNetworkSourceLineServer server;

  // Init server with symbol line limit of 25
  TestModuleManagement() : server(&supplier, &resolver, &net, 25) {}
};

TEST_F(TestModuleManagement, TestModuleUnloading) {
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .Times(3)
    .WillRepeatedly(DoAll(SetArgumentPointee<3>(string("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n")),
                          Return(SymbolSupplier::FOUND)));
  EXPECT_CALL(resolver, HasModule(_))
    .Times(3)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .Times(3)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(resolver, UnloadModule(Property(&CodeModule::code_file,
                                              string("one.dll|one.pdb|1111"))))
    .Times(1);

  // load three modules, each with 10 lines of symbols.
  // the third module will overflow the server's symbol line limit,
  // and should cause the first module to be unloaded.
  binarystream request;
  const u_int16_t sequence = 0x1010;
  request << sequence << P::LOAD << string("one.dll") << string("one.pdb")
          << string("1111");
  binarystream response;
  ASSERT_TRUE(server.HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request2;
  const u_int16_t sequence2 = 0x2020;
  request2 << sequence2 << P::LOAD << string("two.dll") << string("two.pdb")
           << string("2222");
  ASSERT_TRUE(server.HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request3;
  const u_int16_t sequence3 = 0x3030;
  request3 << sequence3 << P::LOAD << string("three.dll") << string("three.pdb")
           << string("3333");
  ASSERT_TRUE(server.HandleRequest(request3, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));
}

TEST_F(TestModuleManagement, TestSymbolLimitTooLow) {
  // load module with symbol count > limit,
  // ensure that it doesn't get unloaded even though it's the only module
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .WillOnce(DoAll(SetArgumentPointee<3>(string("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n")),
                          Return(SymbolSupplier::FOUND)));
  EXPECT_CALL(resolver, HasModule(_))
    .WillOnce(Return(false));
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .WillOnce(Return(true));
  EXPECT_CALL(resolver, UnloadModule(_))
    .Times(0);

  binarystream request;
  const u_int16_t sequence = 0x1010;
  request << sequence << P::LOAD << string("one.dll") << string("one.pdb")
          << string("1111");
  binarystream response;
  ASSERT_TRUE(server.HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));
}

TEST_F(TestModuleManagement, TestModuleLoadLRU) {
  // load 2 modules, then re-load the first one,
  // then load a third one, causing the second one to be unloaded
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .Times(3)
    .WillRepeatedly(DoAll(SetArgumentPointee<3>(string("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n")),
                          Return(SymbolSupplier::FOUND)));
  EXPECT_CALL(resolver, HasModule(_))
    .WillOnce(Return(false))  // load module 1
    .WillOnce(Return(false))  // load module 2
    .WillOnce(Return(true))   // module 1 already loaded
    .WillOnce(Return(false)); // load module 3
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .Times(3)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(resolver, UnloadModule(Property(&CodeModule::code_file,
                                              string("two.dll|two.pdb|2222"))))
    .Times(1);

  binarystream request;
  const u_int16_t sequence = 0x1010;
  request << sequence << P::LOAD << string("one.dll") << string("one.pdb")
          << string("1111");
  binarystream response;
  ASSERT_TRUE(server.HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request2;
  const u_int16_t sequence2 = 0x2020;
  request2 << sequence2 << P::LOAD << string("two.dll") << string("two.pdb")
           << string("2222");
  ASSERT_TRUE(server.HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request3;
  const u_int16_t sequence3 = 0x3030;
  request3 << sequence3 << P::LOAD << string("one.dll") << string("one.pdb")
           << string("1111");
  ASSERT_TRUE(server.HandleRequest(request3, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request4;
  const u_int16_t sequence4 = 0x4040;
  request4 << sequence4 << P::LOAD << string("three.dll") << string("three.pdb")
           << string("3333");
  ASSERT_TRUE(server.HandleRequest(request4, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence4, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));
}

TEST_F(TestModuleManagement, TestModuleGetLRU) {
  // load 2 modules, then issue a GET for the first one,
  // then load a third one, causing the second one to be unloaded
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .Times(3)
    .WillRepeatedly(DoAll(SetArgumentPointee<3>(string("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n")),
                          Return(SymbolSupplier::FOUND)));
  EXPECT_CALL(resolver, HasModule(_))
    .Times(3)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .Times(3)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(resolver, FillSourceLineInfo(_))
    .Times(1);
  EXPECT_CALL(resolver, UnloadModule(Property(&CodeModule::code_file,
                                              string("two.dll|two.pdb|2222"))))
    .Times(1);

  binarystream request;
  const u_int16_t sequence = 0x1010;
  request << sequence << P::LOAD << string("one.dll") << string("one.pdb")
          << string("1111");
  binarystream response;
  ASSERT_TRUE(server.HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request2;
  const u_int16_t sequence2 = 0x2020;
  request2 << sequence2 << P::LOAD << string("two.dll") << string("two.pdb")
           << string("2222");
  ASSERT_TRUE(server.HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request3;
  const u_int16_t sequence3 = 0x3030;
  request3 << sequence3 << P::GET << string("one.dll")
           << string("one.pdb") << string("1111")
           << u_int64_t(0x1000) << u_int64_t(0x1234);
  ASSERT_TRUE(server.HandleRequest(request3, response));
  string function, source_file;
  u_int32_t source_line;
  u_int64_t function_base, source_line_base;
  response >> response_sequence >> response_status >> function
           >> function_base >> source_file >> source_line >> source_line_base;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  // Don't care about the rest of the response, really.

  binarystream request4;
  const u_int16_t sequence4 = 0x4040;
  request4 << sequence4 << P::LOAD << string("three.dll") << string("three.pdb")
           << string("3333");
  ASSERT_TRUE(server.HandleRequest(request4, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence4, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));
}

TEST_F(TestModuleManagement, TestModuleGetStackWinLRU) {
  // load 2 modules, then issue a GETSTACKWIN for the first one,
  // then load a third one, causing the second one to be unloaded
  EXPECT_CALL(supplier, GetSymbolFile(_,_,_,_))
    .Times(3)
    .WillRepeatedly(DoAll(SetArgumentPointee<3>(string("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n")),
                          Return(SymbolSupplier::FOUND)));
  EXPECT_CALL(resolver, HasModule(_))
    .Times(3)
    .WillRepeatedly(Return(false));
  EXPECT_CALL(resolver, LoadModuleUsingMapBuffer(_,_))
    .Times(3)
    .WillRepeatedly(Return(true));
  EXPECT_CALL(resolver, FindWindowsFrameInfo(_))
    .WillOnce(Return((WindowsFrameInfo*)NULL));
  EXPECT_CALL(resolver, UnloadModule(Property(&CodeModule::code_file,
                                              string("two.dll|two.pdb|2222"))))
    .Times(1);

  binarystream request;
  const u_int16_t sequence = 0x1010;
  request << sequence << P::LOAD << string("one.dll") << string("one.pdb")
          << string("1111");
  binarystream response;
  ASSERT_TRUE(server.HandleRequest(request, response));
  u_int16_t response_sequence;
  u_int8_t response_status, response_data;
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request2;
  const u_int16_t sequence2 = 0x2020;
  request2 << sequence2 << P::LOAD << string("two.dll") << string("two.pdb")
           << string("2222");
  ASSERT_TRUE(server.HandleRequest(request2, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence2, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));

  binarystream request3;
  const u_int16_t sequence3 = 0x3030;
  request3 << sequence3 << P::GETSTACKWIN << string("one.dll")
          << string("one.pdb") << string("1111")
          << u_int64_t(0x1000) << u_int64_t(0x1234);
  ASSERT_TRUE(server.HandleRequest(request3, response));
  string stack_info;
  response >> response_sequence >> response_status
           >> stack_info;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence3, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  // Don't care about the rest of the response, really.

  binarystream request4;
  const u_int16_t sequence4 = 0x4040;
  request4 << sequence4 << P::LOAD << string("three.dll") << string("three.pdb")
           << string("3333");
  ASSERT_TRUE(server.HandleRequest(request4, response));
  response >> response_sequence >> response_status >> response_data;
  ASSERT_FALSE(response.eof());
  EXPECT_EQ(sequence4, response_sequence);
  EXPECT_EQ(P::OK, int(response_status));
  ASSERT_EQ(int(P::LOAD_OK), int(response_data));
}

}  // namespace

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
