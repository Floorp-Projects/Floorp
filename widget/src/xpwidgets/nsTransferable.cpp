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
#include "nsWidgetsCID.h"
#include "nsVoidArray.h"
#include "nsIFormatConverter.h"
#include "nsVoidArray.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
 
#include "nsIFileSpec.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"

#include "nsSpecialSystemDirectory.h"

NS_IMPL_ADDREF(nsTransferable)
NS_IMPL_RELEASE(nsTransferable)

// million bytes
#define LARGE_DATASET_SIZE 1000000 
//#define LARGE_DATASET_SIZE 10 

struct DataStruct {
  DataStruct (const nsString & aString)
    : mFlavor(aString), mData(nsnull), mDataLen(0), mCacheFileName(nsnull) { }
  ~DataStruct();
  
  void SetData( char* aData, PRUint32 aDataLen );
  void GetData( char** aData, PRUint32 *aDataLen );
  nsIFileSpec * GetFileSpec(const char * aFileName);
  PRBool IsDataAvilable() { return (nsnull != mData && mDataLen > 0) || (nsnull == mData && nsnull != mCacheFileName); }
  nsString   mFlavor;

protected:
  nsresult WriteCache(char* aData, PRUint32 aDataLen );
  nsresult ReadCache(char** aData, PRUint32* aDataLen );

  
  char *   mData;
  PRUint32 mDataLen;
  char *   mCacheFileName;

};

//-------------------------------------------------------------------------
DataStruct::~DataStruct() 
{ 
  if (mData) 
    delete [] mData; 

  if (mCacheFileName) {
    delete [] mCacheFileName; 
    //nsIFileSpec * cacheFile = GetFileSpec(mCacheFileName);
    //cacheFile->remove();
  }
}

//-------------------------------------------------------------------------
void DataStruct::SetData ( char* aData, PRUint32 aDataLen )
{
  // check to see if we already have data and then delete it
  if ( mData ) {
    delete [] mData;
    mData = nsnull;
  }

  // Now, check to see if we consider the data to be "too large"
  if (aDataLen > LARGE_DATASET_SIZE) {
    // if so, cache it to disk instead of memory
    if (NS_OK == WriteCache(aData, aDataLen)) {
      printf("->>>>>>>>>>>>>> Wrote Clipboard to cache file\n");
      return;
    }
  } else {
    printf("->>>>>>>>>>>>>> Write Clipboard to memory\n");
  }
  mData    = aData;
  mDataLen = aDataLen;
  
}

//-------------------------------------------------------------------------
void DataStruct::GetData ( char** aData, PRUint32 *aDataLen )
{

  // check here to see if the data is cached on disk
  if (nsnull == mData && nsnull != mCacheFileName) {
    // if so, read it in and pass it back
    // ReadCache creates memory and copies the data into it.
    if (NS_OK == ReadCache(aData, aDataLen)) {
      printf("->>>>>>>>>>>>>> Read Clipboard from cache file\n");
      return;
    }
  } else {
    printf("->>>>>>>>>>>>>> Read Clipboard from memory\n");
  }
  // OK, we create memory and copy the contents into it.
  char * data = new char[mDataLen];
  if (nsnull != mData && mDataLen > 0) {
    memcpy(data, mData, mDataLen);
    *aData    = data;
  } else {
    // zeros it out
    *aData = nsnull;
  }
  *aDataLen = mDataLen;
}

//-------------------------------------------------------------------------
nsIFileSpec * DataStruct::GetFileSpec(const char * aFileName)
{
  nsIFileSpec* cacheFile = nsnull;
  nsresult rv;
	// There is no locator component. Or perhaps there is a locator, but the
	// locator couldn't find where to put it. So put it in the cwd (NB, viewer comes here.)
  // #include nsIComponentManager.h
  rv = nsComponentManager::CreateInstance(
      (const char*)NS_FILESPEC_PROGID,
      (nsISupports*)nsnull,
      (const nsID&)nsCOMTypeInfo<nsIFileSpec>::GetIID(),
      (void**)&cacheFile);
  NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a Clipboard Cache file spec.");

  // Get the system temp directory path
  nsSpecialSystemDirectory *sysCacheFile =  
    new nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory);

  // if the param aFileName contains a name we should use that
  // because the file probably already exists
  // otherwise create a unique name
  if (nsnull == aFileName) {
    *sysCacheFile += "clipboardcache";
    sysCacheFile->MakeUnique();
  } else {
    *sysCacheFile += aFileName;
  }

  // now set the entire path for the nsIFileSpec
  cacheFile->setFromFileSpec(*sysCacheFile);

  // delete the temp for getting the system info
  delete sysCacheFile;

  // return the nsIFileSpec
  return cacheFile;
}

