/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMXULFocusTracker_h__
#define nsIDOMXULFocusTracker_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIController;
class nsIDOMElement;

#define NS_IDOMXULFOCUSTRACKER_IID \
 { 0xf3c50361, 0x14fe, 0x11d3, \
		{ 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } } 

class nsIDOMXULFocusTracker : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULFOCUSTRACKER_IID; return iid; }

  NS_IMETHOD    GetCurrent(nsIDOMElement** aCurrent)=0;
  NS_IMETHOD    SetCurrent(nsIDOMElement* aCurrent)=0;

  NS_IMETHOD    AddFocusListener(nsIDOMElement* aListener)=0;

  NS_IMETHOD    RemoveFocusListener(nsIDOMElement* aListener)=0;

  NS_IMETHOD    FocusChanged()=0;

  NS_IMETHOD    GetController(nsIDOMElement* aElement, nsIController** aReturn)=0;

  NS_IMETHOD    SetController(nsIDOMElement* aElement, nsIController* aController)=0;
};


#define NS_DECL_IDOMXULFOCUSTRACKER   \
  NS_IMETHOD    GetCurrent(nsIDOMElement** aCurrent);  \
  NS_IMETHOD    SetCurrent(nsIDOMElement* aCurrent);  \
  NS_IMETHOD    AddFocusListener(nsIDOMElement* aListener);  \
  NS_IMETHOD    RemoveFocusListener(nsIDOMElement* aListener);  \
  NS_IMETHOD    FocusChanged();  \
  NS_IMETHOD    GetController(nsIDOMElement* aElement, nsIController** aReturn);  \
  NS_IMETHOD    SetController(nsIDOMElement* aElement, nsIController* aController);  \



#define NS_FORWARD_IDOMXULFOCUSTRACKER(_to)  \
  NS_IMETHOD    GetCurrent(nsIDOMElement** aCurrent) { return _to GetCurrent(aCurrent); } \
  NS_IMETHOD    SetCurrent(nsIDOMElement* aCurrent) { return _to SetCurrent(aCurrent); } \
  NS_IMETHOD    AddFocusListener(nsIDOMElement* aListener) { return _to AddFocusListener(aListener); }  \
  NS_IMETHOD    RemoveFocusListener(nsIDOMElement* aListener) { return _to RemoveFocusListener(aListener); }  \
  NS_IMETHOD    FocusChanged() { return _to FocusChanged(); }  \
  NS_IMETHOD    GetController(nsIDOMElement* aElement, nsIController** aReturn) { return _to GetController(aElement, aReturn); }  \
  NS_IMETHOD    SetController(nsIDOMElement* aElement, nsIController* aController) { return _to SetController(aElement, aController); }  \


extern "C" NS_DOM nsresult NS_InitXULFocusTrackerClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULFocusTracker(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULFocusTracker_h__
