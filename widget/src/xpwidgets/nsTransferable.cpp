/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Pinkerton (pinkerton@netscape.com)
 *   Dainis Jonitis (Dainis_Jonitis@swh-t.lv)
 *   Mats Palmgren <matpal@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
Notes to self:

- at some point, strings will be accessible from JS, so we won't have to wrap
   flavors in an nsISupportsCString. Until then, we're kinda stuck with
   this crappy API of nsISupportsArrays.

*/

 
#include "nsTransferable.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsIFormatConverter.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsMemory.h"
#include "nsPrimitiveHelpers.h"
#include "nsXPIDLString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsCRT.h" 
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"

NS_IMPL_ISUPPORTS1(nsTransferable, nsITransferable)

PRUint32 GetDataForFlavor (const nsTArray<DataStruct>& aArray,
                           const char* aDataFlavor)
{
  for (PRUint32 i = 0 ; i < aArray.Length () ; ++i) {
    if (aArray[i].GetFlavor().Equals (aDataFlavor))
      return i;
  }

  return aArray.NoIndex;
}

//-------------------------------------------------------------------------
DataStruct::~DataStruct() 
{ 
  if (mCacheFileName) nsCRT::free(mCacheFileName); 

    //nsIFileSpec * cacheFile = GetFileSpec(mCacheFileName);
    //cacheFile->Remove();
}

//-------------------------------------------------------------------------
void
DataStruct::SetData ( nsISupports* aData, PRUint32 aDataLen )
{
  // Now, check to see if we consider the data to be "too large"
  if (aDataLen > kLargeDatasetSize) {
    // if so, cache it to disk instead of memory
    if ( NS_SUCCEEDED(WriteCache(aData, aDataLen)) )
      return;
    else
			NS_WARNING("Oh no, couldn't write data to the cache file");   
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
    if ( NS_SUCCEEDED(ReadCache(aData, aDataLen)) )
      return;
    else {
      // oh shit, something went horribly wrong here.
      NS_WARNING("Oh no, couldn't read data in from the cache file");
      *aData = nsnull;
      *aDataLen = 0;
      return;
    }
  }
  
  *aData = mData;
  if ( mData )
    NS_ADDREF(*aData); 
  *aDataLen = mDataLen;
}


//-------------------------------------------------------------------------
nsIFile*
DataStruct::GetFileSpec(const char * aFileName)
{
  nsIFile* cacheFile;
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, &cacheFile);
  
  if (cacheFile == nsnull)
    return nsnull;

  // if the param aFileName contains a name we should use that
  // because the file probably already exists
  // otherwise create a unique name
  if (!aFileName) {
    cacheFile->AppendNative(NS_LITERAL_CSTRING("clipboardcache"));
    cacheFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  } else {
    cacheFile->AppendNative(nsDependentCString(aFileName));
  }
  
  return cacheFile;
}


//-------------------------------------------------------------------------
nsresult
DataStruct::WriteCache(nsISupports* aData, PRUint32 aDataLen)
{
  // Get a new path and file to the temp directory
  nsCOMPtr<nsIFile> cacheFile ( getter_AddRefs(GetFileSpec(mCacheFileName)) );
  if (cacheFile) {
    // remember the file name
    if (!mCacheFileName) {
      nsXPIDLCString fName;
      cacheFile->GetNativeLeafName(fName);
      mCacheFileName = nsCRT::strdup(fName);
    }

    // write out the contents of the clipboard
    // to the file
    //PRUint32 bytes;
    nsCOMPtr<nsIOutputStream> outStr;

    NS_NewLocalFileOutputStream(getter_AddRefs(outStr),
                                cacheFile);

    if (!outStr) return NS_ERROR_FAILURE;

    void* buff = nsnull;
    nsPrimitiveHelpers::CreateDataFromPrimitive ( mFlavor.get(), aData, &buff, aDataLen );
    if ( buff ) {
      PRUint32 ignored;
      outStr->Write(reinterpret_cast<char*>(buff), aDataLen, &ignored);
      nsMemory::Free(buff);
      return NS_OK;
    }
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
  nsCOMPtr<nsIFile> cacheFile ( getter_AddRefs(GetFileSpec(mCacheFileName)) );
  bool exists;
  if ( cacheFile && NS_SUCCEEDED(cacheFile->Exists(&exists)) && exists ) {
    // get the size of the file
    PRInt64 fileSize;
    PRInt64 max32(LL_INIT(0, 0xFFFFFFFF));
    cacheFile->GetFileSize(&fileSize);
    if (LL_CMP(fileSize, >, max32))
      return NS_ERROR_OUT_OF_MEMORY;
    PRUint32 size;
    LL_L2UI(size, fileSize);

    // create new memory for the large clipboard data
    nsAutoArrayPtr<char> data(new char[size]);
    if ( !data )
      return NS_ERROR_OUT_OF_MEMORY;
      
    // now read it all in
    nsCOMPtr<nsIInputStream> inStr;
    NS_NewLocalFileInputStream( getter_AddRefs(inStr),
                                cacheFile);
    
    if (!cacheFile) return NS_ERROR_FAILURE;

    nsresult rv = inStr->Read(data, fileSize, aDataLen);

    // make sure we got all the data ok
    if (NS_SUCCEEDED(rv) && *aDataLen == size) {
      nsPrimitiveHelpers::CreatePrimitiveForData ( mFlavor.get(), data, fileSize, aData );
      return *aData ? NS_OK : NS_ERROR_FAILURE;
    }

    // zero the return params
    *aData    = nsnull;
    *aDataLen = 0;
  }

  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
//
// Transferable constructor
//
//-------------------------------------------------------------------------
nsTransferable::nsTransferable()
{
}

//-------------------------------------------------------------------------
//
// Transferable destructor
//
//-------------------------------------------------------------------------
nsTransferable::~nsTransferable()
{
}


//
// GetTransferDataFlavors
//
// Returns a copy of the internal list of flavors. This does NOT take into 
// account any converter that may be registered. This list consists of
// nsISupportsCString objects so that the flavor list can be accessed from JS.
//
nsresult
nsTransferable::GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  nsresult rv = NS_NewISupportsArray ( aDataFlavorList );
  if (NS_FAILED(rv)) return rv;

  for ( PRUint32 i=0; i<mDataArray.Length(); ++i ) {
    DataStruct& data = mDataArray.ElementAt(i);
    nsCOMPtr<nsISupportsCString> flavorWrapper = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID);
    if ( flavorWrapper ) {
      flavorWrapper->SetData ( data.GetFlavor() );
      nsCOMPtr<nsISupports> genericWrapper ( do_QueryInterface(flavorWrapper) );
      (*aDataFlavorList)->AppendElement( genericWrapper );
    }
  }

  return NS_OK;
}


