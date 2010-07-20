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

// NetworkSourceLineResolver implements SourceLineResolverInterface and
// SymbolSupplier using a UDP-based network protocol to communicate to a
// server process which handles the lower-level details of loading symbols
// and resolving source info. When used, it must be used simultaneously
// as the SourceLineResolver and SymbolSupplier.
//
// See network_source_line_server.h for a description of the protocol used.
// An implementation of the server side of the protocol is provided there
// as NetworkSourceLineServer.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_RESOLVER_H_
#define GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_RESOLVER_H_

#include <sys/socket.h>

#include <map>
#include <set>

#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/source_line_resolver_interface.h"
#include "google_breakpad/processor/stack_frame.h"
#include "google_breakpad/processor/symbol_supplier.h"
#include "processor/binarystream.h"
#include "processor/linked_ptr.h"
#include "processor/network_interface.h"

namespace google_breakpad {

using std::string;

class NetworkSourceLineResolver : public SourceLineResolverInterface,
                                  public SymbolSupplier {
 public:
  // The server and port to connect to, and the
  // maximum time (in milliseconds) to wait for network replies.
  NetworkSourceLineResolver(const string &server,
                            unsigned short port,
                            int wait_milliseconds);
  // The network interface to connect to, and maximum wait time.
  NetworkSourceLineResolver(NetworkInterface *net,
                            int wait_milliseconds);
  virtual ~NetworkSourceLineResolver();
  
  // SourceLineResolverInterface methods, see source_line_resolver_interface.h
  // for more details.


  // These methods are actually NOOPs in this implementation.
  // The server loads modules as a result of the GetSymbolFile call.
  // Since we're both the symbol supplier and source line resolver,
  // this is an optimization.
  virtual bool LoadModule(const CodeModule *module, const string &map_file);
  virtual bool LoadModuleUsingMapBuffer(const CodeModule *module,
                                        const string &map_buffer);

  void UnloadModule(const CodeModule *module);

  virtual bool HasModule(const CodeModule *module);

  virtual void FillSourceLineInfo(StackFrame *frame);
  virtual WindowsFrameInfo *FindWindowsFrameInfo(const StackFrame *frame);
  virtual CFIFrameInfo *FindCFIFrameInfo(const StackFrame *frame);

  // SymbolSupplier methods, see symbol_supplier.h for more details.
  // Note that the server will actually load the symbol data
  // in response to this request, as an optimization.
  virtual SymbolResult GetSymbolFile(const CodeModule *module,
                                     const SystemInfo *system_info,
                                     string *symbol_file);
  //FIXME: we'll never return symbol_data here, it doesn't make sense.
  // the SymbolSupplier interface should just state that the supplier
  // *may* fill in symbol_data if it desires, and clients should
  // handle it gracefully either way.
  virtual SymbolResult GetSymbolFile(const CodeModule *module,
                                     const SystemInfo *system_info,
                                     string *symbol_file,
                                     string *symbol_data);

 private:
  int wait_milliseconds_;
  // if false, some part of our network setup failed.
  bool initialized_;
  // sequence number of the last request we made
  u_int16_t sequence_;
  NetworkInterface *net_;
  // cached list of loaded modules, so we can quickly answer
  // HasModule requests for modules we've already queried the
  // server about, avoiding another network round-trip.
  std::set<string> module_cache_;
  // cached list of modules for which we don't have symbols,
  // so we can short-circuit that as well.
  std::set<string> no_symbols_cache_;

  // Cached list of source line info, to avoid repeated GET requests
  // for the same frame. In Multithreaded apps that use the same
  // framework across threads, it's pretty common to hit the same
  // exact set of frames in multiple threads.
  // Data is stored in the cache keyed by instruction pointer
  typedef std::map<u_int64_t, StackFrame> SourceCache;
  SourceCache source_line_info_cache_;

  // Cached list of WindowsFrameInfo/CFIFrameInfo, for the same reason.
  // Stored as serialized strings to avoid shuffling around pointers.
  typedef std::map<u_int64_t, string> FrameInfoCache;

  typedef enum {
    kWindowsFrameInfo = 0,
    kCFIFrameInfo = 1,
  } FrameInfoType;
  FrameInfoCache frame_info_cache_[2];
  
  // Send a message to the server, wait a certain amount of time for a reply.
  // Returns true if a response is received, with the response data
  // in |response|.
  // Returns false if the response times out.
  bool SendMessageGetResponse(const binarystream &message,
                              binarystream &response);

  // See if this stack frame is cached, and fill in the source line info
  // if so.
  bool FindCachedSourceLineInfo(StackFrame *frame) const;
  bool FindCachedFrameInfo(const StackFrame *frame,
                           FrameInfoType type,
                           string *info) const;

  // Save this stack frame in the cache
  void CacheSourceLineInfo(const StackFrame *frame);
  void CacheFrameInfo(const StackFrame *frame,
                      FrameInfoType type,
                      const string &info);

  // Disallow unwanted copy ctor and assignment operator
  NetworkSourceLineResolver(const NetworkSourceLineResolver&);
  void operator=(const NetworkSourceLineResolver&);
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_RESOLVER_H_
