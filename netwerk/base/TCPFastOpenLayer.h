/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TCPFastOpenLayer_h__
#define TCPFastOpenLayer_h__

#include "prerror.h"

namespace mozilla {
namespace net {

/**
 * This layer must be placed just above PR-tcp socket, i.e. it must be under
 * nss layer.
 * At the beginning of TCPFastOpenLayer.cpp there is explanation what this
 * layer do.
 **/

#define TFO_NOT_TRIED 0
#define TFO_TRIED     1
#define TFO_DATA_SENT 2
#define TFO_FAILED    3

nsresult AttachTCPFastOpenIOLayer(PRFileDesc *fd);

// Get the result of TCP Fast Open.
void TCPFastOpenFinish(PRFileDesc *fd, PRErrorCode &err,
                       bool &fastOpenNotSupported, uint8_t &tfoStatus);

int32_t TCPFastOpenGetBufferSizeLeft(PRFileDesc *fd);

bool TCPFastOpenGetCurrentBufferSize(PRFileDesc *fd);
bool TCPFastOpenFlushBuffer(PRFileDesc *fd);
}
}

#endif // TCPFastOpenLayer_h__
