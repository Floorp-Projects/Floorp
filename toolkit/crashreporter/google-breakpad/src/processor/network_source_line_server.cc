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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <iostream>
#include <sstream>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/stack_frame.h"
#include "processor/basic_code_module.h"
#include "processor/binarystream.h"
#include "processor/cfi_frame_info.h"
#include "processor/logging.h"
#include "processor/network_source_line_protocol.h"
#include "processor/network_source_line_server.h"
#include "processor/tokenize.h"
#include "processor/windows_frame_info.h"

namespace google_breakpad {

using std::dec;
using std::find;
using std::hex;
// Style guide forbids "using namespace", so at least shorten it.
namespace P = source_line_protocol;

bool NetworkSourceLineServer::Initialize() {
  if (net_->Init(true))
    initialized_ = true;
  return initialized_;
}

bool NetworkSourceLineServer::RunForever() {
  if (!initialized_ && !Initialize())
    return false;

  BPLOG(INFO) << "Running forever...";
  while (true) {
    RunOnce(5000);
  }
  // not reached
  return true;
}

bool NetworkSourceLineServer::RunOnce(int wait_milliseconds) {
  if (!initialized_ && !Initialize())
    return false;

  if (!net_->WaitToReceive(wait_milliseconds))
    return false;
  //TODO(ted): loop, processing packets until wait_milliseconds
  // is actually exhausted?

  vector<char> buffer(1024);
  ssize_t received_bytes;
  if (!net_->Receive(&buffer[0], buffer.size(), received_bytes))
    return false;
  buffer.resize(received_bytes);

  binarystream request(&buffer[0], buffer.size());
  binarystream response;
  if (!HandleRequest(request, response))
    return false;

  string response_string = response.str();
  if (!net_->Send(response_string.c_str(), response_string.length()))
    return false;
  return true;
}

bool NetworkSourceLineServer::HandleRequest(binarystream &request,
                                            binarystream &response) {
  u_int16_t sequence_number;
  u_int8_t  command;
  request >> sequence_number >> command;
  if (request.eof()) {
    BPLOG(ERROR) << "Malformed request, missing sequence number or command";
    return false;
  }
  
  response.rewind();
  response << sequence_number;
  switch(command) {
  case P::HAS:
    HandleHas(request, response);
    break;
  case P::LOAD:
    HandleLoad(request, response);
    break;
  case P::GET:
    HandleGet(request, response);
    break;
  case P::GETSTACKWIN:
    HandleGetStackWin(request, response);
    break;
  case P::GETSTACKCFI:
    HandleGetStackCFI(request, response);
    break;
  default:
    BPLOG(ERROR) << "Unknown command " << int(command);
    response << P::ERROR;
    break;
  }
  return true;
}

void NetworkSourceLineServer::HandleHas(binarystream &message,
                                        binarystream &response) {
  string module_name, debug_file, debug_id;
  message >> module_name >> debug_file >> debug_id;
  if (message.eof()) {
    BPLOG(ERROR) << "HAS message malformed";
    response << P::ERROR;
    return;
  }
  BPLOG(INFO) << "Received message HAS " << module_name
              << " " << debug_file
              << " " << debug_id;
  // Need to lie about the module name here, since BasicSourceLineResolver
  // uses only the module name for unique modules, but we want to allow
  // multiple versions of the same named module in here.
  BasicCodeModule module((u_int64_t)0, (u_int64_t)0,
			 module_name + "|" + debug_file + "|" + debug_id, "",
			 debug_file, debug_id, "");
  u_int8_t data;
  if (resolver_) {
    data = resolver_->HasModule(&module)
      ? P::MODULE_LOADED : P::MODULE_NOT_LOADED;
  } else {
    data = P::MODULE_NOT_LOADED;
  }
  response << P::OK << data;
}

void NetworkSourceLineServer::HandleLoad(binarystream &message,
                                         binarystream &response) {
  string module_name, debug_file, debug_id;
  message >> module_name >> debug_file >> debug_id;
  if (message.eof()) {
    BPLOG(ERROR) << "LOAD message malformed";
    response << P::ERROR;
    return;
  }
  BPLOG(INFO) << "Received message LOAD " << module_name
              << " " << debug_file
              << " " << debug_id;

  u_int8_t reply;
  // stub out the bare minimum here
  BasicCodeModule module((u_int64_t)0, (u_int64_t)0,
                         module_name + "|" + debug_file + "|" + debug_id, "",
                         debug_file, debug_id, "");
  if (resolver_->HasModule(&module)) {
    // just short-circuit the rest of this, since it's already loaded
    BPLOG(INFO) << "Got LOAD for already loaded " << module_name;
    UsedModule(module);
    reply = P::LOAD_OK;
  } else {
    BPLOG(INFO) << "Looking up symbols for (" << module_name << ", "
                << debug_file << ", " << debug_id << ")";
    string symbol_data, symbol_file;
    SymbolSupplier::SymbolResult symbol_result;
    if (supplier_) {
      symbol_result = supplier_->GetSymbolFile(&module, NULL,
                                               &symbol_file, &symbol_data);
    } else {
      symbol_result = SymbolSupplier::NOT_FOUND;
    }

    switch (symbol_result) {
    case SymbolSupplier::FOUND: {
      BPLOG(INFO) << "Found symbols for " << module_name;
      reply = P::LOAD_OK;
      // also go ahead and load the symbols while we're here,
      // since the client is just going to ask us to do this right away
      // and we already have |symbol_data| here.
      int numlines = CountNewlines(symbol_data);
      if (!resolver_->LoadModuleUsingMapBuffer(&module,
                                               symbol_data)) {
        BPLOG(INFO) << "Failed to load symbols for " << module_name;
        reply = P::LOAD_FAIL;
      } else {
        // save some info about this module
        symbol_lines_ += numlines;
        UsedModule(module);
        module_symbol_lines_[module.code_file()] = numlines;

        BPLOG(INFO) << "Loaded symbols for " << module_name
                    << " (" << dec << numlines << " lines, "
                    << symbol_lines_ << " total)";

        if (max_symbol_lines_ != 0 && symbol_lines_ > max_symbol_lines_) {
          // try unloading some modules to reclaim memory
          // (but not the one that was just loaded)
          BPLOG(INFO) << "Exceeded limit of " << dec << max_symbol_lines_
                      << " symbol lines loaded, trying to unload modules";
          TryUnloadModules(module);
        }
      }
    }
      break;
    case SymbolSupplier::NOT_FOUND:
      BPLOG(INFO) << "Symbols not found for " << module_name;
      reply = P::LOAD_NOT_FOUND;
      break;
    case SymbolSupplier::INTERRUPT:
      BPLOG(INFO) << "Symbol provider returned interrupt for " << module_name;
      reply = P::LOAD_INTERRUPT;
      break;
    }
  }
  response << P::OK << reply;
}

void NetworkSourceLineServer::HandleGet(binarystream &message,
                                        binarystream &response) {
  string module_name, debug_file, debug_id;
  u_int64_t module_base, instruction;
  message >> module_name >> debug_file >> debug_id
          >> module_base >> instruction;
  if (message.eof()) {
    BPLOG(ERROR) << "GET message malformed";
    response << P::ERROR;
    return;
  }

  BPLOG(INFO) << "Received message GET " << module_name << " "
              << debug_file << " " << debug_id << " "
              << hex << module_base << " " << instruction;

  StackFrame frame;
  if (resolver_) {
    BasicCodeModule module(module_base, (u_int64_t)0,
                           module_name + "|" + debug_file + "|" + debug_id, "",
                           debug_file, debug_id, "");
    frame.module = &module;
    frame.instruction = instruction;
    resolver_->FillSourceLineInfo(&frame);
    UsedModule(module);
  }

  response << P::OK << frame.function_name << frame.function_base
           << frame.source_file_name << u_int32_t(frame.source_line)
           << frame.source_line_base;
  BPLOG(INFO) << "Sending GET response: " << frame.function_name << " "
              << hex << frame.function_base << " "
              << frame.source_file_name << " "
              << dec << frame.source_line << " "
              << hex << frame.source_line_base;
}

void NetworkSourceLineServer::HandleGetStackWin(binarystream &message,
                                                binarystream &response) {
  string module_name, debug_file, debug_id;
  u_int64_t module_base, instruction;
  message >> module_name >> debug_file >> debug_id
          >> module_base >> instruction;
  if (message.eof()) {
    BPLOG(ERROR) << "GETSTACKWIN message malformed";
    response << P::ERROR;
    return;
  }

  BPLOG(INFO) << "Received message GETSTACKWIN " << module_name << " "
              << debug_file << " " << debug_id << " "
              << hex << module_base << " " << instruction;


  WindowsFrameInfo *frame_info = NULL;
  if (resolver_) {
    StackFrame frame;
    BasicCodeModule module(module_base, (u_int64_t)0,
                           module_name + "|" + debug_file + "|" + debug_id, "",
                           debug_file, debug_id, "");
    frame.module = &module;
    frame.instruction = instruction;
    frame_info = resolver_->FindWindowsFrameInfo(&frame);
    UsedModule(module);
  }

  response << P::OK << FormatWindowsFrameInfo(frame_info);
  BPLOG(INFO) << "Sending GETSTACKWIN response: "
              << FormatWindowsFrameInfo(frame_info);
  delete frame_info;
}

string NetworkSourceLineServer::FormatWindowsFrameInfo(
    WindowsFrameInfo *frame_info) {
  if (frame_info == NULL)
    return "";

  std::ostringstream stream;
  // Put "0" as the type, rva and code size because the client doesn't
  // actually care what these values are, but it's easier to keep the
  // format consistent with the symbol files so the parsing code can be
  // shared.
  stream << "0 0 0 " << hex
	 << frame_info->prolog_size << " "
	 << frame_info->epilog_size << " "
	 << frame_info->parameter_size << " "
	 << frame_info->saved_register_size << " "
	 << frame_info->local_size << " "
	 << frame_info->max_stack_size << " ";
  if (!frame_info->program_string.empty()) {
    stream << 1 << " " << frame_info->program_string;
  } else {
    stream << 0 << " " << frame_info->allocates_base_pointer;
  }
  return stream.str();
}

void NetworkSourceLineServer::HandleGetStackCFI(binarystream &message,
                                                binarystream &response) {
  string module_name, debug_file, debug_id;
  u_int64_t module_base, instruction;
  message >> module_name >> debug_file >> debug_id
          >> module_base >> instruction;
  if (message.eof()) {
    BPLOG(ERROR) << "GETSTACKCFI message malformed";
    response << P::ERROR;
    return;
  }

  BPLOG(INFO) << "Received message GETSTACKCFI " << module_name << " "
              << debug_file << " " << debug_id << " "
              << hex << module_base << " " << instruction;


  CFIFrameInfo *frame_info = NULL;
  if (resolver_) {
    StackFrame frame;
    BasicCodeModule module(module_base, (u_int64_t)0,
                           module_name + "|" + debug_file + "|" + debug_id, "",
                           debug_file, debug_id, "");
    frame.module = &module;
    frame.instruction = instruction;
    frame_info = resolver_->FindCFIFrameInfo(&frame);
    UsedModule(module);
  }

  string frame_info_string;
  if (frame_info != NULL)
    frame_info_string = frame_info->Serialize();
  response << P::OK << frame_info_string;
  BPLOG(INFO) << "Sending GETSTACKCFI response: "
              << frame_info_string;
  delete frame_info;
}

int NetworkSourceLineServer::CountNewlines(const string &str) {
  int count = 0;
  string::const_iterator iter = str.begin();
  while (iter != str.end()) {
    if (*iter == '\n')
      count++;
    iter++;
  }
  return count;
}

void NetworkSourceLineServer::UsedModule(const CodeModule &module) {
  list<string>::iterator iter = find(modules_used_.begin(),
                                     modules_used_.end(),
                                     module.code_file());
  if (iter == modules_used_.end()) {
    modules_used_.push_front(module.code_file());
  } else {
    modules_used_.splice(modules_used_.begin(),
                         modules_used_,
                         iter);
  }
}

void NetworkSourceLineServer::TryUnloadModules(
  const CodeModule &just_loaded_module) {
  if (!resolver_)
    return;

  while (symbol_lines_ > max_symbol_lines_) {
    // never unload just_loaded_module
    if (modules_used_.back() == just_loaded_module.code_file())
      break;

    string module_to_unload = modules_used_.back();
    modules_used_.pop_back();
    BasicCodeModule module(0, 0, module_to_unload, "", "", "", "");
    BPLOG(INFO) << "Unloading module " << module_to_unload;
    resolver_->UnloadModule(&module);

    // reduce the symbol line count
    map<string, int>::iterator iter =
      module_symbol_lines_.find(module_to_unload);
    if (iter != module_symbol_lines_.end()) {
      symbol_lines_ -= iter->second;
      module_symbol_lines_.erase(iter);
    }
  }
}

}  // namespace google_breakpad
