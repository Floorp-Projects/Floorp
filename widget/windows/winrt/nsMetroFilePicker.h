/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMetroFilePicker_h__
#define nsMetroFilePicker_h__

#include "../nsFilePicker.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"

#include "mozwrlbase.h"

#include <windows.system.h>
#include <windows.ui.core.h>
#include <windows.foundation.h>
#include <windows.storage.pickers.h>
#include <windows.storage.fileproperties.h>

/**
 * Metro file picker
 */

class nsMetroFilePicker :
  public nsBaseWinFilePicker
{
  typedef Microsoft::WRL::Wrappers::HString HString;
  typedef ABI::Windows::Storage::StorageFile StorageFile;
  typedef ABI::Windows::Foundation::AsyncStatus AsyncStatus;
  
public:
  nsMetroFilePicker(); 
  virtual ~nsMetroFilePicker();

  NS_DECL_ISUPPORTS
  
  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetFilterIndex(int32_t *aFilterIndex);
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex);
  NS_IMETHOD GetFile(nsIFile * *aFile);
  NS_IMETHOD GetFileURL(nsIURI * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(int16_t *aReturnVal); 
  NS_IMETHOD Open(nsIFilePickerShownCallback *aCallback); 
  NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter);
  NS_IMETHOD Init(nsIDOMWindow *parent, const nsAString& title,
                  int16_t mode);

  // Async callbacks
  HRESULT OnPickSingleFile(ABI::Windows::Foundation::IAsyncOperation<StorageFile*>* aFile, AsyncStatus aStatus);
  HRESULT OnPickMultipleFiles(ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Foundation::Collections::IVectorView<StorageFile*>*>* aFileList, AsyncStatus aStatus);

private:
  void InitNative(nsIWidget*, const nsAString&) {};
  nsresult ParseFiltersIntoVector(Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVector<HSTRING>>& aVector,
                                  const nsAString& aFilter,
                                  bool aAllowAll);
  nsCOMArray<nsILocalFile> mFiles;
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Pickers::IFileOpenPicker> mFileOpenPicker;
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Pickers::IFileSavePicker> mFileSavePicker;
  HString mFilePath;
  HString mFirstTitle;
  nsRefPtr<nsIFilePickerShownCallback> mCallback;
};

#endif // nsMetroFilePicker_h__
