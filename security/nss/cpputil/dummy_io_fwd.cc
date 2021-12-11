/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"

#include "dummy_io.h"

static DummyIOLayerMethods *ToMethods(PRFileDesc *f) {
  return reinterpret_cast<DummyIOLayerMethods *>(f->secret);
}

static PRStatus DummyClose(PRFileDesc *f) { return ToMethods(f)->Close(f); }

static int32_t DummyRead(PRFileDesc *f, void *buf, int32_t length) {
  return ToMethods(f)->Read(f, buf, length);
}

static int32_t DummyWrite(PRFileDesc *f, const void *buf, int32_t length) {
  return ToMethods(f)->Write(f, buf, length);
}

static int32_t DummyAvailable(PRFileDesc *f) {
  return ToMethods(f)->Available(f);
}

static int64_t DummyAvailable64(PRFileDesc *f) {
  return ToMethods(f)->Available64(f);
}

static PRStatus DummySync(PRFileDesc *f) { return ToMethods(f)->Sync(f); }

static int32_t DummySeek(PRFileDesc *f, int32_t offset, PRSeekWhence how) {
  return ToMethods(f)->Seek(f, offset, how);
}

static int64_t DummySeek64(PRFileDesc *f, int64_t offset, PRSeekWhence how) {
  return ToMethods(f)->Seek64(f, offset, how);
}

static PRStatus DummyFileInfo(PRFileDesc *f, PRFileInfo *info) {
  return ToMethods(f)->FileInfo(f, info);
}

static PRStatus DummyFileInfo64(PRFileDesc *f, PRFileInfo64 *info) {
  return ToMethods(f)->FileInfo64(f, info);
}

static int32_t DummyWritev(PRFileDesc *f, const PRIOVec *iov, int32_t iov_size,
                           PRIntervalTime to) {
  return ToMethods(f)->Writev(f, iov, iov_size, to);
}

static PRStatus DummyConnect(PRFileDesc *f, const PRNetAddr *addr,
                             PRIntervalTime to) {
  return ToMethods(f)->Connect(f, addr, to);
}

static PRFileDesc *DummyAccept(PRFileDesc *f, PRNetAddr *addr,
                               PRIntervalTime to) {
  return ToMethods(f)->Accept(f, addr, to);
}

static PRStatus DummyBind(PRFileDesc *f, const PRNetAddr *addr) {
  return ToMethods(f)->Bind(f, addr);
}

static PRStatus DummyListen(PRFileDesc *f, int32_t depth) {
  return ToMethods(f)->Listen(f, depth);
}

static PRStatus DummyShutdown(PRFileDesc *f, int32_t how) {
  return ToMethods(f)->Shutdown(f, how);
}

static int32_t DummyRecv(PRFileDesc *f, void *buf, int32_t buflen,
                         int32_t flags, PRIntervalTime to) {
  return ToMethods(f)->Recv(f, buf, buflen, flags, to);
}

static int32_t DummySend(PRFileDesc *f, const void *buf, int32_t amount,
                         int32_t flags, PRIntervalTime to) {
  return ToMethods(f)->Send(f, buf, amount, flags, to);
}

static int32_t DummyRecvfrom(PRFileDesc *f, void *buf, int32_t amount,
                             int32_t flags, PRNetAddr *addr,
                             PRIntervalTime to) {
  return ToMethods(f)->Recvfrom(f, buf, amount, flags, addr, to);
}

static int32_t DummySendto(PRFileDesc *f, const void *buf, int32_t amount,
                           int32_t flags, const PRNetAddr *addr,
                           PRIntervalTime to) {
  return ToMethods(f)->Sendto(f, buf, amount, flags, addr, to);
}

static int16_t DummyPoll(PRFileDesc *f, int16_t in_flags, int16_t *out_flags) {
  return ToMethods(f)->Poll(f, in_flags, out_flags);
}

static int32_t DummyAcceptRead(PRFileDesc *f, PRFileDesc **nd,
                               PRNetAddr **raddr, void *buf, int32_t amount,
                               PRIntervalTime t) {
  return ToMethods(f)->AcceptRead(f, nd, raddr, buf, amount, t);
}

static int32_t DummyTransmitFile(PRFileDesc *sd, PRFileDesc *f,
                                 const void *headers, int32_t hlen,
                                 PRTransmitFileFlags flags, PRIntervalTime t) {
  return ToMethods(f)->TransmitFile(sd, f, headers, hlen, flags, t);
}

static PRStatus DummyGetpeername(PRFileDesc *f, PRNetAddr *addr) {
  return ToMethods(f)->Getpeername(f, addr);
}

static PRStatus DummyGetsockname(PRFileDesc *f, PRNetAddr *addr) {
  return ToMethods(f)->Getsockname(f, addr);
}

static PRStatus DummyGetsockoption(PRFileDesc *f, PRSocketOptionData *opt) {
  return ToMethods(f)->Getsockoption(f, opt);
}

static PRStatus DummySetsockoption(PRFileDesc *f,
                                   const PRSocketOptionData *opt) {
  return ToMethods(f)->Setsockoption(f, opt);
}

static int32_t DummySendfile(PRFileDesc *f, PRSendFileData *in,
                             PRTransmitFileFlags flags, PRIntervalTime to) {
  return ToMethods(f)->Sendfile(f, in, flags, to);
}

static PRStatus DummyConnectContinue(PRFileDesc *f, int16_t flags) {
  return ToMethods(f)->ConnectContinue(f, flags);
}

static int32_t DummyReserved(PRFileDesc *f) {
  return ToMethods(f)->Reserved(f);
}

extern const struct PRIOMethods DummyMethodsForward = {
    PR_DESC_LAYERED,    DummyClose,
    DummyRead,          DummyWrite,
    DummyAvailable,     DummyAvailable64,
    DummySync,          DummySeek,
    DummySeek64,        DummyFileInfo,
    DummyFileInfo64,    DummyWritev,
    DummyConnect,       DummyAccept,
    DummyBind,          DummyListen,
    DummyShutdown,      DummyRecv,
    DummySend,          DummyRecvfrom,
    DummySendto,        DummyPoll,
    DummyAcceptRead,    DummyTransmitFile,
    DummyGetsockname,   DummyGetpeername,
    DummyReserved,      DummyReserved,
    DummyGetsockoption, DummySetsockoption,
    DummySendfile,      DummyConnectContinue,
    DummyReserved,      DummyReserved,
    DummyReserved,      DummyReserved};
