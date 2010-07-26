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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "processor/udp_network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include <string>

#include "processor/logging.h"

namespace google_breakpad {
using std::string;
using std::dec;

UDPNetwork::~UDPNetwork() {
  if (socket_ != -1) {
    close(socket_);
    socket_ = -1;
  }
}

bool UDPNetwork::Init(bool listen) {
  struct addrinfo hints;
  struct addrinfo *results;
  memset(&hints, 0, sizeof(hints));
  if (ip4only_)
    hints.ai_family = AF_INET;
  else
    // don't care if it's IPv4 or IPv6
    hints.ai_family = AF_UNSPEC;
  // want a UDP socket
  hints.ai_socktype = SOCK_DGRAM;
  if (listen)
    hints.ai_flags = AI_PASSIVE;

  const char *hostname = NULL;
  if (!server_.empty())
    hostname = server_.c_str();
  char portname[6];
  sprintf(portname, "%u", port_);
  if (!listen) {
    BPLOG(INFO) << "Initializing network connection to " << server_
                << ":" << dec << port_;
  }
  if (getaddrinfo(hostname, portname, &hints, &results) != 0) {
    BPLOG(ERROR) << "failed to get address info for address " << server_
                 << ": " << strerror(errno);
    return false;
  }
  // save the address of the first result.
  //TODO(ted): could support multiple DNS entries, round-robin them for
  // fail-over etc
  memcpy(&address_, results->ai_addr, GET_SA_LEN(results->ai_addr));

  socket_ = socket(results->ai_family, results->ai_socktype,
		   results->ai_protocol);
  freeaddrinfo(results);

  if (socket_ == -1) {
    BPLOG(ERROR) << "failed to create socket: " << strerror(errno);
    return false;
  }

  if (listen) {
    char address_string[INET_ADDRSTRLEN];
    void *addr = NULL;
    if (((struct sockaddr*)&address_)->sa_family == AF_INET)
      addr = &((struct sockaddr_in*)&address_)->sin_addr;
    else if (((struct sockaddr*)&address_)->sa_family == AF_INET6)
      addr = &((struct sockaddr_in6*)&address_)->sin6_addr;
    if (inet_ntop(((struct sockaddr*)&address_)->sa_family, addr,
                  address_string, sizeof(address_string)) != NULL)
      BPLOG(INFO) << "Listening on address " << address_string;

    if (bind(socket_,
             (struct sockaddr *)&address_,
             GET_SA_LEN(address_)) == -1) {
      BPLOG(ERROR) << "Failed to bind socket";
      close(socket_);
      return false;
    }
    socklen_t bound_addr_len = GET_SA_LEN(address_);
    if (getsockname(socket_, (struct sockaddr *)&address_, &bound_addr_len)
        == 0) {
      if (((struct sockaddr*)&address_)->sa_family == AF_INET)
        port_ = ntohs(((struct sockaddr_in*)&address_)->sin_port);
      else if (((struct sockaddr*)&address_)->sa_family == AF_INET6)
        port_ = ntohs(((struct sockaddr_in6*)&address_)->sin6_port);
    }
    BPLOG(INFO) << "Listening on port " << port_;
  }
  return true;
}

bool UDPNetwork::Send(const char *data, size_t length) {
  int total_sent = 0;
  while (total_sent < length) {
    int bytes_sent = sendto(socket_,
                            data + total_sent,
                            length - total_sent,
                            0,
                            (struct sockaddr *)&address_,
                            GET_SA_LEN(address_));
    if (bytes_sent < 0) {
      BPLOG(ERROR) << "error sending message: "
                   << strerror(errno) << " (" << errno << ")";
      break;
    }
    total_sent += bytes_sent;
  }
  return total_sent == length;
}

bool UDPNetwork::WaitToReceive(int wait_time) {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(socket_, &readfds);
  struct timeval timeout;

  timeout.tv_sec = wait_time / 1000;
  timeout.tv_usec = (wait_time % 1000) * 1000;
  int ret = select(socket_+1, &readfds, NULL, NULL, &timeout);
  if (ret == 0) {
    return false;
  } else if (ret == -1) {
    if (errno != EINTR)
      BPLOG(ERROR) << "error in select(): " << strerror(errno);
    return false;
  } else if (!FD_ISSET(socket_, &readfds)) {
    BPLOG(ERROR) << "select returned, but our socket isn't ready?";
    return false;
  }

  return true;
}

bool UDPNetwork::Receive(char *buffer, size_t buffer_size, ssize_t &received) {
  socklen_t fromlen = GET_SA_LEN(address_);
  received = recvfrom(socket_, buffer, buffer_size, 0,
                      (struct sockaddr *)&address_,
                      &fromlen);

  if (received == -1) {
    BPLOG(ERROR) << "Error in recvfrom reading response: "
                 << strerror(errno);
  }
  return received != -1;
}

} // namespace google_breakpad
