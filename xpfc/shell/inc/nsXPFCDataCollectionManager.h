/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsXPFCDataCollectionManager_h___
#define nsXPFCDataCollectionManager_h___

#include "nsIXPFCDataCollectionManager.h"
#include "nsIArray.h"
#include "nsIIterator.h"

class nsCollectedData : public nsICollectedData
{
public:
  nsCollectedData();
  ~nsCollectedData();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                        Init();
  NS_IMETHOD                        FindValueInCollectedData(nsString& Label, nsString& result);
  NS_IMETHOD_(nsVoidArray*)         GetDataArray();
  NS_IMETHOD_(nsString)             GetDataHandlerType();
  NS_IMETHOD                        SetDataHandlerType(nsString& TypeString);

private:
  nsVoidArray                       theDataSource;
  nsString                          DataHandlerType;
};

class nsXPFCDataCollectionManager : public nsIXPFCDataCollectionManager
{

public:
  nsXPFCDataCollectionManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                        Init();
  NS_IMETHOD                        AddDataCollection(nsString& DataHandlerName, nsIApplicationShell *aHostShell);
  NS_IMETHOD                        CallDataHandler(nsString& DataHandlerName, nsICollectedData* TheCollectedData);

protected:
  ~nsXPFCDataCollectionManager();

private:
  nsIArray * mDataHands;

};

#endif /* nsXPFCToolbarManager_h___ */
