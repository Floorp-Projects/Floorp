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

#ifndef nsIXPFCOutBoxManager_h___
#define nsIXPFCOutBoxManager_h___

#include "nsISupports.h"
#include "nsIXPFCCommandReceiver.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIApplicationShell.h"
#include "nsIMIMEMessage.h"
#include "nsIIterator.h"
#include "nsString.h"

//a8d3fc10-7420-11d2-80a5-00600832d688
#define NS_IXPFCOUTBOX_MANAGER_IID   \
{ 0xa8d3fc10, 0x7420, 0x11d2,    \
{ 0x80, 0xa5, 0x00, 0x60, 0x08, 0x32, 0xd6, 0x88 } }

//dec1f3a0-74ee-11d2-80ab-00600832d688
#define NS_IXPFCOUTBOX_ITEMHANDLER_IID   \
{ 0xdec1f3a0, 0x74ee, 0x11d2,    \
{ 0x80, 0xab, 0x00, 0x60, 0x08, 0x32, 0xd6, 0x88 } }

//e8a78080-74ee-11d2-80ab-00600832d688
#define NS_IXPFCOUTBOX_ITEMHANDLER_CALLBACK_IID   \
{ 0xe8a78080, 0x74ee, 0x11d2,    \
{ 0x80, 0xab, 0x00, 0x60, 0x08, 0x32, 0xd6, 0x88 } }

/*
** Class nsXPFCOutBoxItem is used by 
*/
class nsIXPFCOutBoxItem : public nsISupports
{
public:
  NS_IMETHOD                        GetMimeType(nsString& aMimeType) = 0;
  NS_IMETHOD                        GetOffset(PRInt32* aOffset) = 0;
  NS_IMETHOD                        GetData(char* aTheData) = 0;
  NS_IMETHOD                        DeleteMe() = 0;
  NS_IMETHOD                        ReWriteMe() = 0;
};

class nsIXPFCOutBoxItemHandler : public nsISupports
{
public:

  NS_IMETHOD                        GetMimeType(nsString& aMimeType) = 0;
  NS_IMETHOD                        HandleItem(nsIApplicationShell*& aHostShell, nsIXPFCOutBoxItem*& aOutBoxItem) = 0;
};

class nsIXPFCOutBoxItemHandlerCallback : public nsISupports
{
public:

  NS_IMETHOD                        XPFCOutBoxCallback(nsIXPFCOutBoxItemHandler* aHandler, nsIXPFCOutBoxItem* aOutBoxItem) = 0;
};

/*
**
** OutBox Manager
**
** The idea here is any time the application will need to send or store data either to a local file 
** or over the network, it will first store that data in the outbox. For each type of data there will
** be a handler who job in life is to take that data and put or send it to the right place. The OutBox
** will manage this. The data must in a mime format.
**
** Thread Use:
**
** The methods AddItem and SendItems maybe called from any thread. They will not block. They will call the
** modal thread to do any real work. There will be only one running thread for this manager.
*/
class nsIXPFCOutBoxManager : public nsISupports
{

public:

  NS_IMETHOD                        Init() = 0 ;
 
  /*
  ** Add an item to the outbox. Caller must provide inStream. Outbox assumes all data
  ** is text based.
  */
  NS_IMETHOD                        AddItem(nsString& aMimeType, nsIMIMEMessage*& aMimeData) = 0;

  /*
  ** Get an item from the outbox. After the stream is closed this item will be 
  ** removed from the outbox. Caller must free outSteam. 
  */
//  NS_IMETHOD                        GetItems(nsString& aMimeType, nsIMIMEMessage*& aMimeData) = 0;
  NS_IMETHOD                        GetItems(nsString& aMimeType, nsIIterator* aListMimeData) = 0;

  /*
  ** Get the count of all the items stored in the outbox
  */
  NS_IMETHOD                        ItemsCount(PRInt32 *OutBoxItems) = 0;

  /*
  ** Get the count of the items with this mimeType that are stored in the outbox
  */
  NS_IMETHOD                        ItemsCount(nsString& aMimeType, PRInt32 *aOutBoxItemsCount) = 0;

  /*
  ** Get the size in bytes of all the items stored in the outbox
  */
  NS_IMETHOD                        ItemsSize(PRInt32 *aOutBoxItemsSize) = 0;

  /*
  ** Set up a handler for an outbox item
  */
  NS_IMETHOD                        AddHandler(nsIXPFCOutBoxItemHandler*& AnOutBoxHandler, nsIXPFCOutBoxItemHandlerCallback*& host) = 0;

  /*
  ** Tell the OutBox Manager to dispatch any stored items
  */
  NS_IMETHOD                        SendItems() = 0;
};

#endif

