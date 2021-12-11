/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsUnixRemoteServer_h__
#define __nsUnixRemoteServer_h__

#include "nsString.h"

#ifdef IS_BIG_ENDIAN
#  define TO_LITTLE_ENDIAN32(x)                           \
    ((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | \
     (((x)&0x0000ff00) << 8) | (((x)&0x000000ff) << 24))
#else
#  define TO_LITTLE_ENDIAN32(x) (x)
#endif

class nsUnixRemoteServer {
 protected:
  void SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                      uint32_t aTimestamp);
  const char* HandleCommandLine(const char* aBuffer, uint32_t aTimestamp);
};

#endif  // __nsGTKRemoteService_h__
