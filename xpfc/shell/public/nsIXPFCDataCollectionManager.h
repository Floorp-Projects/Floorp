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

#ifndef nsIXPFCDataCollectionManager_h___
#define nsIXPFCDataCollectionManager_h___

#include "nsISupports.h"
#include "nsIXPFCCommandReceiver.h"
#include "nsString.h"
#include "nsVoidArray.h"

class nsIXPFCDataCollection;
class nsIApplicationShell;
class nsIShellInstance;

//a2e85d80-5ca8-11d2-80a1-00600832d688
#define NS_IXPFCDATACOLLECTION_MANAGER_IID   \
{ 0xa2e85d80, 0x5ca8, 0x11d2,    \
{ 0x80, 0xa1, 0x00, 0x60, 0x08, 0x32, 0xd6, 0x88 } }

//8ea29570-6923-11d2-80a3-00600832d688
#define NS_ICOLLECED_MANAGER_IID   \
{ 0x8ea29570, 0x6923, 0x11d2,    \
{ 0x80, 0xa3, 0x00, 0x60, 0x08, 0x32, 0xd6, 0x88 } }

typedef struct CollectedData
{
  nsString                          LabelName;
  nsString                          Value;
} *CollectedDataPtr;

class nsICollectedData : public nsISupports
{
public:

  NS_IMETHOD                        Init() = 0 ;
  NS_IMETHOD                        FindValueInCollectedData(nsString& Label, nsString& result) = 0;
  NS_IMETHOD_(nsVoidArray*)         GetDataArray() = 0;
  NS_IMETHOD_(nsString)             GetDataHandlerType() = 0;
  NS_IMETHOD                        SetDataHandlerType(nsString& TypeString) = 0;
};

class nsIXPFCDataCollectionManager : public nsISupports
{

public:

  NS_IMETHOD                        Init() = 0 ;
  NS_IMETHOD                        AddDataCollection(nsString& DataHandlerName, nsIApplicationShell *aHostShell) = 0;
  NS_IMETHOD                        CallDataHandler(nsString& DataHandlerName, nsICollectedData* TheCollectedData) = 0;
};

#endif /* nsIXPFCToolbarManager_h___ */
