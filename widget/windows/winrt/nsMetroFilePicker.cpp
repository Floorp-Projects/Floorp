/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMetroFilePicker.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "MetroUtils.h"

#include <windows.ui.viewmanagement.h>
#include <windows.storage.search.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Storage::Pickers;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::UI::ViewManagement;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla::widget::winrt;

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker

nsMetroFilePicker::nsMetroFilePicker()
{
}

nsMetroFilePicker::~nsMetroFilePicker()
{
}

NS_IMPL_ISUPPORTS1(nsMetroFilePicker, nsIFilePicker)

NS_IMETHODIMP
nsMetroFilePicker::Init(nsIDOMWindow *parent, const nsAString& title, int16_t mode)
{
  mMode = mode;
  HRESULT hr;
  switch(mMode) {
  case nsIFilePicker::modeOpen:
  case nsIFilePicker::modeOpenMultiple:
    hr = ActivateGenericInstance(RuntimeClass_Windows_Storage_Pickers_FileOpenPicker, mFileOpenPicker);
    AssertRetHRESULT(hr, NS_ERROR_UNEXPECTED);
    return NS_OK;
  case nsIFilePicker::modeSave:
    hr = ActivateGenericInstance(RuntimeClass_Windows_Storage_Pickers_FileSavePicker, mFileSavePicker);
    AssertRetHRESULT(hr, NS_ERROR_UNEXPECTED);
    return NS_OK;
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP
nsMetroFilePicker::Show(int16_t *aReturnVal)
{
  // Metro file picker only offers an async variant which calls back to the 
  // UI thread, which is the main thread.  We therefore can't call it
  // synchronously from the main thread.
  return NS_ERROR_NOT_IMPLEMENTED;
}

HRESULT nsMetroFilePicker::OnPickSingleFile(IAsyncOperation<StorageFile*>* aFile,
                                            AsyncStatus aStatus)
{
  if (aStatus != ABI::Windows::Foundation::AsyncStatus::Completed) {
    if (mCallback)
      mCallback->Done(nsIFilePicker::returnCancel);
    return S_OK;
  }

  HRESULT hr;
  ComPtr<IStorageFile> file;
  hr = aFile->GetResults(file.GetAddressOf());
  // When the user cancels hr == S_OK and file is NULL
  if (FAILED(hr) || !file) {
    if (mCallback)
      mCallback->Done(nsIFilePicker::returnCancel);
    return S_OK;
  }
  ComPtr<IStorageItem> storageItem;
  hr = file.As(&storageItem);
  if (FAILED(hr)) {
    if (mCallback)
      mCallback->Done(nsIFilePicker::returnCancel);
    return S_OK;
  }
  
  HSTRING path;
  if (FAILED(storageItem->get_Path(&path))) {
    if (mCallback)
      mCallback->Done(nsIFilePicker::returnCancel);
    return S_OK;
  }
  WindowsDuplicateString(path, mFilePath.GetAddressOf());
  WindowsDeleteString(path);

  if (mCallback) {
    mCallback->Done(nsIFilePicker::returnOK);
  }
  return S_OK;
}

HRESULT nsMetroFilePicker::OnPickMultipleFiles(IAsyncOperation<IVectorView<StorageFile*>*>* aFileList,
                                               AsyncStatus aStatus)
{
  if (aStatus != ABI::Windows::Foundation::AsyncStatus::Completed) {
    if (mCallback)
      mCallback->Done(nsIFilePicker::returnCancel);
    return S_OK;
  }

  HRESULT hr;
  ComPtr<IVectorView<StorageFile*>> view;
  hr = aFileList->GetResults(view.GetAddressOf());
  if (FAILED(hr)) {
    if (mCallback)
      mCallback->Done(nsIFilePicker::returnCancel);
    return S_OK;
  }

  unsigned int length;
  view->get_Size(&length);
  for (unsigned int idx = 0; idx < length; idx++) {
    ComPtr<IStorageFile> file;
    hr = view->GetAt(idx, file.GetAddressOf());
    if (FAILED(hr)) {
      continue;
    }

    ComPtr<IStorageItem> storageItem;
    hr = file.As(&storageItem);
    if (FAILED(hr)) {
      continue;
    }

    HSTRING path;
    if (SUCCEEDED(storageItem->get_Path(&path))) {
      nsCOMPtr<nsILocalFile> file = 
        do_CreateInstance("@mozilla.org/file/local;1");
      unsigned int tmp;
      if (NS_SUCCEEDED(file->InitWithPath(
          nsAutoString(WindowsGetStringRawBuffer(path, &tmp))))) {
        mFiles.AppendObject(file);
      }
    }
    WindowsDeleteString(path);
  }

  if (mCallback) {
    mCallback->Done(nsIFilePicker::returnOK);
  }
  return S_OK;
}

NS_IMETHODIMP
nsMetroFilePicker::Open(nsIFilePickerShownCallback *aCallback)
{
  HRESULT hr;
  // Capture a reference to the callback which we'll also pass into the
  // closure to ensure it's not freed.
  mCallback = aCallback;

  // The filepicker cannot open when in snapped view, try to unsnapp
  // before showing the filepicker.
  ApplicationViewState viewState;
  MetroUtils::GetViewState(viewState);
  if (viewState == ApplicationViewState::ApplicationViewState_Snapped) {
    bool unsnapped = SUCCEEDED(MetroUtils::TryUnsnap());
    NS_ENSURE_TRUE(unsnapped, NS_ERROR_FAILURE);
  }

  switch(mMode) {
  case nsIFilePicker::modeOpen: {
    NS_ENSURE_ARG_POINTER(mFileOpenPicker);

    // Initiate the file picker operation
    ComPtr<IAsyncOperation<StorageFile*>> asyncOperation;
    hr = mFileOpenPicker->PickSingleFileAsync(asyncOperation.GetAddressOf());
    AssertRetHRESULT(hr, NS_ERROR_FAILURE);

    // Subscribe to the completed event
    ComPtr<IAsyncOperationCompletedHandler<StorageFile*>>
      completedHandler(Callback<IAsyncOperationCompletedHandler<StorageFile*>>(
        this, &nsMetroFilePicker::OnPickSingleFile));
    hr = asyncOperation->put_Completed(completedHandler.Get());
    AssertRetHRESULT(hr, NS_ERROR_UNEXPECTED);
    break;
  }

  case nsIFilePicker::modeOpenMultiple: {
    NS_ENSURE_ARG_POINTER(mFileOpenPicker);

    typedef IVectorView<StorageFile*> StorageTemplate;
    typedef IAsyncOperation<StorageTemplate*> AsyncCallbackTemplate;
    typedef IAsyncOperationCompletedHandler<StorageTemplate*> HandlerTemplate;

    // Initiate the file picker operation
    ComPtr<AsyncCallbackTemplate> asyncOperation;
    hr = mFileOpenPicker->PickMultipleFilesAsync(asyncOperation.GetAddressOf());
    AssertRetHRESULT(hr, NS_ERROR_FAILURE);

    // Subscribe to the completed event
    ComPtr<HandlerTemplate> completedHandler(Callback<HandlerTemplate>(
      this, &nsMetroFilePicker::OnPickMultipleFiles));
    hr = asyncOperation->put_Completed(completedHandler.Get());
    AssertRetHRESULT(hr, NS_ERROR_UNEXPECTED);
    break;
  }

  case nsIFilePicker::modeSave: {
    NS_ENSURE_ARG_POINTER(mFileSavePicker);

    // Set the default file name
    mFileSavePicker->put_SuggestedFileName(HStringReference(mDefaultFilename.BeginReading()).Get());

    // Set the default file extension
    if (mDefaultExtension.Length() > 0) {
      nsAutoString defaultFileExtension(mDefaultExtension);

      // Touch up the extansion format platform hands us.
      if (defaultFileExtension[0] == L'*') {
        defaultFileExtension.Cut(0, 1);
      } else if (defaultFileExtension[0] != L'.') {
        defaultFileExtension.Insert(L".", 0);
      }

      // Sometimes the default file extension is not passed in correctly,
      // so we purposfully ignore failures here.
      HString ext;
      ext.Set(defaultFileExtension.BeginReading());
      hr = mFileSavePicker->put_DefaultFileExtension(ext.Get());
      NS_ASSERTION(SUCCEEDED(hr), "put_DefaultFileExtension failed, bad format for extension?");

      // Due to a bug in the WinRT file picker, the first file extension in the
      // list is always used when saving a file. So we explicitly make sure the
      // default extension is the first one in the list here.
      if (mFirstTitle.Get()) {
        ComPtr<IMap<HSTRING, IVector<HSTRING>*>> map;
        mFileSavePicker->get_FileTypeChoices(map.GetAddressOf());
        if (map) {
          boolean found = false;
          unsigned int index;
          map->HasKey(mFirstTitle.Get(), &found);
          if (found) {
            ComPtr<IVector<HSTRING>> list;
            if (SUCCEEDED(map->Lookup(mFirstTitle.Get(), list.GetAddressOf()))) {
              HString ext;
              ext.Set(defaultFileExtension.get());
              found = false;
              list->IndexOf(HStringReference(defaultFileExtension.get()).Get(), &index, &found);
              if (found) {
                list->RemoveAt(index);
                list->InsertAt(0, HStringReference(defaultFileExtension.get()).Get());
              }
            }
          }
        }
      }
    }

    // Dispatch the async show operation
    ComPtr<IAsyncOperation<StorageFile*>> asyncOperation;
    hr = mFileSavePicker->PickSaveFileAsync(asyncOperation.GetAddressOf());
    AssertRetHRESULT(hr, NS_ERROR_FAILURE);

    // Subscribe to the completed event
    ComPtr<IAsyncOperationCompletedHandler<StorageFile*>>
      completedHandler(Callback<IAsyncOperationCompletedHandler<StorageFile*>>(
        this, &nsMetroFilePicker::OnPickSingleFile));
    hr = asyncOperation->put_Completed(completedHandler.Get());
    AssertRetHRESULT(hr, NS_ERROR_UNEXPECTED);
    break;
  }

  case modeGetFolder:
    return NS_ERROR_NOT_IMPLEMENTED;

  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMetroFilePicker::GetFile(nsIFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nullptr;

  if (WindowsIsStringEmpty(mFilePath.Get()))
    return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  unsigned int length;
  file->InitWithPath(nsAutoString(mFilePath.GetRawBuffer(&length)));
  NS_ADDREF(*aFile = file);
  return NS_OK;
}

NS_IMETHODIMP
nsMetroFilePicker::GetFileURL(nsIURI **aFileURL)
{
  *aFileURL = nullptr;
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (!file)
    return rv;

  return NS_NewFileURI(aFileURL, file);
}

NS_IMETHODIMP
nsMetroFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);
  return NS_NewArrayEnumerator(aFiles, mFiles);
}

// Set the filter index
NS_IMETHODIMP
nsMetroFilePicker::GetFilterIndex(int32_t *aFilterIndex)
{
  // No associated concept with a Metro file picker
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMetroFilePicker::SetFilterIndex(int32_t aFilterIndex)
{
  // No associated concept with a Metro file picker
  return NS_ERROR_NOT_IMPLEMENTED;
}

// AFACT, it's up to use to supply the implementation of a vector list.
class MozHStringVector : public RuntimeClass<IVector<HSTRING>> {
  InspectableClass(L"MozHStringVector", TrustLevel::BaseTrust)
  ~MozHStringVector() {
    Clear();
  }

  // See IVector_impl in windows.foundation.collections.h
public:
  STDMETHOD(GetAt)(unsigned aIndex, HSTRING* aString) {
    if (aIndex >= mList.Length()) {
      return E_INVALIDARG;
    }
    return WindowsDuplicateString(mList[aIndex], aString);
  }

  STDMETHOD(get_Size)(unsigned int* aLength) {
    if (!aLength) {
      return E_INVALIDARG;
    }
    *aLength = mList.Length();
    return S_OK;
  }

  STDMETHOD(Append)(HSTRING aString) {
    HSTRING str;
    if (FAILED(WindowsDuplicateString(aString, &str))) {
      return E_INVALIDARG;
    }
    mList.AppendElement(str);
    return S_OK;
  }

  STDMETHOD(Clear)() {
    int length = mList.Length();
    for (int idx = 0; idx < length; idx++)
      WindowsDeleteString(mList[idx]);
    mList.Clear();
    return S_OK;
  }

  // interfaces picker code doesn't seem to need
  STDMETHOD(GetView)(IVectorView<HSTRING> **aView) { return E_NOTIMPL; }
  STDMETHOD(IndexOf)(HSTRING aValue, unsigned *aIndex, boolean *found) { return E_NOTIMPL; }
  STDMETHOD(SetAt)(unsigned aIndex, HSTRING aString) { return E_NOTIMPL; }
  STDMETHOD(InsertAt)(unsigned aIndex, HSTRING aString) { return E_NOTIMPL; }
  STDMETHOD(RemoveAt)(unsigned aIndex) { return E_NOTIMPL; }
  STDMETHOD(RemoveAtEnd)() { return E_NOTIMPL; }

private:
  nsTArray<HSTRING> mList;
};

nsresult
nsMetroFilePicker::ParseFiltersIntoVector(ComPtr<IVector<HSTRING>>& aVector,
                                          const nsAString& aFilter,
                                          bool aAllowAll)
{
  const PRUnichar *beg = aFilter.BeginReading();
  const PRUnichar *end = aFilter.EndReading();
  for (const PRUnichar *cur = beg, *fileTypeStart = beg; cur <= end; ++cur) {
    // Start of a a filetype, example: *.png
    if (cur == end || PRUnichar(' ') == *cur) {
      int32_t startPos = fileTypeStart - beg;
      int32_t endPos = cur - fileTypeStart - (cur == end ? 0 : 1);
      const nsAString& fileType = Substring(aFilter,
                                            startPos,
                                            endPos);
      // There is no way to say show all files in Metro save file picker, so
      // just use .data if * or *.* is specified.
      if (fileType.IsEmpty() ||
          fileType.Equals(L"*") ||
          fileType.Equals(L"*.*")) {
        HString str;
        if (aAllowAll) {
          str.Set(L"*");
          aVector->Append(str.Get());
        } else {
          str.Set(L".data");
          aVector->Append(str.Get());
        }
      } else {
        nsAutoString filter(fileType);
        if (filter[0] == L'*') {
          filter.Cut(0, 1);
        } else if (filter[0] != L'.') {
          filter.Insert(L".", 0);
        }
        HString str;
        str.Set(filter.BeginReading());
        aVector->Append(str.Get());
      }

      fileTypeStart = cur + 1;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMetroFilePicker::AppendFilter(const nsAString& aTitle, 
                                const nsAString& aFilter)
{
  HRESULT hr;
  switch(mMode) {
  case nsIFilePicker::modeOpen:
  case nsIFilePicker::modeOpenMultiple: {
    NS_ENSURE_ARG_POINTER(mFileOpenPicker);
    ComPtr<IVector<HSTRING>> list;
    mFileOpenPicker->get_FileTypeFilter(list.GetAddressOf());
    nsresult rv = ParseFiltersIntoVector(list, aFilter, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  case nsIFilePicker::modeSave: {
    NS_ENSURE_ARG_POINTER(mFileSavePicker);

    ComPtr<IMap<HSTRING,IVector<HSTRING>*>> map;
    hr = mFileSavePicker->get_FileTypeChoices(map.GetAddressOf());
    AssertRetHRESULT(hr, NS_ERROR_FAILURE);

    HString key;
    key.Set(aTitle.BeginReading());

    ComPtr<IVector<HSTRING>> saveTypes;
    saveTypes = Make<MozHStringVector>();
    nsresult rv = ParseFiltersIntoVector(saveTypes, aFilter, false);
    NS_ENSURE_SUCCESS(rv, rv);

    if (WindowsIsStringEmpty(mFirstTitle.Get())) {
      mFirstTitle.Set(key.Get());
    }

    boolean replaced;
    map->Insert(key.Get(), saveTypes.Get(), &replaced);
  }
  break;

  default:
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

