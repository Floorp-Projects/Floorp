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

#include "google_breakpad/processor/network_source_line_resolver.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>
#include <vector>

#include "google_breakpad/processor/stack_frame.h"
#include "processor/binarystream.h"
#include "processor/cfi_frame_info.h"
#include "processor/network_interface.h"
#include "processor/network_source_line_protocol.h"
#include "processor/logging.h"
#include "processor/scoped_ptr.h"
#include "processor/udp_network.h"
#include "processor/windows_frame_info.h"

namespace google_breakpad {

using std::string;
using std::vector;
using std::dec;
using std::hex;
// Style guide forbids "using namespace", so at least shorten it.
namespace P = source_line_protocol;

NetworkSourceLineResolver::NetworkSourceLineResolver(const string &server,
						     unsigned short port,
						     int wait_milliseconds)
  : wait_milliseconds_(wait_milliseconds),
    initialized_(false),
    sequence_(0),
    net_(new UDPNetwork(server, port)) {
  if (net_->Init(false))
    initialized_ = true;
}

NetworkSourceLineResolver::NetworkSourceLineResolver(NetworkInterface *net,
                                                     int wait_milliseconds)
  : wait_milliseconds_(wait_milliseconds),
    initialized_(false),
    sequence_(0),
    net_(net) {
  if (net_ && net->Init(false))
    initialized_ = true;
}

NetworkSourceLineResolver::~NetworkSourceLineResolver() {
  initialized_ = false;
}

bool NetworkSourceLineResolver::LoadModule(const CodeModule *module,
					   const string &map_file) {
  // Just lie here and say it was loaded. The server always loads
  // symbols immediately when they're found, since clients always
  // will want to load them after finding them anyway. Since this class
  // acts as both the symbol supplier and source line resolver,
  // it's just a little optimization.
  return true;
}

bool NetworkSourceLineResolver::LoadModuleUsingMapBuffer(
    const CodeModule *module,
    const string &map_buffer) {
  // see above
  return true;
}

void NetworkSourceLineResolver::UnloadModule(const CodeModule *module) {
  // no-op
}

bool NetworkSourceLineResolver::HasModule(const CodeModule *module) {
  if (!initialized_ || !module)
    return false;

  // cache seen modules so the network round trip can be skipped
  if (module_cache_.find(module->code_file()) != module_cache_.end())
    return true;

  // also cache modules for which symbols aren't found
  if (no_symbols_cache_.find(module->debug_file() + module->debug_identifier())
      != no_symbols_cache_.end())
    return false;

  binarystream message;
  message << P::HAS
          << module->code_file()
          << module->debug_file()
          << module->debug_identifier();
  binarystream response;
  bool got_response = SendMessageGetResponse(message, response);
  u_int8_t response_data;
  response >> response_data;

  bool found = false;
  if (got_response && !response.eof() && response_data == P::MODULE_LOADED) {
    module_cache_.insert(module->code_file());
    found = true;
  }
  return found;
}

void NetworkSourceLineResolver::FillSourceLineInfo(
    StackFrame *frame) {
  if (!initialized_)
    return;

  // if don't this module isn't loaded, can't fill source line info
  if (!frame->module ||
      module_cache_.find(frame->module->code_file()) == module_cache_.end())
    return;

  // if this frame has already been seen, return the cached copy
  if (FindCachedSourceLineInfo(frame)) {
    BPLOG(INFO) << "Using cached source line info";
    return;
  }

  binarystream message;
  message << P::GET
          << frame->module->code_file()
	  << frame->module->debug_file()
	  << frame->module->debug_identifier()
	  << frame->module->base_address()
	  << frame->instruction;
  binarystream response;
  bool got_response = SendMessageGetResponse(message, response);
  if (!got_response)
    return;

  string function_name, source_file;
  u_int32_t source_line;
  u_int64_t function_base, source_line_base;
  response >> function_name >> function_base
           >> source_file >> source_line >> source_line_base;
  
  if (response.eof()) {
    BPLOG(ERROR) << "GET response malformed";
    return;
  } else {
    BPLOG(INFO) << "GET response: " << function_name << " "
                << hex << function_base << " " << source_file << " "
                << dec << source_line << " " << hex
                << source_line_base;
  }

  frame->function_name = function_name;
  frame->function_base = function_base;
  frame->source_file_name = source_file;
  frame->source_line = source_line;
  frame->source_line_base = source_line_base;

  CacheSourceLineInfo(frame);
}

WindowsFrameInfo*
NetworkSourceLineResolver::FindWindowsFrameInfo(const StackFrame *frame) {
  if (!initialized_)
    return NULL;

  // if this module isn't loaded, can't get frame info
  if (!frame->module ||
      module_cache_.find(frame->module->code_file()) == module_cache_.end())
    return NULL;

  // check the cache first
  string stack_info;
  
  if (FindCachedFrameInfo(frame, kWindowsFrameInfo, &stack_info)) {
    BPLOG(INFO) << "Using cached windows frame info";
  } else {
    binarystream message;
    message << P::GETSTACKWIN
            << frame->module->code_file()
            << frame->module->debug_file()
            << frame->module->debug_identifier()
            << frame->module->base_address()
            << frame->instruction;
    binarystream response;
    if (SendMessageGetResponse(message, response)) {
      response >> stack_info;
      CacheFrameInfo(frame, kWindowsFrameInfo, stack_info);
    }
  }

  WindowsFrameInfo *info = NULL;
  if (!stack_info.empty()) {
    int type;
    u_int64_t rva, code_size;
    info = WindowsFrameInfo::ParseFromString(stack_info,
                                             type,
                                             rva,
                                             code_size);
  }

  return info;
}

CFIFrameInfo*
NetworkSourceLineResolver::FindCFIFrameInfo(const StackFrame *frame)
{
  if (!initialized_)
    return NULL;

  // if this module isn't loaded, can't get frame info
  if (!frame->module ||
      module_cache_.find(frame->module->code_file()) == module_cache_.end())
    return NULL;

  string stack_info;

  if (FindCachedFrameInfo(frame, kCFIFrameInfo, &stack_info)) {
    BPLOG(INFO) << "Using cached CFI frame info";
  } else {
    binarystream message;
    message << P::GETSTACKCFI
            << frame->module->code_file()
            << frame->module->debug_file()
            << frame->module->debug_identifier()
            << frame->module->base_address()
            << frame->instruction;
    binarystream response;
    if (SendMessageGetResponse(message, response)) {
      response >> stack_info;
      CacheFrameInfo(frame, kCFIFrameInfo, stack_info);
    }
  }

  if (!stack_info.empty()) {
    scoped_ptr<CFIFrameInfo> info(new CFIFrameInfo());
    CFIFrameInfoParseHandler handler(info.get());
    CFIRuleParser parser(&handler);
    if (parser.Parse(stack_info))
      return info.release();
  }

  return NULL;
}

SymbolSupplier::SymbolResult
NetworkSourceLineResolver::GetSymbolFile(const CodeModule *module,
					 const SystemInfo *system_info,
					 string *symbol_file) {
  BPLOG_IF(ERROR, !symbol_file) << "NetworkSourceLineResolver::GetSymbolFile "
                                   "requires |symbol_file|";
  assert(symbol_file);

  if (!initialized_)
    return NOT_FOUND;

  if (no_symbols_cache_.find(module->debug_file() + module->debug_identifier())
      != no_symbols_cache_.end())
    return NOT_FOUND;

  binarystream message;
  message << P::LOAD
          << module->code_file()
          << module->debug_file()
          << module->debug_identifier();
  binarystream response;
  bool got_response = SendMessageGetResponse(message, response);
  if (!got_response) {
    // Didn't get a response, which is the same as not having symbols.
    // Don't cache this, though, to force a retry if the client asks for
    // symbols for the same file again.
    return NOT_FOUND;
  }
  u_int8_t response_data;
  response >> response_data;

  if (response.eof()) {
    BPLOG(ERROR) << "Malformed LOAD response";
    return NOT_FOUND;
  }

  if (response_data == P::LOAD_NOT_FOUND || response_data == P::LOAD_FAIL) {
    // Received NOT or FAIL, symbols not present or failed to load them.
    // Same problem to the client any way you look at it.
    // Cache this module to avoid pointless retry.
    no_symbols_cache_.insert(module->debug_file() + module->debug_identifier());
    return NOT_FOUND;
  } else if (response_data == P::LOAD_INTERRUPT) {
    return INTERRUPT;
  }

  // otherwise, OK
  module_cache_.insert(module->code_file());
  *symbol_file = "<loaded on server>";
  return FOUND;
}

SymbolSupplier::SymbolResult
NetworkSourceLineResolver::GetSymbolFile(const CodeModule *module,
					 const SystemInfo *system_info,
					 string *symbol_file,
					 string *symbol_data) {
  if(symbol_data)
    symbol_data->clear();
  return GetSymbolFile(module, system_info, symbol_file);
}

bool NetworkSourceLineResolver::SendMessageGetResponse(
    const binarystream &message,
    binarystream &response) {
  binarystream sequence_stream;
  u_int16_t sent_sequence = sequence_;
  sequence_stream << sequence_;
  ++sequence_;
  string message_string = sequence_stream.str();
  message_string.append(message.str());
  BPLOG(INFO) << "Sending " << message_string.length() << " bytes";
  if (!net_->Send(message_string.c_str(), message_string.length()))
    return false;

  bool done = false;
  while (!done) {
    if (!net_->WaitToReceive(wait_milliseconds_))
      return false;

    vector<char> buffer(1024);
    ssize_t received_bytes;
    if (!net_->Receive(&buffer[0], buffer.size(), received_bytes))
      return false;

    BPLOG(INFO) << "received " << received_bytes << " bytes";
    buffer.resize(received_bytes);

    response.str(string(&buffer[0], buffer.size()));
    response.rewind();
    u_int16_t read_sequence;
    u_int8_t  status;
    response >> read_sequence >> status;
    if (response.eof()) {
      BPLOG(ERROR) << "malformed response, missing sequence number or status";
      return false;
    }
    if (read_sequence < sent_sequence) // old packet
      continue;

    if (read_sequence != sent_sequence) {
      // not expecting this packet, just error
      BPLOG(ERROR) << "error, got sequence number " << read_sequence
		   << ", expected " << sent_sequence;
      return false;
    }

    // This is the expected packet, so even if it's an error this loop is done
    done = true;

    if (status != P::OK) {
      BPLOG(ERROR) << "received an ER response packet";
      return false;
    }
    // the caller will process the rest of response
  }
  return true;
}

bool NetworkSourceLineResolver::FindCachedSourceLineInfo(StackFrame *frame)
  const
{
  SourceCache::const_iterator iter =
    source_line_info_cache_.find(frame->instruction);
  if (iter == source_line_info_cache_.end())
    return false;

  const StackFrame &f = iter->second;
  frame->function_name = f.function_name;
  frame->function_base = f.function_base;
  frame->source_file_name = f.source_file_name;
  frame->source_line = f.source_line;
  frame->source_line_base = f.source_line_base;
  return true;
}

bool NetworkSourceLineResolver::FindCachedFrameInfo(
  const StackFrame *frame,
  FrameInfoType type,
  string *info) const
{
  FrameInfoCache::const_iterator iter =
    frame_info_cache_[type].find(frame->instruction);
  if (iter == frame_info_cache_[type].end())
    return false;

  *info = iter->second;
  return true;
}

void NetworkSourceLineResolver::CacheSourceLineInfo(const StackFrame *frame) {
  StackFrame f(*frame);
  // can't hang onto this pointer, the caller owns it
  f.module = NULL;
  source_line_info_cache_[frame->instruction] = f;
}

void NetworkSourceLineResolver::CacheFrameInfo(
  const StackFrame *frame,
  FrameInfoType type,
  const string &info) {
  frame_info_cache_[type][frame->instruction] = info;
}

} // namespace google_breakpad
