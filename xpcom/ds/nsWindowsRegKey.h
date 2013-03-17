/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
extern "C" nsresult
NS_NewWindowsRegKey(nsIWindowsRegKey **result);

//-----------------------------------------------------------------------------

#ifdef _IMPL_NS_COM

// a53bc624-d577-4839-b8ec-bb5040a52ff4
#define NS_WINDOWSREGKEY_CID \
  { 0xa53bc624, 0xd577, 0x4839, \
    { 0xb8, 0xec, 0xbb, 0x50, 0x40, 0xa5, 0x2f, 0xf4 } }

extern nsresult
nsWindowsRegKeyConstructor(nsISupports *outer, const nsIID &iid, void **result);

#endif  // _IMPL_NS_COM

//-----------------------------------------------------------------------------

#endif  // nsWindowsRegKey_h__
