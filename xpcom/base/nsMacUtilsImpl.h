  /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMacUtilsImpl_h___
#define nsMacUtilsImpl_h___

#include "nsIMacUtils.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

class nsMacUtilsImpl MOZ_FINAL : public nsIMacUtils
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACUTILS

  nsMacUtilsImpl()
  {
  }

private:
  ~nsMacUtilsImpl()
  {
  }

  nsresult GetArchString(nsAString& aArchString);

  // A string containing a "-" delimited list of architectures
  // in our binary.
  nsString mBinaryArchs;
};

// Global singleton service
// 697BD3FD-43E5-41CE-AD5E-C339175C0818
#define NS_MACUTILSIMPL_CID \
 {0x697BD3FD, 0x43E5, 0x41CE, {0xAD, 0x5E, 0xC3, 0x39, 0x17, 0x5C, 0x08, 0x18}}
#define NS_MACUTILSIMPL_CONTRACTID "@mozilla.org/xpcom/mac-utils;1"

#endif /* nsMacUtilsImpl_h___ */
