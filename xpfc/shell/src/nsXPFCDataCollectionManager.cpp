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

#include "prmem.h"
#include "nsxpfcCIID.h"
#include "nsxpfcutil.h"
#include "nsXPFCDataCollectionManager.h"
#include "nsIApplicationShell.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCDataCollectionManagerCID, NS_XPFCDATACOLLECTION_MANAGER_CID);
static NS_DEFINE_IID(kIXPFCDataCollectionManagerIID, NS_IXPFCDATACOLLECTION_MANAGER_IID);

typedef struct Data_ManagerList
{
  nsString*                   HandlerName;
  nsIApplicationShell*        HandleShell; 
} DataManagerListStruct, *DataManagerList;

nsXPFCDataCollectionManager :: nsXPFCDataCollectionManager()
{
  NS_INIT_REFCNT();
}

nsXPFCDataCollectionManager :: ~nsXPFCDataCollectionManager()
{
  if (mDataHands != nsnull) 
  {
    for(PRInt32 x = 0; 
        x < mDataHands->Count();
        x++)
    {
      DataManagerList dml = (DataManagerList)(mDataHands->ElementAt(x));
      PR_DELETE(dml);
    }
    mDataHands->RemoveAll();
    NS_RELEASE(mDataHands);
  }
}

NS_IMPL_QUERY_INTERFACE(nsXPFCDataCollectionManager, kIXPFCDataCollectionManagerIID)
NS_IMPL_ADDREF(nsXPFCDataCollectionManager)
NS_IMPL_RELEASE(nsXPFCDataCollectionManager)

nsresult nsXPFCDataCollectionManager :: Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                     nsnull, 
                                     kCVectorCID, 
                                     (void **)&mDataHands);

  if (NS_OK != res)
    return res ;

  mDataHands->Init();

  return NS_OK ;
}

nsresult
nsXPFCDataCollectionManager::AddDataCollection(nsString& DataHandlerName, nsIApplicationShell *aHostShell)
{
  DataManagerList dml = PR_NEW(DataManagerListStruct);

  if (dml)
  {
    dml->HandleShell = aHostShell;
    dml->HandlerName = new nsString(DataHandlerName);
    mDataHands->Append((void *)dml);
  }

  return NS_OK;
}

nsresult
nsXPFCDataCollectionManager :: CallDataHandler(nsString& DataHandlerName, nsICollectedData* TheCollectedData)
{
  for(PRInt32 x = 0; x < mDataHands->Count(); x++)
  {
    DataManagerList dml;
   
    dml = (DataManagerList)(mDataHands->ElementAt(x));
    if ((*dml->HandlerName) == DataHandlerName)
    {
      TheCollectedData->SetDataHandlerType(DataHandlerName);
      (dml->HandleShell)->ReceiveCallback(*TheCollectedData);
    }
  }

  return NS_OK ;
}

/********************************************************************************************
***
    Collected Data Code
***
********************************************************************************************/
static NS_DEFINE_IID(kICollectedDataIID, NS_IXPFCDATACOLLECTION_MANAGER_IID);

nsCollectedData::nsCollectedData()
{
    NS_INIT_REFCNT();
}

nsCollectedData::~nsCollectedData()
{
}

NS_IMPL_QUERY_INTERFACE(nsCollectedData, kICollectedDataIID)
NS_IMPL_ADDREF(nsCollectedData)
NS_IMPL_RELEASE(nsCollectedData)

nsresult
nsCollectedData::Init()
{
  return NS_OK ;
}

nsVoidArray*
nsCollectedData::GetDataArray()
{
  return &theDataSource;
}

nsresult
nsCollectedData::FindValueInCollectedData(nsString& Label, nsString& result)
{
  result = "";

  for(PRInt32 x = 0; x < theDataSource.Count(); x++)
  {
    nsString a,b;
    CollectedDataPtr c;
    c = (CollectedDataPtr)(theDataSource.ElementAt(x));
    a = c->LabelName;
    b = c->Value;

    if (a.RFind(Label, PR_TRUE) != -1)
    {
      result = b;
      break;
    }
  }

  return NS_OK;
}

nsString
nsCollectedData::GetDataHandlerType()
{
  return DataHandlerType;
}

nsresult
nsCollectedData::SetDataHandlerType(nsString& TypeString)
{
  DataHandlerType = TypeString;
  return NS_OK;
}
