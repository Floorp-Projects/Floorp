/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSmartCardEvent_h_
#define nsSmartCardEvent_h_

#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPCOM.h"

// Expose SmartCard Specific paramenters to smart card events.
class nsSmartCardEvent : public nsIDOMSmartCardEvent,
                         public nsIDOMNSEvent
{
public:
  nsSmartCardEvent(const nsAString &aTokenName);
  virtual ~nsSmartCardEvent();


  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSMARTCARDEVENT
  NS_DECL_NSIDOMNSEVENT
  NS_DECL_NSIDOMEVENT

protected:
  nsCOMPtr<nsIDOMEvent> mInner;
  nsCOMPtr<nsIDOMNSEvent> mNSEvent;
  nsString mTokenName;
};

#define SMARTCARDEVENT_INSERT "smartcard-insert"
#define SMARTCARDEVENT_REMOVE "smartcard-remove"

#endif
