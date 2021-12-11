/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowsRegKey_h__
#define nsWindowsRegKey_h__

//-----------------------------------------------------------------------------

#include "nsIWindowsRegKey.h"

/**
 * This ContractID may be used to instantiate a windows registry key object
 * via the XPCOM component manager.
 */
#define NS_WINDOWSREGKEY_CONTRACTID "@mozilla.org/windows-registry-key;1"

/**
 * This function may be used to instantiate a windows registry key object prior
 * to XPCOM being initialized.
 */
extern "C" void NS_NewWindowsRegKey(nsIWindowsRegKey** aResult);

//-----------------------------------------------------------------------------

#ifdef IMPL_LIBXUL

// a53bc624-d577-4839-b8ec-bb5040a52ff4
#  define NS_WINDOWSREGKEY_CID                         \
    {                                                  \
      0xa53bc624, 0xd577, 0x4839, {                    \
        0xb8, 0xec, 0xbb, 0x50, 0x40, 0xa5, 0x2f, 0xf4 \
      }                                                \
    }

[[nodiscard]] extern nsresult nsWindowsRegKeyConstructor(nsISupports* aOuter,
                                                         const nsIID& aIID,
                                                         void** aResult);

#endif  // IMPL_LIBXUL

//-----------------------------------------------------------------------------

#endif  // nsWindowsRegKey_h__
