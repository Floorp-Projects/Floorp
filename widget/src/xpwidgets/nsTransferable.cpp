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

/*
Notes to self:

- fix QI
- should DataStruct.mFlavor be a string or a nsISupportsString internally? Would save
   on having to recreate it each time it is asked for externally. Would complicate things
   if this the list is used internally a lot 

*/

 
#include "nsTransferable.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFormatConverter.h"
#include "nsVoidArray.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
 
#include "nsIFileSpec.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"

#include "nsSpecialSystemDirectory.h"

NS_IMPL_ADDREF(nsTransferable)
NS_IMPL_RELEASE(nsTransferable)
// NS_IMPL_QUERY_INTERFACE1(nsITransferable)
NS_IMPL_QUERY_INTERFACE(nsTransferable, NS_GET_IID(nsITransferable))


// million bytes
#define LARGE_DATASET_SIZE 1000000 
//#define LARGE_DATASET_SIZE 10 


struct DataStruct
{
  DataStruct ( const char* aFlavor )
    : mFlavor(aFlavor), mDataLen(0), mCacheFileName(nsnull) { }
  ~DataStruct();
  
  void SetData( nsISupports* inData, PRUint32 inDataLen );
  void GetData( nsISupports** outData, PRUint32 *outDataLen );
  nsIFileSpec * GetFileSpec(const char * aFileName);
  PRBool IsDataAvilable() { return (mData && mDataLen > 0) || (!mData && mCacheFileName); }
  nsString   mFlavor;

protected:
  nsresult WriteCache(nsISupports* aData, PRUint32 aDataLen );
  nsresult ReadCache(nsISupports** aData, PRUint32* aDataLen );
  
  nsCOMPtr<nsISupports> mData;   // OWNER - some varient of primitive wrapper
  PRUint32 mDataLen;
  char *   mCacheFileName;

};


//-------------------------------------------------------------------------
DataStruct::~DataStruct() 
{ 
  delete [] mCacheFileName; 
    //nsIFileSpec * cacheFile = GetFileSpec(mCacheFileName);
    //cacheFile->Remove();
}

