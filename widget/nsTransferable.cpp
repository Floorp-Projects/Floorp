/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Notes to self:

- at some point, strings will be accessible from JS, so we won't have to wrap
   flavors in an nsISupportsCString. Until then, we're kinda stuck with
   this crappy API of nsIArrays.

*/

#include "nsTransferable.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsIFormatConverter.h"
#include "nsIContentPolicy.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsMemory.h"
#include "nsPrimitiveHelpers.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsILoadContext.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsTransferable, nsITransferable)

DataStruct::DataStruct(DataStruct&& aRHS)
  : mData(aRHS.mData.forget())
  , mDataLen(aRHS.mDataLen)
  , mCacheFD(aRHS.mCacheFD)
  , mFlavor(aRHS.mFlavor)
{
  aRHS.mCacheFD = nullptr;
}

//-------------------------------------------------------------------------
DataStruct::~DataStruct()
{
  if (mCacheFD) {
    PR_Close(mCacheFD);
  }
}

//-------------------------------------------------------------------------
void
DataStruct::SetData(nsISupports* aData, uint32_t aDataLen, bool aIsPrivateData)
{
  // Now, check to see if we consider the data to be "too large"
  // as well as ensuring that private browsing mode is disabled
  if (aDataLen > kLargeDatasetSize && !aIsPrivateData) {
    // if so, cache it to disk instead of memory
    if (NS_SUCCEEDED(WriteCache(aData, aDataLen))) {
      // Clear previously set small data.
      mData = nullptr;
      mDataLen = 0;
      return;
    }
    NS_WARNING("Oh no, couldn't write data to the cache file");
  }

  if (mCacheFD) {
    // Clear previously set big data.
    PR_Close(mCacheFD);
    mCacheFD = nullptr;
  }

  mData = aData;
  mDataLen = aDataLen;
}

//-------------------------------------------------------------------------
void
DataStruct::GetData(nsISupports** aData, uint32_t* aDataLen)
{
  // check here to see if the data is cached on disk
  if (mCacheFD) {
    // if so, read it in and pass it back
    // ReadCache creates memory and copies the data into it.
    if (NS_SUCCEEDED(ReadCache(aData, aDataLen))) {
      return;
    }

    // oh shit, something went horribly wrong here.
    NS_WARNING("Oh no, couldn't read data in from the cache file");
    *aData = nullptr;
    *aDataLen = 0;
    PR_Close(mCacheFD);
    mCacheFD = nullptr;
    return;
  }

  nsCOMPtr<nsISupports> data = mData;
  data.forget(aData);
  *aDataLen = mDataLen;
}

