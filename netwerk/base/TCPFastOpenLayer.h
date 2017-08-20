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

typedef enum {
  TFO_NOT_TRIED,
  TFO_TRIED,
  TFO_DATA_SENT,
  TFO_FAILED_CONNECTION_REFUSED,
  TFO_FAILED_NET_TIMEOUT,
  TFO_FAILED_UNKNOW_ERROR,
  TFO_FAILED_BACKUP_CONNECTION,
  TFO_FAILED_CONNECTION_REFUSED_NO_TFO_FAILED_TOO,
  TFO_FAILED_NET_TIMEOUT__NO_TFO_FAILED_TOO,
  TFO_FAILED_UNKNOW_ERROR_NO_TFO_FAILED_TOO,
  TFO_FAILED_BACKUP_CONNECTION_NO_TFO_FAILED_TOO,
  TFO_FAILED
} TFOResult;

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
