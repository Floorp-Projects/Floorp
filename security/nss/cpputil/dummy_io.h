/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dummy_io_h__
#define dummy_io_h__

#include "prerror.h"
#include "prio.h"

#include "nss_scoped_ptrs.h"

class DummyIOLayerMethods {
 public:
  static ScopedPRFileDesc CreateFD(PRDescIdentity id,
                                   DummyIOLayerMethods *methods);

  virtual PRStatus Close(PRFileDesc *f);
  virtual int32_t Read(PRFileDesc *f, void *buf, int32_t length);
  virtual int32_t Write(PRFileDesc *f, const void *buf, int32_t length);
  virtual int32_t Available(PRFileDesc *f);
  virtual int64_t Available64(PRFileDesc *f);
  virtual PRStatus Sync(PRFileDesc *f);
  virtual int32_t Seek(PRFileDesc *f, int32_t offset, PRSeekWhence how);
  virtual int64_t Seek64(PRFileDesc *f, int64_t offset, PRSeekWhence how);
  virtual PRStatus FileInfo(PRFileDesc *f, PRFileInfo *info);
  virtual PRStatus FileInfo64(PRFileDesc *f, PRFileInfo64 *info);
  virtual int32_t Writev(PRFileDesc *f, const PRIOVec *iov, int32_t iov_size,
                         PRIntervalTime to);
  virtual PRStatus Connect(PRFileDesc *f, const PRNetAddr *addr,
                           PRIntervalTime to);
  virtual PRFileDesc *Accept(PRFileDesc *sd, PRNetAddr *addr,
                             PRIntervalTime to);
  virtual PRStatus Bind(PRFileDesc *f, const PRNetAddr *addr);
  virtual PRStatus Listen(PRFileDesc *f, int32_t depth);
  virtual PRStatus Shutdown(PRFileDesc *f, int32_t how);
  virtual int32_t Recv(PRFileDesc *f, void *buf, int32_t buflen, int32_t flags,
                       PRIntervalTime to);
  virtual int32_t Send(PRFileDesc *f, const void *buf, int32_t amount,
                       int32_t flags, PRIntervalTime to);
  virtual int32_t Recvfrom(PRFileDesc *f, void *buf, int32_t amount,
                           int32_t flags, PRNetAddr *addr, PRIntervalTime to);
  virtual int32_t Sendto(PRFileDesc *f, const void *buf, int32_t amount,
                         int32_t flags, const PRNetAddr *addr,
                         PRIntervalTime to);
  virtual int16_t Poll(PRFileDesc *f, int16_t in_flags, int16_t *out_flags);
  virtual int32_t AcceptRead(PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
                             void *buf, int32_t amount, PRIntervalTime t);
  virtual int32_t TransmitFile(PRFileDesc *sd, PRFileDesc *f,
                               const void *headers, int32_t hlen,
                               PRTransmitFileFlags flags, PRIntervalTime t);
  virtual PRStatus Getpeername(PRFileDesc *f, PRNetAddr *addr);
  virtual PRStatus Getsockname(PRFileDesc *f, PRNetAddr *addr);
  virtual PRStatus Getsockoption(PRFileDesc *f, PRSocketOptionData *opt);
  virtual PRStatus Setsockoption(PRFileDesc *f, const PRSocketOptionData *opt);
  virtual int32_t Sendfile(PRFileDesc *out, PRSendFileData *in,
                           PRTransmitFileFlags flags, PRIntervalTime to);
  virtual PRStatus ConnectContinue(PRFileDesc *f, int16_t flags);
  virtual int32_t Reserved(PRFileDesc *f);
};

#endif  // dummy_io_h__
