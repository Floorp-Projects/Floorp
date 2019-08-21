/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 */

/*
Based partially on original code from nICEr and nrappkit.

nICEr copyright:

Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


nrappkit copyright:

   Copyright (C) 2001-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Thu Dec 20 20:14:49 2001
*/

// Original author: bcampen@mozilla.com [:bwc]

#ifndef test_nr_socket__
#define test_nr_socket__

extern "C" {
#include "transport_addr.h"
}

#include "nr_socket_prsock.h"

extern "C" {
#include "nr_socket.h"
}

#include <set>
#include <vector>
#include <map>
#include <list>
#include <string>

#include "mozilla/UniquePtr.h"
#include "prinrval.h"
#include "mediapacket.h"

namespace mozilla {

class TestNrSocket;

/**
 * A group of TestNrSockets that behave as if they were behind the same NAT.
 * @note We deliberately avoid addref/release of TestNrSocket here to avoid
 *       masking lifetime errors elsewhere.
 */
class TestNat {
 public:
  /**
   * This allows TestNat traffic to be passively inspected.
   * If a non-zero (error) value is returned, the packet will be dropped,
   * allowing for tests to extend how packet manipulation is done by
   * TestNat with having to modify TestNat itself.
   */
  class NatDelegate {
   public:
    virtual int on_read(TestNat* nat, void* buf, size_t maxlen,
                        size_t* len) = 0;
    virtual int on_sendto(TestNat* nat, const void* msg, size_t len, int flags,
                          nr_transport_addr* to) = 0;
    virtual int on_write(TestNat* nat, const void* msg, size_t len,
                         size_t* written) = 0;
  };

  typedef enum {
    /** For mapping, one port is used for all destinations.
     *  For filtering, allow any external address/port. */
    ENDPOINT_INDEPENDENT,

    /** For mapping, one port for each destination address (for any port).
     *  For filtering, allow incoming traffic from addresses that outgoing
     *  traffic has been sent to. */
    ADDRESS_DEPENDENT,

    /** For mapping, one port for each destination address/port.
     *  For filtering, allow incoming traffic only from addresses/ports that
     *  outgoing traffic has been sent to. */
    PORT_DEPENDENT,
  } NatBehavior;

  TestNat()
      : enabled_(false),
        filtering_type_(ENDPOINT_INDEPENDENT),
        mapping_type_(ENDPOINT_INDEPENDENT),
        mapping_timeout_(30000),
        allow_hairpinning_(false),
        refresh_on_ingress_(false),
        block_udp_(false),
        block_stun_(false),
        block_tcp_(false),
        delay_stun_resp_ms_(0),
        nat_delegate_(nullptr),
        sockets_() {}

  bool has_port_mappings() const;

  // Helps determine whether we're hairpinning
  bool is_my_external_tuple(const nr_transport_addr& addr) const;
  bool is_an_internal_tuple(const nr_transport_addr& addr) const;

  int create_socket_factory(nr_socket_factory** factorypp);

  void insert_socket(TestNrSocket* socket) { sockets_.insert(socket); }

  void erase_socket(TestNrSocket* socket) { sockets_.erase(socket); }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestNat);

  static NatBehavior ToNatBehavior(const std::string& type);

  bool enabled_;
  TestNat::NatBehavior filtering_type_;
  TestNat::NatBehavior mapping_type_;
  uint32_t mapping_timeout_;
  bool allow_hairpinning_;
  bool refresh_on_ingress_;
  bool block_udp_;
  bool block_stun_;
  bool block_tcp_;
  /* Note: this can only delay a single response so far (bug 1253657) */
  uint32_t delay_stun_resp_ms_;

  NatDelegate* nat_delegate_;

 private:
  std::set<TestNrSocket*> sockets_;

  ~TestNat() {}
};

/**
 * Subclass of NrSocketBase that can simulate things like being behind a NAT,
 * packet loss, latency, packet rewriting, etc. Also exposes some stuff that
 * assists in diagnostics.
 * This is accomplished by wrapping an "internal" socket (that handles traffic
 * behind the NAT), and a collection of "external" sockets (that handle traffic
 * into/out of the NAT)
 */
class TestNrSocket : public NrSocketBase {
 public:
  explicit TestNrSocket(TestNat* nat);

  bool has_port_mappings() const;
  bool is_my_external_tuple(const nr_transport_addr& addr) const;

  // Overrides of NrSocketBase
  int create(nr_transport_addr* addr) override;
  int sendto(const void* msg, size_t len, int flags,
             nr_transport_addr* to) override;
  int recvfrom(void* buf, size_t maxlen, size_t* len, int flags,
               nr_transport_addr* from) override;
  int getaddr(nr_transport_addr* addrp) override;
  void close() override;
  int connect(nr_transport_addr* addr) override;
  int write(const void* msg, size_t len, size_t* written) override;
  int read(void* buf, size_t maxlen, size_t* len) override;