//-------------------------------------------------------------------------
void
DataStruct::SetData ( nsISupports* aData, PRUint32 aDataLen )
{
  // Now, check to see if we consider the data to be "too large"
  if (aDataLen > LARGE_DATASET_SIZE) {
    // if so, cache it to disk instead of memory
    if ( NS_SUCCEEDED(WriteCache(aData, aDataLen)) ) {
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
void
DataStruct::GetData ( nsISupports** aData, PRUint32 *aDataLen )
{
  // check here to see if the data is cached on disk
  if ( !mData && mCacheFileName ) {
    // if so, read it in and pass it back
    // ReadCache creates memory and copies the data into it.
    if ( NS_SUCCEEDED(ReadCache(aData, aDataLen)) ) {
      printf("->>>>>>>>>>>>>> Read Clipboard from cache file\n");
      return;
    }
  } else {
    printf("->>>>>>>>>>>>>> Read Clipboard from memory\n");
  }
  
  *aData = mData;
  NS_ADDREF(*aData); 
  *aDataLen = mDataLen;
}


//-------------------------------------------------------------------------
nsIFileSpec*
DataStruct::GetFileSpec(const char * aFileName)
{
  nsIFileSpec* cacheFile = nsnull;
  nsresult rv = nsComponentManager::CreateInstance( NS_FILESPEC_PROGID, nsnull, NS_GET_IID(nsIFileSpec),
                                                     NS_REINTERPRET_CAST(void**,&cacheFile));
  NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a Clipboard Cache file spec.");

  // Get the system temp directory path
  nsSpecialSystemDirectory *sysCacheFile =  
    new nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory);

  // if the param aFileName contains a name we should use that
  // because the file probably already exists
  // otherwise create a unique name
  if (!aFileName) {
    *sysCacheFile += "clipboardcache";
    sysCacheFile->MakeUnique();
  } else {
    *sysCacheFile += aFileName;
  }

  // now set the entire path for the nsIFileSpec
  cacheFile->SetFromFileSpec(*sysCacheFile);

  // delete the temp for getting the system info
  delete sysCacheFile;

  // return the nsIFileSpec. The addref comes from CreateInstance()
  return cacheFile;
}


//-------------------------------------------------------------------------
nsresult
DataStruct::WriteCache(nsISupports* aData, PRUint32 aDataLen)
{
  // Get a new path and file to the temp directory
  nsCOMPtr<nsIFileSpec> cacheFile ( getter_AddRefs(GetFileSpec(mCacheFileName)) );
  if (cacheFile) {
    // remember the file name
    if (!mCacheFileName)
      cacheFile->GetLeafName(&mCacheFileName);

    // write out the contents of the clipboard
    // to the file
    //PRUint32 bytes;
    nsCOMPtr<nsIOutputStream> outStr;
    cacheFile->GetOutputStream( getter_AddRefs(outStr) );
    
    //XXX broken until i can stream/inflate these primitive objects.
    //outStr->Write(aData, aDataLen, &bytes);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
nsresult
DataStruct::ReadCache(nsISupports** aData, PRUint32* aDataLen)
{
  // if we don't have a cache filename we are out of luck
  if (!mCacheFileName)
    return NS_ERROR_FAILURE;

  // get the path and file name
  nsCOMPtr<nsIFileSpec> cacheFile ( getter_AddRefs(GetFileSpec(mCacheFileName)) );
  if ( cacheFile && Exists(cacheFile)) {
    // get the size of the file
    PRUint32 fileSize;
    cacheFile->GetFileSize(&fileSize);

    // create new memory for the large clipboard data
    char * data = new char[fileSize];
    if ( !data )
      return NS_ERROR_OUT_OF_MEMORY;
      
    // now read it all in
    nsCOMPtr<nsIInputStream> inStr;
    cacheFile->GetInputStream( getter_AddRefs(inStr) );
    nsresult rv = inStr->Read(data, fileSize, aDataLen);

    // make sure we got all the data ok
    if ( NS_SUCCEEDED(rv) && *aDataLen == (PRUint32)fileSize) {
      *aDataLen = fileSize;
      
      //XXX broken until i can inflate primitive objects
      //*aData    = data;
      return NS_OK;
    }

    // delete the buffer because we got an error
    // and zero the return params
    delete[] data;
    *aData    = nsnull;
    *aDataLen = 0;
  }

  return NS_ERROR_FAILURE;
}


#pragma mark -


//-------------------------------------------------------------------------
//
// Transferable constructor
//
//-------------------------------------------------------------------------
nsTransferable::nsTransferable()
{
  NS_INIT_REFCNT();
  mDataArray = new nsVoidArray();
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
    delete data;
  }
  delete mDataArray;
}


//
// GetTransferDataFlavors
//
// Returns a copy of the internal list of flavors. This does NOT take into 
// account any converter that may be registered. This list consists of
// nsISupportsString objects so that the flavor list can be accessed from JS.
//
NS_IMETHODIMP
nsTransferable :: GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  if (!aDataFlavorList)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = NS_OK;
  
  NS_NewISupportsArray ( aDataFlavorList );
  if ( *aDataFlavorList ) {
    for ( PRInt32 i=0; i<mDataArray->Count(); ++i ) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      nsCOMPtr<nsISupportsString> flavorWrapper;
      rv = nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                               NS_GET_IID(nsISupportsString), getter_AddRefs(flavorWrapper));
      if ( flavorWrapper ) {
        char* tempBecauseNSStringIsLame = data->mFlavor.ToNewCString();
        flavorWrapper->SetData ( tempBecauseNSStringIsLame );
        nsCOMPtr<nsISupports> genericWrapper ( do_QueryInterface(flavorWrapper) );
        (*aDataFlavorList)->AppendElement( genericWrapper );
        delete [] tempBecauseNSStringIsLame;
      }
    }
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
}


//
// GetTransferData
//
// Returns the data of the requested flavor, obtained from either having the data on hand or
// using a converter to get it. The data is wrapped in a nsISupports primitive so that it is
// accessable from JS.
//
NS_IMETHODIMP
nsTransferable :: GetTransferData(const char *aFlavor, nsISupports **aData, PRUint32 *aDataLen)
{
  if ( !aFlavor || !aData ||!aDataLen )
    return NS_ERROR_INVALID_ARG;

  PRBool found = PR_FALSE;
  
  // first look and see if the data is present in one of the intrinsic flavors
  PRInt32 i;
  for ( i=0; i<mDataArray->Count(); ++i ) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if ( data->mFlavor.Equals(aFlavor) ) {
      data->GetData(aData, aDataLen);
      if (*aData && *aDataLen > 0)
        found = PR_TRUE;
    }
  }

  // if not, try using a format converter to get the requested flavor
  if ( !found && mFormatConv ) {
    for (i=0;i<mDataArray->Count();i++) {
      DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
      char* tempBecauseNSStringIsLame = data->mFlavor.ToNewCString();
      PRBool canConvert = PR_FALSE;
      mFormatConv->CanConvert(tempBecauseNSStringIsLame, aFlavor, &canConvert);
      if ( canConvert ) {
        nsCOMPtr<nsISupports> dataBytes;
        PRUint32 len;
        data->GetData(getter_AddRefs(dataBytes), &len);
        mFormatConv->Convert(tempBecauseNSStringIsLame, dataBytes, len, aFlavor, aData, aDataLen);
        found = PR_TRUE;
      }
      delete [] tempBecauseNSStringIsLame;
    }
  }
  return found ? NS_OK : NS_ERROR_FAILURE;
}


