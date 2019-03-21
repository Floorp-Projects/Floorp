/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _cert_storage_h_
#define _cert_storage_h_

#include "nsISupportsUtils.h"  // for nsresult, etc.
#include "mozilla/SyncRunnable.h"

// {16e5c837-f877-4e23-9c64-eddf905e30e6}
#define NS_CERT_STORAGE_CID                          \
  {                                                  \
    0x16e5c837, 0xf877, 0x4e23, {                    \
      0x9c, 0x64, 0xed, 0xdf, 0x90, 0x5e, 0x30, 0xe6 \
    }                                                \
  }

extern "C" {
nsresult cert_storage_constructor(nsISupports* outer, REFNSIID iid,
                                  void** result);
};

nsresult construct_cert_storage(nsISupports* outer, REFNSIID iid,
                                void** result);

#endif  // _cert_storage_h_