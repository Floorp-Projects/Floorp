/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef __nsIRelatedLinks_h
#define __nsIRelatedLinks_h

#include "nscore.h"
#include "nsISupports.h"

#include "nspr.h"

typedef struct _RL_Window* RL_Window; 
typedef struct _RL_Item* RL_Item; 
typedef uint8 RL_ItemType; 

#define RL_CONTAINER 1 
#define RL_SEPARATOR 2 
#define RL_LINK      3
#define RL_LINK_NEW_WINDOW      4

typedef void (*RLCallBackProc)(void* pdata, RL_Window win);

/* 1A0B6FA1-EA25-11d1-BEAE-00805F8A66DC */

#define NS_IRELATEDLINKS_IID                               \
{ 0x2ae42530, 0x1b90, 0x11d2,                         \
   { 0x84, 0xb6, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7 } }

class nsIRelatedLinks : public nsISupports
{
public:

   virtual void DestroyRLWindow(RL_Window win) = 0;

   virtual void SetRLWindowURL(RL_Window win, char* url) = 0;

   virtual RL_Item WindowItems (RL_Window win) = 0;

   virtual RL_Item NextItem(RL_Item item) = 0;

   virtual RL_Item ItemChild(RL_Item item) = 0;

   virtual uint8 ItemCount(RL_Window win) = 0;

   virtual RL_ItemType GetItemType(RL_Item item) = 0;

   virtual char* ItemName(RL_Item item) = 0;

   virtual char* ItemUrl(RL_Item item) = 0;

   virtual RL_Window MakeRLWindowWithCallback(RLCallBackProc callBack, void*fedata) = 0;

};

extern "C" void RL_Init();
extern "C" nsIRelatedLinks * NS_NewRelatedLinks(); 

#endif


