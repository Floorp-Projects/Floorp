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

#ifndef nsIRollupListener_h__
#define nsIRollupListener_h__

#include "nsISupports.h"

// {23C2BA03-6C76-11d3-96ED-0060B0FB9956}
#define NS_IROLLUPLISTENER_IID      \
{ 0x23c2ba03, 0x6c76, 0x11d3, { 0x96, 0xed, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } };

class nsIRollupListener : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IROLLUPLISTENER_IID)

   /**
    * Notifies the object to rollup
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD Rollup() = 0;

};

#endif
