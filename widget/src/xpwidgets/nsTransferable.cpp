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

#include "nsTransferable.h"
#include "nsString.h"
#include "nsDataFlavor.h"
#include "nsWidgetsCID.h"
#include "nsVoidArray.h"
#include "nsIFormatConverter.h"
#include "nsVoidArray.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
 

static NS_DEFINE_IID(kITransferableIID,        NS_ITRANSFERABLE_IID);


NS_IMPL_ADDREF(nsTransferable)
NS_IMPL_RELEASE(nsTransferable)

typedef struct {
  nsString * mFlavor;
  char *     mData;
  PRUint32   mDataLen;
} DataStruct;

//-------------------------------------------------------------------------
//
// Transferable constructor
//
//-------------------------------------------------------------------------
nsTransferable::nsTransferable()
{
  NS_INIT_REFCNT();
  mDataArray  = new nsVoidArray();
}

//-------------------------------------------------------------------------
//
// Transferable destructor
//
//-------------------------------------------------------------------------
nsTransferable::~nsTransferable()
{
  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (data) {
      delete data->mFlavor;
      if (data->mData) {
        delete[] data->mData;
      }
      delete data;
    }
  }
  delete mDataArray;
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsTransferable::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kITransferableIID)) {
    *aInstancePtr = (void*) ((nsITransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferDataFlavors(nsVoidArray ** aDataFlavorList)
{
  nsVoidArray * array = new nsVoidArray();
  if (nsnull != array) {
    PRInt32 i;
    for (i=0;i<mDataArray->Count();i++) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      array->AppendElement(data->mFlavor);    
    }
    *aDataFlavorList = array;
  } else {
    aDataFlavorList = nsnull;
  }
  return NS_OK;
}

/**
  * The transferable owns the data (memory) and only gives the aData a copy of the pointer address to it.
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferData(nsString * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (aDataFlavor->Equals(*data->mFlavor)) {
      *aData    = data->mData;
      *aDataLen = data->mDataLen;
      if (nsnull != data->mData && data->mDataLen > 0) {
        return NS_OK;
      }
    }
  }

  if ( mFormatConv ) {
    for (i=0;i<mDataArray->Count();i++) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      if (NS_OK == mFormatConv->CanConvert(data->mFlavor, aDataFlavor)) {
        mFormatConv->Convert(data->mFlavor, data->mData, data->mDataLen, aDataFlavor, aData, aDataLen);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

/**
  * The transferable now owns the data (the memory pointing to it)
  *
  */
NS_IMETHODIMP nsTransferable::SetTransferData(nsString * aDataFlavor, void * aData, PRUint32 aDataLen)
{
  if (aData == nsnull) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (aDataFlavor->Equals(*data->mFlavor)) {
      if (nsnull != data->mData) {
        delete[] data->mData;
      }
      data->mData     = (char *)aData;
      data->mDataLen  = aDataLen;
      return NS_OK;
    }
  }

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::AddDataFlavor(nsString * aDataFlavor)
{
  if (nsnull == aDataFlavor) {
    return NS_ERROR_FAILURE;
  }

  // Do we have the data flavor already?
  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (aDataFlavor->Equals(*data->mFlavor)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Create a new "slot" for the data
  DataStruct * data = new DataStruct;
  data->mFlavor     = new nsString(*aDataFlavor);
  data->mData       = nsnull;
  data->mDataLen    = 0;

  mDataArray->AppendElement((void *)data);

  return NS_OK;
}
/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::RemoveDataFlavor(nsString * aDataFlavor)
{
  if (nsnull == aDataFlavor) {
    return NS_ERROR_FAILURE;
  }

  // Do we have the data flavor already?
  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (aDataFlavor->Equals(*data->mFlavor)) {
      delete data->mFlavor;
      if (data->mData) {
        delete[] data->mData;
      }
      mDataArray->RemoveElementAt(i);
      return NS_OK;
    }
  }


  return NS_ERROR_FAILURE;
}

/**
  * 
  *
  */
NS_IMETHODIMP_(PRBool) nsTransferable::IsLargeDataSet()
{
  return PR_FALSE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::SetConverter(nsIFormatConverter * aConverter)
{
  mFormatConv = dont_QueryInterface(aConverter);
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetConverter(nsIFormatConverter ** aConverter)
{
  if ( mFormatConv ) {
    *aConverter = mFormatConv;
    NS_ADDREF(*aConverter);
  } else {
    *aConverter = nsnull;
  }
  return NS_OK;
}


//
// FlavorsTransferableCanImport
//
// Computes a list of flavors that the transferable can accept into it, either through
// intrinsic knowledge or input data converters.
//
NS_IMETHODIMP
nsTransferable :: FlavorsTransferableCanImport ( nsVoidArray** outFlavorList )
{
  if ( !outFlavorList )
    return NS_ERROR_INVALID_ARG;
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go, looking for the desired flavor.
  GetTransferDataFlavors(outFlavorList);  // addrefs
  nsCOMPtr<nsIFormatConverter> converter;
  GetConverter(getter_AddRefs(converter));
  if ( converter ) {
    nsVoidArray * convertedList;
    converter->GetInputDataFlavors(&convertedList);
    if ( nsnull != convertedList ) {
      PRUint32 i;
      PRUint32 cnt = convertedList->Count();
      for (i=0;i<cnt;i++) {
  	    nsString * temp = (nsString *)convertedList->ElementAt(i);
        (*outFlavorList)->AppendElement(temp);    
      } // foreach flavor that can be converted to
      delete convertedList;
    }
  } // if a converter exists

  return NS_OK;
  
} // FlavorsTransferableCanImport


//
// FlavorsTransferableCanExport
//
// Computes a list of flavors that the transferable can export, either through
// intrinsic knowledge or output data converters.
//
NS_IMETHODIMP
nsTransferable :: FlavorsTransferableCanExport ( nsVoidArray** outFlavorList )
{
  if ( !outFlavorList )
    return NS_ERROR_INVALID_ARG;
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go, looking for the desired flavor.
  GetTransferDataFlavors(outFlavorList);  // addrefs
  nsCOMPtr<nsIFormatConverter> converter;
  GetConverter(getter_AddRefs(converter));
  if ( converter ) {
    nsVoidArray * convertedList;
    converter->GetOutputDataFlavors(&convertedList);
    if ( nsnull != convertedList ) {
      PRUint32 i;
      PRUint32 cnt = convertedList->Count();
      for (i=0;i<cnt;i++) {
  	    nsString * temp = (nsString *)convertedList->ElementAt(i);
        (*outFlavorList)->AppendElement(temp);    // this addref's for us
      } // foreach flavor that can be converted to
    }
  } // if a converter exists

  return NS_OK;
  
} // FlavorsTransferableCanImport

