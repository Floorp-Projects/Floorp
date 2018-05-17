/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSHelper_h
#define nsNSSHelper_h

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "pk11func.h"

// Implementation of an nsIInterfaceRequestor for use as context for NSS calls.
class PipUIContext : public nsIInterfaceRequestor
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  PipUIContext();

protected:
  virtual ~PipUIContext();
};

// Function to get the implementor for a certain set of NSS specific dialogs.
nsresult
getNSSDialogs(void **_result, REFNSIID aIID, const char *contract);

// A function that sets the password on an unitialized slot.
nsresult
setPassword(PK11SlotInfo* slot, nsIInterfaceRequestor* ctx);

#endif // nsNSSHelper_h
