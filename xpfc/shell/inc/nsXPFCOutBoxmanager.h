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

#ifndef nsXPFCOutBoxManager_h___
#define nsXPFCOutBoxManager_h___

#include "nsIXPFCOutBoxManager.h"
#include "nsArray.h"

class nsXPFCOutBoxItem
{
public:
  nsXPFCOutBoxItem();
  ~nsXPFCOutBoxItem();

  NS_IMETHOD                        GetMimeType(nsString& aMimeType);
  NS_IMETHOD                        GetOffset(PRInt32* aOffset);
  NS_IMETHOD                        GetData(char* aTheData);
  NS_IMETHOD                        DeleteMe();
  NS_IMETHOD                        ReWriteMe();

private:
  nsString                          m_mimeType;
  PRInt32                           m_offset;
  char*                             m_theData;
};

class nsXPFCOutBoxItemHandler : public nsIXPFCOutBoxItemHandler
{
public:
  nsXPFCOutBoxItemHandler();

  NS_IMETHOD                        GetMimeType(nsString& mimeType);
  NS_IMETHOD                        HandleItem(nsIApplicationShell* HostShell, nsIXPFCOutBoxItem* AnOutBoxitem);

private:
  nsString                          m_mimeType;

  NS_IMETHOD                        SetMimeType(nsString& mimeType);

protected:
  ~nsXPFCOutBoxItemHandler();
};

class nsXPFCOutBoxManager : public nsIXPFCOutBoxManager
{
public:
  nsXPFCOutBoxManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                        Init();
  NS_IMETHOD                        AddItem(nsString& mimeType, nsIInputStream& inStream) ;
  NS_IMETHOD                        FindItem(nsString& mimeType, nsIOutputStream* outSteam);
  NS_IMETHOD                        ItemsCount(PRInt32 *OutBoxItems);
  NS_IMETHOD                        ItemsCount(nsString& mimeType, PRInt32 *OutBoxItems);
  NS_IMETHOD                        ItemsSize(PRInt32 *OutBoxItemsSize);
  NS_IMETHOD                        AddHandler(nsIXPFCOutBoxItemHandler*& AnOutBoxHandler, nsIXPFCOutBoxItemHandlerCallback*& host);
  NS_IMETHOD                        SendItems();

private:
  nsVoidArray                       m_theDataSource;  // Array of nsXPFCOutBoxItem
  nsVoidArray                       m_theHandlers;    // Array of nsXPFCOutBoxItemHandler

protected:
  ~nsXPFCOutBoxManager();
};

#endif
