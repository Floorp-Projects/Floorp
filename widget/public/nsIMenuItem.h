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

#ifndef nsIMenuItem_h__
#define nsIMenuItem_h__

#include "nsISupports.h"
#include "nsString.h"

// {7F045771-4BEB-11d2-8DBB-00609703C14E}
#define NS_IMENUITEM_IID      \
{ 0x7f045771, 0x4beb, 0x11d2, \
  { 0x8d, 0xbb, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

class nsIMenu;
class nsIPopUpMenu;
class nsIWidget;

/**
 * MenuItem widget
 */
class nsIMenuItem : public nsISupports {

  public:
 
   /**
    * Creates the MenuItem
    *
    */
    NS_IMETHOD Create(nsIMenu        *aParent, 
                      const nsString &aLabel, 
                      PRUint32       aCommand) = 0;
    
   /**
    * Creates the MenuItem
    *
    */
    NS_IMETHOD Create(nsIPopUpMenu   *aParent, 
                      const nsString &aLabel,  
                      PRUint32        aCommand) = 0;
    
   /**
    * Get the MenuItem label
    *
    */
    NS_IMETHOD GetLabel(nsString &aText) = 0;

   /**
    * Get the MenuItem label
    *
    */
    NS_IMETHOD SetLabel(nsString &aText) = 0;

   /**
    * Gets the MenuItem Command identifier
    *
    */
    NS_IMETHOD GetCommand(PRUint32 & aCommand) = 0;

   /**
    * Gets the target for MenuItem
    *
    */
    NS_IMETHOD GetTarget(nsIWidget *& aTarget) = 0;
    
   /**
    * Gets Native Menu Handle
    *
    */
    NS_IMETHOD GetNativeData(void*& aData) = 0;


};

#endif
