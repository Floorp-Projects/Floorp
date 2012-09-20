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

// NetworkInterface is an abstract interface for network connections.
// Its purpose is to make the network portion of certain classes
// easier to mock for testing. A concrete implementation of this
// interface can be found in udp_network.h.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_NETWORK_INTERFACE_H_
#define GOOGLE_BREAKPAD_PROCESSOR_NETWORK_INTERFACE_H_
namespace google_breakpad {

class NetworkInterface {
 public:
  // Prepare a network connection.
  // If listen is true, prepare the socket to listen for incoming
  // connections.
  // Returns true for success, false for failure.
  virtual bool Init(bool listen) = 0;

  // Send length bytes of data to the current address.
  // Returns true for success, false for failure.
  virtual bool Send(const char *data, size_t length) = 0;

  // Wait at most timeout milliseconds, returning when data is available or
  // time has expired.
  // Returns true if data is available, false if a timeout or error occurred.
  virtual bool WaitToReceive(int timeout) = 0;

  // Read data into buffer. received will contain the number of bytes received.
  // Returns true for success, false for failure.
  virtual bool Receive(char *buffer, size_t buffer_size, ssize_t &received) = 0;
};

}  // namespace google_breakpad
#endif  // GOOGLE_BREAKPAD_PROCESSOR_NETWORK_INTERFACE_H_
