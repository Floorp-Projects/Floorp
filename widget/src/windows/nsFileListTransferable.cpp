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
#include "nsIDataFlavor.h"
#include "nsITransferable.h"
#include "nsDataFlavor.h"
#include "nsWidgetsCID.h"
#include "nsISupportsArray.h"
#include "nsIFormatConverter.h"
#include "nsVoidArray.h"
#include "nsIComponentManager.h"
//#include "nsIFileListTransferable.h"
#include "nsCOMPtr.h"
 
// Platform includes
#include <windows.h>
#include <SHLOBJ.H>

//static NS_DEFINE_IID(kITransferableIID,        NS_ITRANSFERABLE_IID);
//static NS_DEFINE_IID(kIFileListTransferableIID, NS_IFILELISTTRANSFERABLE_IID);
//static NS_DEFINE_IID(kIDataFlavorIID,          NS_IDATAFLAVOR_IID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);


NS_IMPL_ADDREF(nsFileListTransferable)
NS_IMPL_RELEASE(nsFileListTransferable)

//-------------------------------------------------------------------------
//
// Transferable constructor
//
//-------------------------------------------------------------------------
nsFileListTransferable::nsFileListTransferable()
{
  NS_INIT_REFCNT();
  mFileList  = new nsVoidArray();
  
  nsresult rv = nsComponentManager::CreateInstance(kCDataFlavorCID, nsnull, 
                                                   nsIDataFlavor::GetIID(), 
                                                   (void**) getter_AddRefs(mIDListDataFlavor));
  if (NS_OK == rv) {
    mIDListDataFlavor->Init(kDropFilesMime, "ID List");
    
  }

}

//-------------------------------------------------------------------------
//
// Transferable destructor
//
//-------------------------------------------------------------------------
nsFileListTransferable::~nsFileListTransferable()
{
  ClearFileList();
  delete mFileList;
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsFileListTransferable::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(nsITransferable::GetIID())) {
    *aInstancePtr = (void*) ((nsITransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(nsIGenericTransferable::GetIID())) {
    *aInstancePtr = (void*) ((nsIGenericTransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(nsIFileListTransferable::GetIID())) {
    *aInstancePtr = (void*) ((nsIFileListTransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  nsISupportsArray * array;
  nsresult rv = NS_NewISupportsArray(&array);
  if (NS_OK == rv) {
    array->AppendElement(mIDListDataFlavor);    // this addref's for us
    *aDataFlavorList = array;
  }
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  /*PRInt32 i;
  for (i=0;i<mFileList->Count();i++) {
    DataStruct * data = (DataStruct *)mFileList->ElementAt(i);
    nsAutoString mime;
    data->mFlavor->GetMimeType(mime);
    if (mimeInQuestion.Equals(mime)) {
      return NS_OK;
    }
  }*/
  return NS_ERROR_FAILURE;
}

/**
  * The transferable owns the data (memory) and only gives the aData a copy of the pointer address to it.
  *
  */
NS_IMETHODIMP nsFileListTransferable::GetTransferData(nsIDataFlavor * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{
  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

/*  PRInt32 i;
  for (i=0;i<mFileList->Count();i++) {
    DataStruct * data = (DataStruct *)mFileList->ElementAt(i);
    nsAutoString mime;
    data->mFlavor->GetMimeType(mime);
    if (mimeInQuestion.Equals(mime)) {
       *aData    = data->mData;
       *aDataLen = data->mDataLen;
       if (nsnull != data->mData && data->mDataLen > 0) {
         return NS_OK;
       }
    }
  }

  if ( mFormatConv ) {
    for (i=0;i<mFileList->Count();i++) {
      DataStruct * data = (DataStruct *)mFileList->ElementAt(i);
      if (NS_OK == mFormatConv->CanConvert(data->mFlavor, aDataFlavor)) {
        mFormatConv->Convert(data->mFlavor, data->mData, data->mDataLen, aDataFlavor, aData, aDataLen);
        return NS_OK;
      }
    }
  }*/
  return NS_ERROR_FAILURE;
}

//---------------------------------------------------
void nsFileListTransferable::ClearFileList()
{
  PRInt32 ii;
  for (ii=0;ii<mFileList->Count();ii++) {
    nsFileSpec * fileSpec = (nsFileSpec *)mFileList->ElementAt(ii);
    if (fileSpec) {
      delete[] fileSpec;
    }
  }
}

/**
  * The transferable now owns the data (the memory pointing to it)
  *
  */
NS_IMETHODIMP nsFileListTransferable::SetTransferData(nsIDataFlavor * aDataFlavor, void * aData, PRUint32 aDataLen)
{
  if (aData == nsnull) {
    return NS_ERROR_FAILURE;
  }

  ClearFileList();

  LPDROPFILES dropFiles = (LPDROPFILES)aData;

  char fileName[1024];
  UINT numFiles = DragQueryFile(dropFiles, 0xFFFFFFFF, NULL, 0);
  for (UINT i=0;i<numFiles;i++) {
    UINT bytesCopied = ::DragQueryFile(dropFiles, i, fileName, 1024);
    //nsAutoString name((char *)fileName, bytesCopied-1);
    nsString name = fileName;
    printf("name [%s]\n", name.ToNewCString());
    nsFilePath filePath(name);
    nsFileSpec * fileSpec = new nsFileSpec(filePath);
    mFileList->AppendElement(fileSpec);
  }

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP_(PRBool) nsFileListTransferable::IsLargeDataSet()
{
  return PR_FALSE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::SetFileList(nsVoidArray * aFileList)
{
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::GetFileList(nsVoidArray ** aFileList)
{
  return NS_OK;
}


//
// FlavorsTransferableCanImport
//
// Computes a list of flavors that the transferable can accept into it, either through
// intrinsic knowledge or input data converters.
//
NS_IMETHODIMP
nsFileListTransferable :: FlavorsTransferableCanImport ( nsISupportsArray** aOutFlavorList )
{
  if ( !aOutFlavorList )
    return NS_ERROR_INVALID_ARG;
  
  return GetTransferDataFlavors(aOutFlavorList);  // addrefs
  
} // FlavorsTransferableCanImport


//
// FlavorsTransferableCanExport
//
// Computes a list of flavors that the transferable can export, either through
// intrinsic knowledge or output data converters.
//
NS_IMETHODIMP
nsFileListTransferable :: FlavorsTransferableCanExport ( nsISupportsArray** aOutFlavorList )
{
  if ( !aOutFlavorList )
    return NS_ERROR_INVALID_ARG;
  
  return GetTransferDataFlavors(aOutFlavorList);  // addrefs
  
} // FlavorsTransferableCanImport

/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::AddDataFlavor(nsIDataFlavor * aDataFlavor)
{

  return NS_OK;
}
/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::RemoveDataFlavor(nsIDataFlavor * aDataFlavor)
{
  return NS_ERROR_FAILURE;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::SetConverter(nsIFormatConverter * aConverter)
{
  return NS_ERROR_FAILURE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsFileListTransferable::GetConverter(nsIFormatConverter ** aConverter)
{
  return NS_ERROR_FAILURE;
}