//
// GetAnyTransferData
//
// Returns the data of the first flavor found. Caller is responsible for deleting the
// flavor string.
// 
NS_IMETHODIMP
nsTransferable::GetAnyTransferData(char **aFlavor, nsISupports **aData, PRUint32 *aDataLen)
{
  if ( !aFlavor || !aData || !aDataLen )
    return NS_ERROR_FAILURE;

  for ( PRInt32 i=0; i < mDataArray->Count(); ++i ) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if (data->IsDataAvilable()) {
      *aFlavor = data->mFlavor.ToNewCString();
      data->GetData(aData, aDataLen);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


//
// SetTransferData
//
//
// 
NS_IMETHODIMP
nsTransferable::SetTransferData(const char *aFlavor, nsISupports *aData, PRUint32 aDataLen)
{
  if ( !aFlavor )
    return NS_ERROR_FAILURE;

  for ( PRInt32 i=0; i<mDataArray->Count(); ++i ) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if ( data->mFlavor.Equals(aFlavor) ) {
      data->SetData ( aData, aDataLen );
      return NS_OK;
    }
  }

  return NS_OK;
}


//
// AddDataFlavor
//
// Adds a data flavor to our list with no data. Error if it already exists.
// 
NS_IMETHODIMP
nsTransferable :: AddDataFlavor(const char *aDataFlavor)
{
  // Do we have the data flavor already?
  PRInt32 i;
  for (i=0;i<mDataArray->Count();i++) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if ( data->mFlavor.Equals(aDataFlavor) )
      return NS_ERROR_FAILURE;
  }

  // Create a new "slot" for the data
  DataStruct * data = new DataStruct ( aDataFlavor ) ;
  mDataArray->AppendElement((void *)data);

  return NS_OK;
}


//
// RemoveDataFlavor
//
// Removes a data flavor (and causes the data to be destroyed). Error if
// the requested flavor is not present.
//
NS_IMETHODIMP
nsTransferable::RemoveDataFlavor(const char *aDataFlavor)
{
  // Do we have the data flavor already?
  for ( PRInt32 i=0; i<mDataArray->Count(); ++i ) {
    DataStruct * data = (DataStruct *)mDataArray->ElementAt(i);
    if ( data->mFlavor.Equals(aDataFlavor) ) {
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
NS_IMETHODIMP
nsTransferable::IsLargeDataSet(PRBool *_retval)
{
  if ( !_retval )
    return NS_ERROR_FAILURE;
  
  *_retval = PR_FALSE;
  
  return NS_OK;
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
NS_IMETHODIMP nsTransferable::GetConverter(nsIFormatConverter * *aConverter)
{
  if ( !aConverter )
    return NS_ERROR_FAILURE;

  if ( mFormatConv ) {
    *aConverter = mFormatConv;
    NS_ADDREF(*aConverter);
  } else
    *aConverter = nsnull;

  return NS_OK;
}


//
// FlavorsTransferableCanImport
//
// Computes a list of flavors that the transferable can accept into it, either through
// intrinsic knowledge or input data converters.
//
NS_IMETHODIMP
nsTransferable :: FlavorsTransferableCanImport(nsISupportsArray **_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go, looking for the desired flavor.
  GetTransferDataFlavors(_retval);  // addrefs
  nsCOMPtr<nsIFormatConverter> converter;
  GetConverter(getter_AddRefs(converter));
  if ( converter ) {
    nsCOMPtr<nsISupportsArray> convertedList;
    converter->GetInputDataFlavors(getter_AddRefs(convertedList));
    if ( convertedList ) {
      PRUint32 importListLen;
      convertedList->Count(&importListLen);
      for ( PRUint32 i=0; i < importListLen; ++i ) {
        nsCOMPtr<nsISupports> genericFlavor;
        convertedList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
        (*_retval)->AppendElement(genericFlavor);
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
nsTransferable :: FlavorsTransferableCanExport(nsISupportsArray **_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go, looking for the desired flavor.
  GetTransferDataFlavors(_retval);  // addrefs
  nsCOMPtr<nsIFormatConverter> converter;
  GetConverter(getter_AddRefs(converter));
  if ( converter ) {
    nsCOMPtr<nsISupportsArray> convertedList;
    converter->GetOutputDataFlavors(getter_AddRefs(convertedList));
    PRUint32 importListLen;
    if ( convertedList ) {
      convertedList->Count(&importListLen);
      for ( PRUint32 i=0; i < importListLen; ++i ) {
        nsCOMPtr<nsISupports> genericFlavor;
        convertedList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
        (*_retval)->AppendElement(genericFlavor);
      } // foreach flavor that can be converted to
    }
  } // if a converter exists

  return NS_OK;
  
} // FlavorsTransferableCanImport

