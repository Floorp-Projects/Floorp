/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

MOZ_BEGIN_EXTERN_C
  void MozillaRegisterDebugFD(int fd);
  void MozillaUnRegisterDebugFD(int fd);
MOZ_END_EXTERN_C

#ifdef __cplusplus
namespace mozilla {
void PoisonWrite();
void DisableWritePoisoning();
}
#endif