//
// GetTransferData
//
// Returns the data of the requested flavor, obtained from either having the data on hand or
// using a converter to get it. The data is wrapped in a nsISupports primitive so that it is
// accessible from JS.
//
NS_IMETHODIMP
nsTransferable::GetTransferData(const char *aFlavor, nsISupports **aData, PRUint32 *aDataLen)
{
  NS_ENSURE_ARG_POINTER(aFlavor && aData && aDataLen);

  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> savedData;
  
  // first look and see if the data is present in one of the intrinsic flavors
  PRUint32 i;
  for (i = 0; i < mDataArray.Length(); ++i ) {
    DataStruct& data = mDataArray.ElementAt(i);
    if ( data.GetFlavor().Equals(aFlavor) ) {
      nsCOMPtr<nsISupports> dataBytes;
      PRUint32 len;
      data.GetData(getter_AddRefs(dataBytes), &len);
      if (len == kFlavorHasDataProvider && dataBytes) {
        // do we have a data provider?
        nsCOMPtr<nsIFlavorDataProvider> dataProvider = do_QueryInterface(dataBytes);
        if (dataProvider) {
          rv = dataProvider->GetFlavorData(this, aFlavor,
                                           getter_AddRefs(dataBytes), &len);
          if (NS_FAILED(rv))
            break;    // the provider failed. fall into the converter code below.
        }
      }
      if (dataBytes && len > 0) { // XXXmats why is zero length not ok?
        *aDataLen = len;
        dataBytes.forget(aData);
        return NS_OK;
      }
      savedData = dataBytes;  // return this if format converter fails
      break;
    }
  }

  bool found = false;

  // if not, try using a format converter to get the requested flavor
  if ( mFormatConv ) {
    for (i = 0; i < mDataArray.Length(); ++i) {
      DataStruct& data = mDataArray.ElementAt(i);
      bool canConvert = false;
      mFormatConv->CanConvert(data.GetFlavor().get(), aFlavor, &canConvert);
      if ( canConvert ) {
        nsCOMPtr<nsISupports> dataBytes;
        PRUint32 len;
        data.GetData(getter_AddRefs(dataBytes), &len);
        if (len == kFlavorHasDataProvider && dataBytes) {
          // do we have a data provider?
          nsCOMPtr<nsIFlavorDataProvider> dataProvider = do_QueryInterface(dataBytes);
          if (dataProvider) {
            rv = dataProvider->GetFlavorData(this, aFlavor,
                                             getter_AddRefs(dataBytes), &len);
            if (NS_FAILED(rv))
              break;  // give up
          }
        }
        mFormatConv->Convert(data.GetFlavor().get(), dataBytes, len, aFlavor, aData, aDataLen);
        found = PR_TRUE;
        break;
      }
    }
  }

  // for backward compatibility
  if (!found) {
    savedData.forget(aData);
    *aDataLen = 0;
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
  NS_ENSURE_ARG_POINTER(aFlavor && aData && aDataLen);

  for ( PRUint32 i=0; i < mDataArray.Length(); ++i ) {
    DataStruct& data = mDataArray.ElementAt(i);
    if (data.IsDataAvailable()) {
      *aFlavor = ToNewCString(data.GetFlavor());
      data.GetData(aData, aDataLen);
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
  NS_ENSURE_ARG(aFlavor);

  // first check our intrinsic flavors to see if one has been registered.
  PRUint32 i = 0;
  for (i = 0; i < mDataArray.Length(); ++i) {
    DataStruct& data = mDataArray.ElementAt(i);
    if ( data.GetFlavor().Equals(aFlavor) ) {
      data.SetData ( aData, aDataLen );
      return NS_OK;
    }
  }

  // if not, try using a format converter to find a flavor to put the data in
  if ( mFormatConv ) {
    for (i = 0; i < mDataArray.Length(); ++i) {
      DataStruct& data = mDataArray.ElementAt(i);
      bool canConvert = false;
      mFormatConv->CanConvert(aFlavor, data.GetFlavor().get(), &canConvert);

      if ( canConvert ) {
        nsCOMPtr<nsISupports> ConvertedData;
        PRUint32 ConvertedLen;
        mFormatConv->Convert(aFlavor, aData, aDataLen, data.GetFlavor().get(), getter_AddRefs(ConvertedData), &ConvertedLen);
        data.SetData(ConvertedData, ConvertedLen);
        return NS_OK;
      }
    }
  }

  // Can't set data neither directly nor through converter. Just add this flavor and try again
  nsresult result = NS_ERROR_FAILURE;
  if ( NS_SUCCEEDED(AddDataFlavor(aFlavor)) )
    result = SetTransferData (aFlavor, aData, aDataLen);
    
  return result;
}


//
// AddDataFlavor
//
// Adds a data flavor to our list with no data. Error if it already exists.
// 
NS_IMETHODIMP
nsTransferable::AddDataFlavor(const char *aDataFlavor)
{
  if (GetDataForFlavor (mDataArray, aDataFlavor) != mDataArray.NoIndex)
    return NS_ERROR_FAILURE;

  // Create a new "slot" for the data
  mDataArray.AppendElement(DataStruct ( aDataFlavor ));

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
  PRUint32 idx;
  if ((idx = GetDataForFlavor(mDataArray, aDataFlavor)) != mDataArray.NoIndex) {
    mDataArray.RemoveElementAt (idx);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


/**
  * 
  *
  */
NS_IMETHODIMP
nsTransferable::IsLargeDataSet(bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::SetConverter(nsIFormatConverter * aConverter)
{
  mFormatConv = aConverter;
  return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsTransferable::GetConverter(nsIFormatConverter * *aConverter)
{
  NS_ENSURE_ARG_POINTER(aConverter);
  *aConverter = mFormatConv;
  NS_IF_ADDREF(*aConverter);
  return NS_OK;
}


//
// FlavorsTransferableCanImport
//
// Computes a list of flavors that the transferable can accept into it, either through
// intrinsic knowledge or input data converters.
//
NS_IMETHODIMP
nsTransferable::FlavorsTransferableCanImport(nsISupportsArray **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go, looking for the desired flavor.
  GetTransferDataFlavors(_retval);                        // addrefs
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

        nsCOMPtr<nsISupportsCString> flavorWrapper ( do_QueryInterface (genericFlavor) );
        nsCAutoString flavorStr;
        flavorWrapper->GetData( flavorStr );

        if (GetDataForFlavor (mDataArray, flavorStr.get())
            == mDataArray.NoIndex) // Don't append if already in intrinsic list
          (*_retval)->AppendElement (genericFlavor);
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
nsTransferable::FlavorsTransferableCanExport(nsISupportsArray **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go, looking for the desired flavor.
  GetTransferDataFlavors(_retval);  // addrefs
  nsCOMPtr<nsIFormatConverter> converter;
  GetConverter(getter_AddRefs(converter));
  if ( converter ) {
    nsCOMPtr<nsISupportsArray> convertedList;
    converter->GetOutputDataFlavors(getter_AddRefs(convertedList));

    if ( convertedList ) {
      PRUint32 importListLen;
      convertedList->Count(&importListLen);

      for ( PRUint32 i=0; i < importListLen; ++i ) {
        nsCOMPtr<nsISupports> genericFlavor;
        convertedList->GetElementAt ( i, getter_AddRefs(genericFlavor) );

        nsCOMPtr<nsISupportsCString> flavorWrapper ( do_QueryInterface (genericFlavor) );
        nsCAutoString flavorStr;
        flavorWrapper->GetData( flavorStr );

        if (GetDataForFlavor (mDataArray, flavorStr.get())
            == mDataArray.NoIndex) // Don't append if already in intrinsic list
          (*_retval)->AppendElement (genericFlavor);
      } // foreach flavor that can be converted to
    }
  } // if a converter exists

  return NS_OK;
} // FlavorsTransferableCanExport
