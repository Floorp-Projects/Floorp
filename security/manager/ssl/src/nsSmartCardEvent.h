/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Red Hat Inc.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Robert Relyea <rrelyea@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsSmartCardEvent_h_
#define nsSmartCardEvent_h_

#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPCOM.h"

// Expose SmartCard Specific paramenters to smart card events.
class nsSmartCardEvent : public nsIDOMSmartCardEvent,
                         public nsIDOMNSEvent,
                         public nsIPrivateDOMEvent
{
public:
  nsSmartCardEvent(const nsAString &aTokenName);
  virtual ~nsSmartCardEvent();


  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSMARTCARDEVENT
  NS_DECL_NSIDOMNSEVENT

  //NS_DECL_NSIPRIVATEDOMEEVENT
  NS_IMETHOD DuplicatePrivateData();
  NS_IMETHOD SetTarget(nsIDOMEventTarget *aTarget);
  NS_IMETHOD_(nsEvent*) GetInternalNSEvent();
  NS_IMETHOD_(bool ) IsDispatchStopped();
  NS_IMETHOD SetTrusted(bool aResult);
  virtual void Serialize(IPC::Message* aMsg,
                         bool aSerializeInterfaceType);
  virtual bool Deserialize(const IPC::Message* aMsg, void** aIter);

  NS_DECL_NSIDOMEVENT

protected:
  nsCOMPtr<nsIDOMEvent> mInner;
  nsCOMPtr<nsIPrivateDOMEvent> mPrivate;
  nsCOMPtr<nsIDOMNSEvent> mNSEvent;
  nsString mTokenName;
};

#define SMARTCARDEVENT_INSERT "smartcard-insert"
#define SMARTCARDEVENT_REMOVE "smartcard-remove"

#endif
