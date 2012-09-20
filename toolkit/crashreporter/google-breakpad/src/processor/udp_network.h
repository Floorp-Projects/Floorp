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

// UDPNetwork implements NetworkInterface using UDP sockets.

#ifndef _GOOGLE_BREAKPAD_PROCESSOR_UDP_NETWORK_H_
#define _GOOGLE_BREAKPAD_PROCESSOR_UDP_NETWORK_H_

#include <sys/socket.h>

#include <string>

#include "processor/network_interface.h"

namespace google_breakpad {

class UDPNetwork : public NetworkInterface {
 public:
  // Initialize a UDP Network socket at this address and port.
  // address can be empty to indicate that any local address is acceptable.
  UDPNetwork(const std::string address,
             unsigned short port,
             bool ip4only = false)
    : server_(address),
      port_(port),
      ip4only_(ip4only),
      socket_(-1) {};

  ~UDPNetwork();

  virtual bool Init(bool listen);
  virtual bool Send(const char *data, size_t length);
  virtual bool WaitToReceive(int timeout);
  virtual bool Receive(char *buffer, size_t buffer_size, ssize_t &received);

  unsigned short port() { return port_; }

 private:
  std::string server_;
  unsigned short port_;
  bool ip4only_;
  struct sockaddr_storage address_;
  int socket_;
};

}  // namespace google_breakpad
#endif  // GOOGLE_BREAKPAD_PROCESSOR_UDP_NETWORK_H_
