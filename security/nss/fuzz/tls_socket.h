/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_socket_h__
#define tls_socket_h__

#include "dummy_io.h"

class DummyPrSocket : public DummyIOLayerMethods {
 public:
  DummyPrSocket(const uint8_t *buf, size_t len) : buf_(buf), len_(len) {}

  int32_t Read(PRFileDesc *f, void *data, int32_t len) override;
  int32_t Write(PRFileDesc *f, const void *buf, int32_t length) override;
  int32_t Recv(PRFileDesc *f, void *buf, int32_t buflen, int32_t flags,
               PRIntervalTime to) override;

 private:
  const uint8_t *buf_;
  size_t len_;
};

#endif  // tls_socket_h__