//-------------------------------------------------------------------------
nsresult DataStruct::WriteCache(char* aData, PRUint32 aDataLen)
{
  // Get a new path and file to the temp directory
  nsIFileSpec * cacheFile = GetFileSpec(mCacheFileName);
  if (nsnull != cacheFile) {
    // remember the file name
    if (nsnull == mCacheFileName) {
      cacheFile->GetLeafName(&mCacheFileName);
    }
    // write out the contents of the clipboard
    // to the file
    PRUint32 bytes;
    nsIOutputStream * outStr;
    cacheFile->GetOutputStream(&outStr);
    outStr->Write(aData, aDataLen, &bytes);

    // clean up
    NS_RELEASE(outStr);
    NS_RELEASE(cacheFile);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
nsresult DataStruct::ReadCache(char** aData, PRUint32* aDataLen)
{
  // if we don't have a cache filename we are out of luck
  if (nsnull == mCacheFileName) {
    return NS_ERROR_FAILURE;
  }

  // get the path and file name
  nsIFileSpec * cacheFile = GetFileSpec(mCacheFileName);
  if (nsnull != cacheFile && Exists(cacheFile)) {
    // get the size of the file
    PRUint32 fileSize;
    cacheFile->GetFileSize(&fileSize);

    // create new memory for the large clipboard data
    char * data = new char[fileSize];

    // now read it all in
    nsIInputStream * inStr;
    cacheFile->GetInputStream(&inStr);
    nsresult rv = inStr->Read(data, fileSize, aDataLen);
    // clean up file & stream
    NS_RELEASE(inStr);
    NS_RELEASE(cacheFile);

    // make sure we got all the data ok
    if (NS_OK == rv && *aDataLen == (PRUint32)fileSize) {
      *aDataLen = fileSize;
      *aData    = data;
      return NS_OK;
    }

    // delete the buffer because we got an error
    // and zero the return params
    delete[] data;
    *aData    = nsnull;
    *aDataLen = 0;
    return NS_ERROR_FAILURE;
  }
  // this is necessary because we may have created
  // a proper cacheFile but it might not exist
  NS_IF_RELEASE(cacheFile);

  return NS_ERROR_FAILURE;
}


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
    if (data)
      delete data;
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

  if (aIID.Equals(nsCOMTypeInfo<nsITransferable>::GetIID())) {
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
  if (nsnull == aDataFlavorList) {
    return NS_ERROR_FAILURE;
  }

  nsVoidArray * array = new nsVoidArray();
  if (nsnull != array) {
    PRInt32 i;
    for (i=0;i<mDataArray->Count();i++) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      array->AppendElement(&data->mFlavor);    
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
  if (nsnull == aDataFlavor || nsnull == aData || nsnull == aDataLen) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (aDataFlavor->Equals(data->mFlavor)) {
      data->GetData((char **)aData, aDataLen);
      if (nsnull != *aData && *aDataLen > 0) {
        return NS_OK;
      }
    }
  }

  if ( mFormatConv ) {
    for (i=0;i<mDataArray->Count();i++) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      if (NS_OK == mFormatConv->CanConvert(&data->mFlavor, aDataFlavor)) {
        char * dataBytes;
        PRUint32 len;
        data->GetData(&dataBytes, &len);
        mFormatConv->Convert(&data->mFlavor, dataBytes, len, aDataFlavor, aData, aDataLen);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

/**
  * The transferable owns the data (memory) and only gives the aData a copy of the pointer address to it.
  *
  */
NS_IMETHODIMP nsTransferable::GetAnyTransferData(nsString * aDataFlavor, void ** aData, PRUint32 * aDataLen)
{
  if (nsnull == aDataFlavor || nsnull == aData || nsnull == aDataLen) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (data->IsDataAvilable()) {
      *aDataFlavor = data->mFlavor;
      data->GetData((char **)aData, aDataLen);
      return NS_OK;
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
  if (nsnull == aDataFlavor) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (aDataFlavor->Equals(data->mFlavor)) {
      data->SetData ( NS_STATIC_CAST(char*,aData), aDataLen );
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
    if (aDataFlavor->Equals(data->mFlavor)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Create a new "slot" for the data
  DataStruct * data = new DataStruct ( *aDataFlavor ) ;
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
    if (aDataFlavor->Equals(data->mFlavor)) {
      mDataArray->RemoveElementAt(i);
      delete data;
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

