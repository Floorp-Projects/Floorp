/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsSmartCardEvent.h"
#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMEvent.h"
#include "nsXPCOM.h"

// DOM event class to handle progress notifications
nsSmartCardEvent::nsSmartCardEvent(const nsAString &aTokenName) 
    : mInner(nullptr), mTokenName(aTokenName)
{
}

nsSmartCardEvent::~nsSmartCardEvent()
{}

//NS_DECL_DOM_CLASSINFO(SmartCardEvent)

NS_INTERFACE_MAP_BEGIN(nsSmartCardEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMSmartCardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSmartCardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SmartCardEvent)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsSmartCardEvent)
NS_IMPL_RELEASE(nsSmartCardEvent)

//
// Init must be called before we do anything with the event.
//
NS_IMETHODIMP nsSmartCardEvent::Init(nsIDOMEvent * aInner)
{
  NS_ASSERTION(aInner, "SmartCardEvent initialized with a null Event");
  mInner = aInner;
  mInner->SetTrusted(true);
  return NS_OK;
}

// nsSmartCard Specific methods
NS_IMETHODIMP nsSmartCardEvent::GetTokenName(nsAString &aTokenName)
{
  aTokenName = mTokenName;
  return NS_OK;
}

// nsIDOMEvent maps
NS_IMETHODIMP nsSmartCardEvent::DuplicatePrivateData(void)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->DuplicatePrivateData();
}

NS_IMETHODIMP nsSmartCardEvent::SetTarget(nsIDOMEventTarget *aTarget)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->SetTarget(aTarget);
}

NS_IMETHODIMP_(bool ) nsSmartCardEvent::IsDispatchStopped()
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->IsDispatchStopped();
}

NS_IMETHODIMP_(nsEvent*) nsSmartCardEvent::GetInternalNSEvent()
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetInternalNSEvent();
}

NS_IMETHODIMP_(void) nsSmartCardEvent::SetTrusted(bool aResult)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  mInner->SetTrusted(aResult);
}

void
nsSmartCardEvent::Serialize(IPC::Message* aMsg,
                            bool aSerializeInterfaceType)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  mInner->Serialize(aMsg, aSerializeInterfaceType);
}

bool
nsSmartCardEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->Deserialize(aMsg, aIter);
}

NS_IMETHODIMP nsSmartCardEvent::GetOriginalTarget(nsIDOMEventTarget * *aOriginalTarget)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetOriginalTarget(aOriginalTarget);
}

NS_IMETHODIMP nsSmartCardEvent::GetExplicitOriginalTarget(nsIDOMEventTarget * *aTarget)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetExplicitOriginalTarget(aTarget);
}

NS_IMETHODIMP nsSmartCardEvent::PreventBubble(void)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->PreventBubble();
}

NS_IMETHODIMP nsSmartCardEvent::PreventCapture(void)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->PreventCapture();
}

NS_IMETHODIMP nsSmartCardEvent::GetIsTrusted(bool *aIsTrusted)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetIsTrusted(aIsTrusted);
}

NS_IMETHODIMP
nsSmartCardEvent::GetPreventDefault(bool* aReturn)
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->GetPreventDefault(aReturn);
}

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

NS_IMETHODIMP nsSmartCardEvent::GetEventPhase(uint16_t *aEventPhase)
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

NS_IMETHODIMP nsSmartCardEvent::StopImmediatePropagation()
{
  NS_ASSERTION(mInner, "SmartCardEvent called without Init");
  return mInner->StopImmediatePropagation();
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

