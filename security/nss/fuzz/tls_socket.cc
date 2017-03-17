/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <string.h>
#include <algorithm>

#include "prerror.h"
#include "prio.h"

#include "tls_socket.h"

int32_t DummyPrSocket::Read(PRFileDesc *f, void *data, int32_t len) {
  assert(data && len > 0);

  int32_t amount = std::min(len, static_cast<int32_t>(len_));
  memcpy(data, buf_, amount);

  buf_ += amount;
  len_ -= amount;

  return amount;
}

int32_t DummyPrSocket::Write(PRFileDesc *f, const void *buf, int32_t length) {
  return length;
}

int32_t DummyPrSocket::Recv(PRFileDesc *f, void *buf, int32_t buflen,
                            int32_t flags, PRIntervalTime to) {
  assert(flags == 0);
  return Read(f, buf, buflen);
}
