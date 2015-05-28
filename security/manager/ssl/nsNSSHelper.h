/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSS_HELPER_
#define NSS_HELPER_

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "pk11func.h"

//
// Implementation of an nsIInterfaceRequestor for use
// as context for NSS calls
//
class PipUIContext : public nsIInterfaceRequestor
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  PipUIContext();

protected:
  virtual ~PipUIContext();
};

//
// Function to get the implementor for a certain set of NSS
// specific dialogs.
//

nsresult 
getNSSDialogs(void **_result, REFNSIID aIID, const char *contract);

extern "C" {
// a "fake" unicode conversion function
PRBool
pip_ucs2_ascii_conversion_fn(PRBool toUnicode,
                             unsigned char *inBuf,
                             unsigned int inBufLen,
                             unsigned char *outBuf,
                             unsigned int maxOutBufLen,
                             unsigned int *outBufLen,
                             PRBool swapBytes);
}

//
// A function that sets the password on an unitialized slot.
//
nsresult
setPassword(PK11SlotInfo *slot, nsIInterfaceRequestor *ctx);

#endif

