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

// NetworkSourceLineServer implements a UDP-based network protocol
// to allow clients to query for source line information.
//
// A brief protocol description can be found in network_source_line_protocol.h

#ifndef GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_SERVER_H_
#define GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_SERVER_H_

#include <list>
#include <string>
#include <vector>

#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/symbol_supplier.h"
#include "processor/binarystream.h"
#include "processor/network_interface.h"
#include "processor/udp_network.h"

namespace google_breakpad {

using std::list;
using std::string;
using std::vector;

class NetworkSourceLineServer {
 public:
  explicit NetworkSourceLineServer(SymbolSupplier *supplier,
                                   SourceLineResolverInterface *resolver,
                                   unsigned short listen_port,
                                   bool ip4only,
                                   const string &listen_address,
                                   u_int64_t max_symbol_lines)
    : initialized_(false),
      net_(new UDPNetwork(listen_address, listen_port, ip4only)),
      resolver_(resolver),
      supplier_(supplier),
      max_symbol_lines_(max_symbol_lines),
      symbol_lines_(0) {};

  NetworkSourceLineServer(SymbolSupplier *supplier,
                          SourceLineResolverInterface *resolver,
                          NetworkInterface *net,
                          u_int64_t max_symbol_lines = 0)
    : initialized_(false),
      net_(net),
      resolver_(resolver),
      supplier_(supplier),
      max_symbol_lines_(max_symbol_lines),
      symbol_lines_(0) {};

  // Initialize network connection. Will be called automatically by
  // RunOnce or RunForever if not already initialized.
  // Returns false if network setup fails.
  bool Initialize();

  // Run forever serving connections.
  // Returns false only if network setup fails.
  bool RunForever();

  // Look for incoming connections and serve them.
  // Wait at most |wait_milliseconds| before returning. Return true
  // if any connections were served, and false otherwise.
  bool RunOnce(int wait_milliseconds);

 private:
  bool initialized_;
  NetworkInterface *net_;
  SourceLineResolverInterface *resolver_;
  SymbolSupplier *supplier_;
  // Maximum number of symbol lines to store in memory.
  // The number of lines in a symbol file is used as a rough
  // proxy for memory usage when parsed and loaded. When
  // this limit is surpassed, modules will be unloaded until
  // the sum of currently loaded modules is again lower
  // than this limit.
  const u_int64_t max_symbol_lines_;
  // Current number of symbol lines loaded
  u_int64_t symbol_lines_;
  // List of modules loaded, in most-to-least recently used order
  list<string> modules_used_;
  // Number of symbol lines loaded, per module.
  map<string, int> module_symbol_lines_;

  void HandleHas(binarystream &message, binarystream &response);
  void HandleLoad(binarystream &message, binarystream &response);
  void HandleGet(binarystream &message, binarystream &response);
  void HandleGetStackWin(binarystream &message, binarystream &response);
  void HandleGetStackCFI(binarystream &message, binarystream &response);
  string FormatWindowsFrameInfo(WindowsFrameInfo *frame_info);

  int CountNewlines(const string &str);

  // Move this module to the front of the used list.
  void UsedModule(const CodeModule &module);
  // Try to unload some modules to reclaim memory.
  // Do not unload the module passed in, as this was just loaded.
  void TryUnloadModules(const CodeModule &just_loaded_module);

 protected:
  // protected so we can easily unit test it
  bool HandleRequest(binarystream &request, binarystream &response);
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_SERVER_H_
