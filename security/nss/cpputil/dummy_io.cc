/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <iostream>

#include "prerror.h"
#include "prio.h"

#include "dummy_io.h"

#define UNIMPLEMENTED()                                        \
  std::cerr << "Unimplemented: " << __FUNCTION__ << std::endl; \
  assert(false);

extern const struct PRIOMethods DummyMethodsForward;

ScopedPRFileDesc DummyIOLayerMethods::CreateFD(PRDescIdentity id,
                                               DummyIOLayerMethods *methods) {
  ScopedPRFileDesc fd(PR_CreateIOLayerStub(id, &DummyMethodsForward));
  assert(fd);
  if (!fd) {
    return nullptr;
  }
  fd->secret = reinterpret_cast<PRFilePrivate *>(methods);
  return fd;
}

PRStatus DummyIOLayerMethods::Close(PRFileDesc *f) {
  f->secret = nullptr;
  f->dtor(f);
  return PR_SUCCESS;
}

int32_t DummyIOLayerMethods::Read(PRFileDesc *f, void *buf, int32_t length) {
  UNIMPLEMENTED();
  return -1;
}

int32_t DummyIOLayerMethods::Write(PRFileDesc *f, const void *buf,
                                   int32_t length) {
  UNIMPLEMENTED();
  return -1;
}

int32_t DummyIOLayerMethods::Available(PRFileDesc *f) {
  UNIMPLEMENTED();
  return -1;
}

int64_t DummyIOLayerMethods::Available64(PRFileDesc *f) {
  UNIMPLEMENTED();
  return -1;
}

PRStatus DummyIOLayerMethods::Sync(PRFileDesc *f) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

int32_t DummyIOLayerMethods::Seek(PRFileDesc *f, int32_t offset,
                                  PRSeekWhence how) {
  UNIMPLEMENTED();
  return -1;
}

int64_t DummyIOLayerMethods::Seek64(PRFileDesc *f, int64_t offset,
                                    PRSeekWhence how) {
  UNIMPLEMENTED();
  return -1;
}

PRStatus DummyIOLayerMethods::FileInfo(PRFileDesc *f, PRFileInfo *info) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

PRStatus DummyIOLayerMethods::FileInfo64(PRFileDesc *f, PRFileInfo64 *info) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

int32_t DummyIOLayerMethods::Writev(PRFileDesc *f, const PRIOVec *iov,
                                    int32_t iov_size, PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

PRStatus DummyIOLayerMethods::Connect(PRFileDesc *f, const PRNetAddr *addr,
                                      PRIntervalTime to) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

PRFileDesc *DummyIOLayerMethods::Accept(PRFileDesc *sd, PRNetAddr *addr,
                                        PRIntervalTime to) {
  UNIMPLEMENTED();
  return nullptr;
}

PRStatus DummyIOLayerMethods::Bind(PRFileDesc *f, const PRNetAddr *addr) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

PRStatus DummyIOLayerMethods::Listen(PRFileDesc *f, int32_t depth) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

PRStatus DummyIOLayerMethods::Shutdown(PRFileDesc *f, int32_t how) {
  return PR_SUCCESS;
}

int32_t DummyIOLayerMethods::Recv(PRFileDesc *f, void *buf, int32_t buflen,
                                  int32_t flags, PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

// Note: this is always nonblocking and assumes a zero timeout.
int32_t DummyIOLayerMethods::Send(PRFileDesc *f, const void *buf,
                                  int32_t amount, int32_t flags,
                                  PRIntervalTime to) {
  return Write(f, buf, amount);
}

int32_t DummyIOLayerMethods::Recvfrom(PRFileDesc *f, void *buf, int32_t amount,
                                      int32_t flags, PRNetAddr *addr,
                                      PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

int32_t DummyIOLayerMethods::Sendto(PRFileDesc *f, const void *buf,
                                    int32_t amount, int32_t flags,
                                    const PRNetAddr *addr, PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

int16_t DummyIOLayerMethods::Poll(PRFileDesc *f, int16_t in_flags,
                                  int16_t *out_flags) {
  UNIMPLEMENTED();
  return -1;
}

int32_t DummyIOLayerMethods::AcceptRead(PRFileDesc *sd, PRFileDesc **nd,
                                        PRNetAddr **raddr, void *buf,
                                        int32_t amount, PRIntervalTime t) {
  UNIMPLEMENTED();
  return -1;
}

int32_t DummyIOLayerMethods::TransmitFile(PRFileDesc *sd, PRFileDesc *f,
                                          const void *headers, int32_t hlen,
                                          PRTransmitFileFlags flags,
                                          PRIntervalTime t) {
  UNIMPLEMENTED();
  return -1;
}

// TODO: Modify to return unique names for each channel
// somehow, as opposed to always the same static address. The current
// implementation messes up the session cache, which is why it's off
// elsewhere
PRStatus DummyIOLayerMethods::Getpeername(PRFileDesc *f, PRNetAddr *addr) {
  addr->inet.family = PR_AF_INET;
  addr->inet.port = 0;
  addr->inet.ip = 0;

  return PR_SUCCESS;
}

PRStatus DummyIOLayerMethods::Getsockname(PRFileDesc *f, PRNetAddr *addr) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

PRStatus DummyIOLayerMethods::Getsockoption(PRFileDesc *f,
                                            PRSocketOptionData *opt) {
  switch (opt->option) {
    case PR_SockOpt_Nonblocking:
      opt->value.non_blocking = PR_TRUE;
      return PR_SUCCESS;
    default:
      UNIMPLEMENTED();
      break;
  }

  return PR_FAILURE;
}

PRStatus DummyIOLayerMethods::Setsockoption(PRFileDesc *f,
                                            const PRSocketOptionData *opt) {
  switch (opt->option) {
    case PR_SockOpt_Nonblocking:
      return PR_SUCCESS;
    case PR_SockOpt_NoDelay:
      return PR_SUCCESS;
    default:
      UNIMPLEMENTED();
      break;
  }

  return PR_FAILURE;
}

int32_t DummyIOLayerMethods::Sendfile(PRFileDesc *out, PRSendFileData *in,
                                      PRTransmitFileFlags flags,
                                      PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

PRStatus DummyIOLayerMethods::ConnectContinue(PRFileDesc *f, int16_t flags) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

int32_t DummyIOLayerMethods::Reserved(PRFileDesc *f) {
  UNIMPLEMENTED();
  return -1;
}
