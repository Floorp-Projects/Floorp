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
#include "nsIDataFlavor.h"
#include "nsDataFlavor.h"
#include "nsWidgetsCID.h"
#include "nsISupportsArray.h"
#include "nsIFormatConverter.h"
#include "nsVoidArray.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
 

static NS_DEFINE_IID(kITransferableIID,        NS_ITRANSFERABLE_IID);
static NS_DEFINE_IID(kIGenericTransferableIID, NS_IGENERICTRANSFERABLE_IID);
static NS_DEFINE_IID(kIDataFlavorIID,          NS_IDATAFLAVOR_IID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);


NS_IMPL_ADDREF(nsTransferable)
NS_IMPL_RELEASE(nsTransferable)

typedef struct {
  nsIDataFlavor * mFlavor;
  char *   mData;
  PRUint32 mDataLen;
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
    NS_RELEASE(data->mFlavor);
    if (data->mData) {
      delete[] data->mData;
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

  if (aIID.Equals(kIGenericTransferableIID)) {
    *aInstancePtr = (void*) ((nsIGenericTransferable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  nsISupportsArray * array;
  nsresult rv = NS_NewISupportsArray(&array);
  if (NS_OK == rv) {
    PRInt32 i;
    for (i=0;i<mDataArray->Count();i++) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      array->AppendElement(data->mFlavor);    // this addref's for us
    }
    *aDataFlavorList = array;
  }
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    nsAutoString mime;
    data->mFlavor->GetMimeType(mime);
    if (mimeInQuestion.Equals(mime)) {
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetTransferData(nsIDataFlavor * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{
  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    nsAutoString mime;
    data->mFlavor->GetMimeType(mime);
    if (mimeInQuestion.Equals(mime)) {
       *aData    = data->mData;
       *aDataLen = data->mDataLen;
       return NS_OK;
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
  * 
  *
  */
NS_IMETHODIMP nsTransferable::SetTransferData(nsIDataFlavor * aDataFlavor, void * aData, PRUint32 aDataLen)
{
  if (aData == nsnull) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    nsAutoString mime;
    data->mFlavor->GetMimeType(mime);
    if (mimeInQuestion.Equals(mime)) {
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
NS_IMETHODIMP nsTransferable::AddDataFlavor(nsIDataFlavor * aDataFlavor)
{
  if (nsnull == aDataFlavor) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString  mimeInQuestion;
  aDataFlavor->GetMimeType(mimeInQuestion);

  // Do we have the data flavor already?
  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    nsAutoString mime;
    data->mFlavor->GetMimeType(mime);
    if (mimeInQuestion.Equals(mime)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Create a new "slot" for the data
  DataStruct * data = new DataStruct;
  data->mFlavor     = aDataFlavor;
  data->mData       = nsnull;
  data->mDataLen    = 0;

  NS_ADDREF(aDataFlavor);

  mDataArray->AppendElement((void *)data);

  return NS_OK;
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
nsTransferable :: FlavorsTransferableCanImport ( nsISupportsArray** outFlavorList )
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
    nsCOMPtr<nsISupportsArray> convertedList;
    converter->GetInputDataFlavors(getter_AddRefs(convertedList));
    if ( convertedList ) {
      PRUint32 i;
      for (i=0;i<convertedList->Count();i++) {
  	    nsCOMPtr<nsISupports> temp = getter_AddRefs(convertedList->ElementAt(i));
        (*outFlavorList)->AppendElement(temp);    // this addref's for us
      } // foreach flavor that can be converted to
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
nsTransferable :: FlavorsTransferableCanExport ( nsISupportsArray** outFlavorList )
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
    nsCOMPtr<nsISupportsArray> convertedList;
    converter->GetOutputDataFlavors(getter_AddRefs(convertedList));
    if ( convertedList ) {
      PRUint32 i;
      for (i=0;i<convertedList->Count();i++) {
  	    nsCOMPtr<nsISupports> temp = getter_AddRefs(convertedList->ElementAt(i));
        (*outFlavorList)->AppendElement(temp);    // this addref's for us
      } // foreach flavor that can be converted to
    }
  } // if a converter exists

  return NS_OK;
  
} // FlavorsTransferableCanImport