  int listen(int backlog) override;
  int accept(nr_transport_addr* addrp, nr_socket** sockp) override;
  bool IsProxied() const override;
  int async_wait(int how, NR_async_cb cb, void* cb_arg, char* function,
                 int line) override;
  int cancel(int how) override;

  // Need override since this is virtual in NrSocketBase
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestNrSocket, override)

 private:
  virtual ~TestNrSocket();

  class UdpPacket {
   public:
    UdpPacket(const void* msg, size_t len, const nr_transport_addr& addr)
        : buffer_(new MediaPacket) {
      buffer_->Copy(static_cast<const uint8_t*>(msg), len);
      // TODO(bug 1170299): Remove const_cast when no longer necessary
      nr_transport_addr_copy(&remote_address_,
                             const_cast<nr_transport_addr*>(&addr));
    }

    nr_transport_addr remote_address_;
    UniquePtr<MediaPacket> buffer_;

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UdpPacket);

   private:
    ~UdpPacket() {}
  };

  class PortMapping {
   public:
    PortMapping(const nr_transport_addr& remote_address,
                const RefPtr<NrSocketBase>& external_socket);

    int sendto(const void* msg, size_t len, const nr_transport_addr& to);
    int async_wait(int how, NR_async_cb cb, void* cb_arg, char* function,
                   int line);
    int cancel(int how);
    int send_from_queue();
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PortMapping);

    PRIntervalTime last_used_;
    RefPtr<NrSocketBase> external_socket_;
    // For non-symmetric, most of the data here doesn't matter
    nr_transport_addr remote_address_;

   private:
    ~PortMapping() { external_socket_->close(); }

    // If external_socket_ returns E_WOULDBLOCK, we don't want to propagate
    // that to the code using the TestNrSocket. We can also perhaps use this
    // to help simulate things like latency.
    std::list<RefPtr<UdpPacket>> send_queue_;
  };

  struct DeferredPacket {
    DeferredPacket(TestNrSocket* sock, const void* data, size_t len, int flags,
                   nr_transport_addr* addr,
                   RefPtr<NrSocketBase> internal_socket)
        : socket_(sock),
          buffer_(),
          flags_(flags),
          internal_socket_(internal_socket) {
      buffer_.Copy(reinterpret_cast<const uint8_t*>(data), len);
      nr_transport_addr_copy(&to_, addr);
    }

    TestNrSocket* socket_;
    MediaPacket buffer_;
    int flags_;
    nr_transport_addr to_;
    RefPtr<NrSocketBase> internal_socket_;
  };

  bool is_port_mapping_stale(const PortMapping& port_mapping) const;
  bool allow_ingress(const nr_transport_addr& from,
                     PortMapping** port_mapping_used) const;
  void destroy_stale_port_mappings();

  static void socket_readable_callback(void* real_sock_v, int how,
                                       void* test_sock_v);
  void on_socket_readable(NrSocketBase* external_or_internal_socket);
  void fire_readable_callback();

  static void port_mapping_tcp_passthrough_callback(void* ext_sock_v, int how,
                                                    void* test_sock_v);
  void cancel_port_mapping_async_wait(int how);

  static void port_mapping_writeable_callback(void* ext_sock_v, int how,
                                              void* test_sock_v);
  void write_to_port_mapping(NrSocketBase* external_socket);
  bool is_tcp_connection_behind_nat() const;

  PortMapping* get_port_mapping(const nr_transport_addr& remote_addr,
                                TestNat::NatBehavior filter) const;
  PortMapping* create_port_mapping(
      const nr_transport_addr& remote_addr,
      const RefPtr<NrSocketBase>& external_socket) const;
  RefPtr<NrSocketBase> create_external_socket(
      const nr_transport_addr& remote_addr) const;

  static void process_delayed_cb(NR_SOCKET s, int how, void* cb_arg);

  RefPtr<NrSocketBase> readable_socket_;
  // The socket for the "internal" address; used to talk to stuff behind the
  // same nat.
  RefPtr<NrSocketBase> internal_socket_;
  RefPtr<TestNat> nat_;
  bool tls_;
  // Since our comparison logic is different depending on what kind of NAT
  // we simulate, and the STL does not make it very easy to switch out the
  // comparison function at runtime, and these lists are going to be very
  // small anyway, we just brute-force it.
  std::list<RefPtr<PortMapping>> port_mappings_;

  void* timer_handle_;
};

}  // namespace mozilla

#endif  // test_nr_socket__
