/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMemory_h__
#define nsMemory_h__

#include "nsError.h"

class nsIMemory;

#define NS_MEMORY_CONTRACTID "@mozilla.org/xpcom/memory-service;1"
#define NS_MEMORY_CID                                \
  { /* 30a04e40-38e7-11d4-8cf5-0060b0fc14a3 */       \
    0x30a04e40, 0x38e7, 0x11d4, {                    \
      0x8c, 0xf5, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 \
    }                                                \
  }

/**
 * Static helper routines to manage memory. These routines allow easy access
 * to xpcom's built-in (global) nsIMemory implementation, without needing
 * to go through the service manager to get it. However this requires clients
 * to link with the xpcom DLL.
 *
 * This class is not threadsafe and is intented for use only on the main
 * thread.
 */
class nsMemory {
 public:
  static nsresult HeapMinimize(bool aImmediate);
  static nsIMemory* GetGlobalMemoryService();  // AddRefs
};

#endif  // nsMemory_h__
