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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIKeyBindMgr_h__
#define nsIKeyBindMgr_h__

#include "nsISupports.h"
#include "nsIDOMNode.h"
#include "nsGUIEvent.h"
#include "nsIWebShell.h"
#include "nsIContent.h"

// {a91c0821-de58-11d2-b345-00a0cc3c1cde}
#define NS_IKEYBINDMGR_IID \
{ 0xa91c0821, 0xde58, 0x11d2, \
{ 0xb3, 0x45, 0x0, 0xa0, 0xcc, 0x3c, 0x1c, 0xde } }

// {8B5314BD-DB01-11d2-96CE-0060B0FB9977}
#define NS_KEYBINDMGR_CID      \
{ 0x8b5314bd, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x77 } }

/**
 * Keyboard Binding utility.
 * Given a key event and a DOM node to search executes any 'key' command
 * that matches the event
 */

class nsIKeyBindMgr : public nsISupports
{
public:
  static const nsIID& GetIID()
    { static nsIID iid = NS_IKEYBINDMGR_IID; return iid; }
    
  NS_IMETHOD ProcessKeyEvent(
    nsIDOMDocument   * domDoc, 
    const nsKeyEvent & theEvent, 
    nsIWebShell      * webShell,  
    nsEventStatus    & theStatus) = 0;

};

#endif // nsIKeyBindMgr_h__