//-------------------------------------------------------------------------
nsresult
DataStruct::WriteCache(nsISupports* aData, uint32_t aDataLen)
{
  nsresult rv;
  if (!mCacheFD) {
    rv = NS_OpenAnonymousTemporaryFile(&mCacheFD);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  } else if (PR_Seek64(mCacheFD, 0, PR_SEEK_SET) == -1) {
    return NS_ERROR_FAILURE;
  }

  // write out the contents of the clipboard to the file
  void* buff = nullptr;
  nsPrimitiveHelpers::CreateDataFromPrimitive(mFlavor, aData, &buff, aDataLen);
  if (buff) {
    int32_t written = PR_Write(mCacheFD, buff, aDataLen);
    free(buff);
    if (written) {
      return NS_OK;
    }
  }
  PR_Close(mCacheFD);
  mCacheFD = nullptr;
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
nsresult
DataStruct::ReadCache(nsISupports** aData, uint32_t* aDataLen)
{
  if (!mCacheFD) {
    return NS_ERROR_FAILURE;
  }

  PRFileInfo fileInfo;
  if (PR_GetOpenFileInfo(mCacheFD, &fileInfo) != PR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }
  if (PR_Seek64(mCacheFD, 0, PR_SEEK_SET) == -1) {
    return NS_ERROR_FAILURE;
  }
  uint32_t fileSize = fileInfo.size;

  auto data = MakeUnique<char[]>(fileSize);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t actual = PR_Read(mCacheFD, data.get(), fileSize);
  if (actual != fileSize) {
    return NS_ERROR_FAILURE;
  }

  nsPrimitiveHelpers::CreatePrimitiveForData(
    mFlavor, data.get(), fileSize, aData);
  *aDataLen = fileSize;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Transferable constructor
//
//-------------------------------------------------------------------------
nsTransferable::nsTransferable()
  : mPrivateData(false)
  , mContentPolicyType(nsIContentPolicy::TYPE_OTHER)
#ifdef DEBUG
  , mInitialized(false)
#endif
{
}

//-------------------------------------------------------------------------
//
// Transferable destructor
//
//-------------------------------------------------------------------------
nsTransferable::~nsTransferable() {}

NS_IMETHODIMP
nsTransferable::Init(nsILoadContext* aContext)
{
  MOZ_ASSERT(!mInitialized);

  if (aContext) {
    mPrivateData = aContext->UsePrivateBrowsing();
  }
#ifdef DEBUG
  mInitialized = true;
#endif
  return NS_OK;
}

//
// GetTransferDataFlavors
//
// Returns a copy of the internal list of flavors. This does NOT take into
// account any converter that may be registered.
//
void
nsTransferable::GetTransferDataFlavors(nsTArray<nsCString>& aFlavors)
{
  MOZ_ASSERT(mInitialized);

  for (size_t i = 0; i < mDataArray.Length(); ++i) {
    DataStruct& data = mDataArray.ElementAt(i);
    aFlavors.AppendElement(data.GetFlavor());
  }
}

Maybe<size_t>
nsTransferable::FindDataFlavor(const char* aFlavor)
{
  nsDependentCString flavor(aFlavor);

  for (size_t i = 0; i < mDataArray.Length(); ++i) {
    if (mDataArray[i].GetFlavor().Equals(flavor)) {
      return Some(i);
    }
  }

  return Nothing();
}


//
// GetTransferData
//
// Returns the data of the requested flavor, obtained from either having the
// data on hand or using a converter to get it. The data is wrapped in a
// nsISupports primitive so that it is accessible from JS.
//
NS_IMETHODIMP
nsTransferable::GetTransferData(const char* aFlavor,
                                nsISupports** aData,
                                uint32_t* aDataLen)
{
  MOZ_ASSERT(mInitialized);

  *aData = nullptr;
  *aDataLen = 0;

  nsresult rv = NS_OK;

  // First look and see if the data is present in one of the intrinsic flavors.
  if (Maybe<size_t> index = FindDataFlavor(aFlavor)) {
    nsCOMPtr<nsISupports> dataBytes;
    uint32_t len;
    mDataArray[index.value()].GetData(getter_AddRefs(dataBytes), &len);

    // Do we have a (lazy) data provider?
    if (nsCOMPtr<nsIFlavorDataProvider> dataProvider =
          do_QueryInterface(dataBytes)) {
      rv = dataProvider->GetFlavorData(this, aFlavor,
                                       getter_AddRefs(dataBytes), &len);
      if (NS_FAILED(rv)) {
        dataBytes = nullptr;
        // The provider failed, fall into the converter code below.
      }
    }

    if (dataBytes) {
      *aDataLen = len;
      dataBytes.forget(aData);
      return NS_OK;
    }

    // Empty data
  }

  // If not, try using a format converter to get the requested flavor.
  if (mFormatConv) {
    for (size_t i = 0; i < mDataArray.Length(); ++i) {
      DataStruct& data = mDataArray.ElementAt(i);
      bool canConvert = false;
      mFormatConv->CanConvert(data.GetFlavor().get(), aFlavor, &canConvert);
      if (canConvert) {
        nsCOMPtr<nsISupports> dataBytes;
        uint32_t len;
        data.GetData(getter_AddRefs(dataBytes), &len);

        // Do we have a (lazy) data provider?
        if (nsCOMPtr<nsIFlavorDataProvider> dataProvider =
              do_QueryInterface(dataBytes)) {
          rv = dataProvider->GetFlavorData(this, aFlavor,
                                           getter_AddRefs(dataBytes), &len);
          if (NS_FAILED(rv)) {
            // Give up.
            return rv;
          }
        }

        return mFormatConv->Convert(data.GetFlavor().get(), dataBytes, len,
                                    aFlavor, aData, aDataLen);
      }
    }
  }

  return NS_ERROR_FAILURE;
}

//
// GetAnyTransferData
//
// Returns the data of the first flavor found. Caller is responsible for
// deleting the flavor string.
//
NS_IMETHODIMP
nsTransferable::GetAnyTransferData(nsACString& aFlavor,
                                   nsISupports** aData,
                                   uint32_t* aDataLen)
{
  MOZ_ASSERT(mInitialized);

  for (size_t i = 0; i < mDataArray.Length(); ++i) {
    DataStruct& data = mDataArray.ElementAt(i);
    if (data.IsDataAvailable()) {
      aFlavor.Assign(data.GetFlavor());
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
nsTransferable::SetTransferData(const char* aFlavor,
                                nsISupports* aData,
                                uint32_t aDataLen)
{
  MOZ_ASSERT(mInitialized);

  // first check our intrinsic flavors to see if one has been registered.
  if (Maybe<size_t> index = FindDataFlavor(aFlavor)) {
    DataStruct& data = mDataArray.ElementAt(index.value());
    data.SetData(aData, aDataLen, mPrivateData);
    return NS_OK;
  }

  // if not, try using a format converter to find a flavor to put the data in
  if (mFormatConv) {
    for (size_t i = 0; i < mDataArray.Length(); ++i) {
      DataStruct& data = mDataArray.ElementAt(i);
      bool canConvert = false;
      mFormatConv->CanConvert(aFlavor, data.GetFlavor().get(), &canConvert);

      if (canConvert) {
        nsCOMPtr<nsISupports> ConvertedData;
        uint32_t ConvertedLen;
        mFormatConv->Convert(aFlavor,
                             aData,
                             aDataLen,
                             data.GetFlavor().get(),
                             getter_AddRefs(ConvertedData),
                             &ConvertedLen);
        data.SetData(ConvertedData, ConvertedLen, mPrivateData);
        return NS_OK;
      }
    }
  }

  // Can't set data neither directly nor through converter. Just add this flavor
  // and try again
  if (NS_SUCCEEDED(AddDataFlavor(aFlavor))) {
    return SetTransferData(aFlavor, aData, aDataLen);
  }

  return NS_ERROR_FAILURE;
}

//
// AddDataFlavor
//
// Adds a data flavor to our list with no data. Error if it already exists.
//
NS_IMETHODIMP
nsTransferable::AddDataFlavor(const char* aDataFlavor)
{
  MOZ_ASSERT(mInitialized);

  if (FindDataFlavor(aDataFlavor).isSome()) {
    return NS_ERROR_FAILURE;
  }

  // Create a new "slot" for the data
  mDataArray.AppendElement(DataStruct(aDataFlavor));
  return NS_OK;
}

//
// RemoveDataFlavor
//
// Removes a data flavor (and causes the data to be destroyed). Error if
// the requested flavor is not present.
//
NS_IMETHODIMP
nsTransferable::RemoveDataFlavor(const char* aDataFlavor)
{
  MOZ_ASSERT(mInitialized);

  if (Maybe<size_t> index = FindDataFlavor(aDataFlavor)) {
    mDataArray.RemoveElementAt(index.value());
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTransferable::SetConverter(nsIFormatConverter* aConverter)
{
  MOZ_ASSERT(mInitialized);

  mFormatConv = aConverter;
  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::GetConverter(nsIFormatConverter** aConverter)
{
  MOZ_ASSERT(mInitialized);

  nsCOMPtr<nsIFormatConverter> converter = mFormatConv;
  converter.forget(aConverter);
  return NS_OK;
}

//
// FlavorsTransferableCanImport
//
// Computes a list of flavors that the transferable can accept into it, either
// through intrinsic knowledge or input data converters.
//
NS_IMETHODIMP
nsTransferable::FlavorsTransferableCanImport(nsTArray<nsCString>& aFlavors)
{
  MOZ_ASSERT(mInitialized);

  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the
  // list in one go, looking for the desired flavor.
  GetTransferDataFlavors(aFlavors);

  if (mFormatConv) {
    nsTArray<nsCString> convertedList;
    mFormatConv->GetInputDataFlavors(convertedList);

    for (uint32_t i = 0; i < convertedList.Length(); ++i) {
      nsCString& flavorStr = convertedList[i];

      // Don't append if already in intrinsic list
      if (!aFlavors.Contains(flavorStr)) {
        aFlavors.AppendElement(flavorStr);
      }
    }
  }

  return NS_OK;
}

//
// FlavorsTransferableCanExport
//
// Computes a list of flavors that the transferable can export, either through
// intrinsic knowledge or output data converters.
//
NS_IMETHODIMP
nsTransferable::FlavorsTransferableCanExport(nsTArray<nsCString>& aFlavors)
{
  MOZ_ASSERT(mInitialized);

  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the
  // list in one go, looking for the desired flavor.
  GetTransferDataFlavors(aFlavors);

  if (mFormatConv) {
    nsTArray<nsCString> convertedList;
    mFormatConv->GetOutputDataFlavors(convertedList);

    for (uint32_t i = 0; i < convertedList.Length(); ++i) {
      nsCString& flavorStr = convertedList[i];

      // Don't append if already in intrinsic list
      if (!aFlavors.Contains(flavorStr)) {
        aFlavors.AppendElement(flavorStr);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::GetIsPrivateData(bool* aIsPrivateData)
{
  MOZ_ASSERT(mInitialized);

  *aIsPrivateData = mPrivateData;
  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::SetIsPrivateData(bool aIsPrivateData)
{
  MOZ_ASSERT(mInitialized);

  mPrivateData = aIsPrivateData;
  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::GetRequestingPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  MOZ_ASSERT(mInitialized);

  nsCOMPtr<nsIPrincipal> principal = mRequestingPrincipal;
  principal.forget(aRequestingPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::SetRequestingPrincipal(nsIPrincipal* aRequestingPrincipal)
{
  MOZ_ASSERT(mInitialized);

  mRequestingPrincipal = aRequestingPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::GetContentPolicyType(nsContentPolicyType* aContentPolicyType)
{
  MOZ_ASSERT(mInitialized);

  *aContentPolicyType = mContentPolicyType;
  return NS_OK;
}

NS_IMETHODIMP
nsTransferable::SetContentPolicyType(nsContentPolicyType aContentPolicyType)
{
  MOZ_ASSERT(mInitialized);

  mContentPolicyType = aContentPolicyType;
  return NS_OK;
}
