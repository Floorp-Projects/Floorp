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
#include "nsSmartCardEvent.h"
#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEvent.h"
#include "nsXPCOM.h"

// DOM event class to handle progress notifications
nsSmartCardEvent::nsSmartCardEvent(const nsAString &aTokenName) 
    : mInner(nsnull), mPrivate(nsnull), mTokenName(aTokenName)
{
}

nsSmartCardEvent::~nsSmartCardEvent()
{}

//NS_DECL_DOM_CLASSINFO(SmartCardEvent)

// QueryInterface implementation for nsXMLHttpRequest
NS_INTERFACE_MAP_BEGIN(nsSmartCardEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMSmartCardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSmartCardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateDOMEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SmartCardEvent)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsSmartCardEvent)
NS_IMPL_RELEASE(nsSmartCardEvent)

//
// Init must be called before we do anything with the event.
//
NS_IMETHODIMP nsSmartCardEvent::Init(nsIDOMEvent * aInner)
{
  nsresult rv;

  NS_ASSERTION(aInner, "SmartCardEvent initialized with a null Event");
  mInner = aInner;
  mPrivate = do_QueryInterface(mInner, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mNSEvent = do_QueryInterface(mInner, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return mPrivate->SetTrusted(PR_TRUE);
}

// nsSmartCard Specific methods
NS_IMETHODIMP nsSmartCardEvent::GetTokenName(nsAString &aTokenName)
{
  aTokenName = mTokenName;
  return NS_OK;
}

// nsIPrivateDOMEvent maps
NS_IMETHODIMP nsSmartCardEvent::DuplicatePrivateData(void)
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  return mPrivate->DuplicatePrivateData();
}

NS_IMETHODIMP nsSmartCardEvent::SetTarget(nsIDOMEventTarget *aTarget)
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  return mPrivate->SetTarget(aTarget);
}

NS_IMETHODIMP_(bool ) nsSmartCardEvent::IsDispatchStopped()
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  return mPrivate->IsDispatchStopped();
}

NS_IMETHODIMP_(nsEvent*) nsSmartCardEvent::GetInternalNSEvent()
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  return mPrivate->GetInternalNSEvent();
}

NS_IMETHODIMP nsSmartCardEvent::SetTrusted(bool aResult)
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  return mPrivate->SetTrusted(aResult);
}

void
nsSmartCardEvent::Serialize(IPC::Message* aMsg,
                            bool aSerializeInterfaceType)
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  mPrivate->Serialize(aMsg, aSerializeInterfaceType);
}

bool
nsSmartCardEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ASSERTION(mPrivate, "SmartCardEvent called without Init");
  return mPrivate->Deserialize(aMsg, aIter);
}

// IDOMNSEvent maps
NS_IMETHODIMP nsSmartCardEvent::GetOriginalTarget(nsIDOMEventTarget * *aOriginalTarget)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->GetOriginalTarget(aOriginalTarget);
}

NS_IMETHODIMP nsSmartCardEvent::GetExplicitOriginalTarget(nsIDOMEventTarget * *aTarget)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->GetExplicitOriginalTarget(aTarget);
}

NS_IMETHODIMP nsSmartCardEvent::GetTmpRealOriginalTarget(nsIDOMEventTarget * *aTarget)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->GetTmpRealOriginalTarget(aTarget);
}

NS_IMETHODIMP nsSmartCardEvent::PreventBubble(void)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->PreventBubble();
}

NS_IMETHODIMP nsSmartCardEvent::PreventCapture(void)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->PreventCapture();
}

NS_IMETHODIMP nsSmartCardEvent::GetIsTrusted(bool *aIsTrusted)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->GetIsTrusted(aIsTrusted);
}

NS_IMETHODIMP
nsSmartCardEvent::GetPreventDefault(bool* aReturn)
{
  NS_ASSERTION(mNSEvent, "SmartCardEvent called without Init");
  return mNSEvent->GetPreventDefault(aReturn);
}

// IDOMEvent maps
NS_IMETHODIMP nsSmartCardEvent::GetType(nsAString & aType)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetType(aType);
}

NS_IMETHODIMP nsSmartCardEvent::GetTarget(nsIDOMEventTarget * *aTarget)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetTarget(aTarget);
}

NS_IMETHODIMP nsSmartCardEvent::GetCurrentTarget(nsIDOMEventTarget * *aCurrentTarget)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetCurrentTarget(aCurrentTarget);
}

NS_IMETHODIMP nsSmartCardEvent::GetEventPhase(PRUint16 *aEventPhase)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetEventPhase(aEventPhase);
}

NS_IMETHODIMP nsSmartCardEvent::GetBubbles(bool *aBubbles)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetBubbles(aBubbles);
}

NS_IMETHODIMP nsSmartCardEvent::GetCancelable(bool *aCancelable)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetCancelable(aCancelable);
}

NS_IMETHODIMP nsSmartCardEvent::GetTimeStamp(DOMTimeStamp *aTimeStamp)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetTimeStamp(aTimeStamp);
}

NS_IMETHODIMP nsSmartCardEvent::StopPropagation()
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->StopPropagation();
}

NS_IMETHODIMP nsSmartCardEvent::PreventDefault()
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->PreventDefault();
}

NS_IMETHODIMP nsSmartCardEvent::GetDefaultPrevented(bool* aReturn)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetDefaultPrevented(aReturn);
}

NS_IMETHODIMP nsSmartCardEvent::InitEvent(const nsAString & eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->InitEvent(eventTypeArg, canBubbleArg, cancelableArg);
}

