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

#include "nsFileSpec.h"
#include "nsFileListTransferable.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsVoidArray.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
 
static NS_DEFINE_IID(kCDataFlavorCID, NS_DATAFLAVOR_CID);


NS_IMPL_ADDREF(nsFileListTransferable)
NS_IMPL_RELEASE(nsFileListTransferable)

//-------------------------------------------------------------------------
// nsFileListTransferable constructor
//-------------------------------------------------------------------------
nsFileListTransferable::nsFileListTransferable()
{
  NS_INIT_REFCNT();
  mFileList = new nsVoidArray();
  
  mFileListDataFlavor = kDropFilesMime;

}

//-------------------------------------------------------------------------
// nsFileListTransferable destructor
//-------------------------------------------------------------------------
nsFileListTransferable::~nsFileListTransferable()
{
  ClearFileList();
  delete mFileList;
}

//-------------------------------------------------------------------------
// @param aIID The name of the class implementing the method
// @param _classiiddef The name of the #define symbol that defines the IID
// for the class (e.g. NS_ISUPPORTS_IID)
//  
//-------------------------------------------------------------------------
nsresult nsFileListTransferable::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(nsCOMTypeInfo<nsITransferable>::GetIID())) {
    *aInstancePtr = (void*) ((nsITransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(nsCOMTypeInfo<nsIFileListTransferable>::GetIID())) {
    *aInstancePtr = (void*) ((nsIFileListTransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::GetTransferDataFlavors(nsVoidArray ** aDataFlavorList)
{
  nsVoidArray * array = new nsVoidArray();
  if (nsnull != array) {
    array->AppendElement(new nsString(mFileListDataFlavor));    // this addref's for us
    *aDataFlavorList = array;
  } else {
    aDataFlavorList = nsnull;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// The transferable owns the data (memory) and only gives the aData a copy of the pointer address to it.
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::GetTransferData(nsString * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{
  if (nsnull != aDataFlavor) {
    return NS_ERROR_FAILURE;
  }

  if (nsnull != mFileList && mFileListDataFlavor.Equals(*aDataFlavor)) {
    *aData    = mFileList;
    *aDataLen = mFileList->Count();
    return NS_OK;
  } else {
    *aData   = nsnull;
    aDataLen = 0;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsFileListTransferable::GetAnyTransferData(nsString * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{
  if (nsnull != aDataFlavor) {
    return NS_ERROR_FAILURE;
  }

  *aData       = mFileList;
  *aDataLen    = mFileList->Count();
  *aDataFlavor = mFileListDataFlavor;

  return NS_OK;

}
//---------------------------------------------------
// remove all the items and delete them
//---------------------------------------------------
void nsFileListTransferable::ClearFileList()
{
  if (nsnull != mFileList) {
    PRInt32 ii;
    for (ii=0;ii<mFileList->Count();ii++) {
      nsFileSpec * fileSpec = (nsFileSpec *)mFileList->ElementAt(ii);
      if (fileSpec) {
        delete[] fileSpec;
      }
    }
  }
}

//---------------------------------------------------
// The transferable now owns the data (the memory pointing to it)
//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::SetTransferData(nsString * aDataFlavor, void * aData, PRUint32 aDataLen)
{
  if (aData == nsnull &&  mFileListDataFlavor.Equals(*aDataFlavor)) {
    return NS_ERROR_FAILURE;
  }

  ClearFileList();

  mFileList = (nsVoidArray *)aData;
  return NS_OK;
}

//---------------------------------------------------
NS_IMETHODIMP_(PRBool) nsFileListTransferable::IsLargeDataSet()
{
  return PR_FALSE;
}

//---------------------------------------------------
// Copy List
//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::CopyFileList(nsVoidArray * aFromFileList,
                                                   nsVoidArray * aToFileList)
{
  PRInt32 i;
  for (i=0;i<aFromFileList->Count();i++) {
    nsFileSpec * fs = (nsFileSpec *)aFromFileList->ElementAt(i);
    nsFileSpec * newFS = new nsFileSpec(*fs);
    aToFileList->AppendElement(newFS);
  }
  return NS_OK;
}

//---------------------------------------------------
// Copies the list
//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::SetFileList(nsVoidArray * aFileList)
{
  if (nsnull != aFileList && nsnull != mFileList) {
    ClearFileList();
    CopyFileList(aFileList, mFileList);
  }

  mFileList = aFileList;

  return NS_OK;
}

//---------------------------------------------------
// Fills the list provided by the caller
//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::GetFileList(nsVoidArray * aFileList) 
{
  if (nsnull != aFileList && nsnull != mFileList) {
    CopyFileList(mFileList, aFileList);
  }

  return NS_OK;
}


//---------------------------------------------------
// FlavorsTransferableCanImport
//
// Computes a list of flavors that the transferable can accept into it, either through
// intrinsic knowledge or input data converters.
//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::FlavorsTransferableCanImport( nsVoidArray ** aOutFlavorList )
{
  if ( !aOutFlavorList )
    return NS_ERROR_INVALID_ARG;
  
  return GetTransferDataFlavors(aOutFlavorList);  // addrefs
  
} // FlavorsTransferableCanImport


//---------------------------------------------------
// FlavorsTransferableCanExport
//
// Computes a list of flavors that the transferable can export, either through
// intrinsic knowledge or output data converters.
//---------------------------------------------------
NS_IMETHODIMP
nsFileListTransferable::FlavorsTransferableCanExport( nsVoidArray** aOutFlavorList )
{
  if ( !aOutFlavorList )
    return NS_ERROR_INVALID_ARG;
  
  return GetTransferDataFlavors(aOutFlavorList);  // addrefs
  
} // FlavorsTransferableCanImport

//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::AddDataFlavor(nsString * aDataFlavor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::RemoveDataFlavor(nsString * aDataFlavor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::SetConverter(nsIFormatConverter * aConverter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//---------------------------------------------------
NS_IMETHODIMP nsFileListTransferable::GetConverter(nsIFormatConverter ** aConverter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

