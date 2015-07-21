/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#include "nsIStringBundle.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsCOMArray.h"
#include "nsIFile.h"
#include "nsEnumeratorUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/Services.h"
#include "WidgetUtils.h"
#include "nsThreadUtils.h"

#include "nsBaseFilePicker.h"

using namespace mozilla::widget;
using namespace mozilla::dom;

#define FILEPICKER_TITLES "chrome://global/locale/filepicker.properties"
#define FILEPICKER_FILTERS "chrome://global/content/filepicker.properties"

/**
 * A runnable to dispatch from the main thread to the main thread to display
 * the file picker while letting the showAsync method return right away.
*/
class AsyncShowFilePicker : public nsRunnable
{
public:
  AsyncShowFilePicker(nsIFilePicker *aFilePicker,
                      nsIFilePickerShownCallback *aCallback) :
    mFilePicker(aFilePicker),
    mCallback(aCallback)
  {
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(),
                 "AsyncShowFilePicker should be on the main thread!");

    // It's possible that some widget implementations require GUI operations
    // to be on the main thread, so that's why we're not dispatching to another
    // thread and calling back to the main after it's done.
    int16_t result = nsIFilePicker::returnCancel;
    nsresult rv = mFilePicker->Show(&result);
    if (NS_FAILED(rv)) {
      NS_ERROR("FilePicker's Show() implementation failed!");
    }

    if (mCallback) {
      mCallback->Done(result);
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsIFilePicker> mFilePicker;
  nsRefPtr<nsIFilePickerShownCallback> mCallback;
};

class nsBaseFilePickerEnumerator : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS

  explicit nsBaseFilePickerEnumerator(nsPIDOMWindow* aParent,
                                      nsISimpleEnumerator* iterator,
                                      int16_t aMode)
    : mIterator(iterator)
    , mParent(aParent)
    , mMode(aMode)
  {}

  NS_IMETHOD
  GetNext(nsISupports** aResult) override
  {
    nsCOMPtr<nsISupports> tmp;
    nsresult rv = mIterator->GetNext(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!tmp) {
      return NS_OK;
    }

    nsCOMPtr<nsIFile> localFile = do_QueryInterface(tmp);
    if (!localFile) {
      return NS_ERROR_FAILURE;
    }

    nsRefPtr<File> domFile = File::CreateFromFile(mParent, localFile);

    // Right now we're on the main thread of the chrome process. We need
    // to call SetIsDirectory on the BlobImpl, but it's preferable not to
    // call nsIFile::IsDirectory to determine what argument to pass since
    // IsDirectory does synchronous I/O. It's true that since we've just
    // been called synchronously directly after nsIFilePicker::Show blocked
    // the main thread while the picker was being shown and the OS did file
    // system access, doing more I/O to stat the selected files probably
    // wouldn't be the end of the world. However, we can simply check
    // mMode and avoid calling IsDirectory.
    //
    // In future we may take advantage of OS X's ability to allow both
    // files and directories to be picked at the same time, so we do assert
    // in debug builds that the mMode trick produces the correct results.
    // If we do add that support maybe it's better to use IsDirectory
    // directly, but in an nsRunnable punted off to a background thread.
#ifdef DEBUG
    bool isDir;
    localFile->IsDirectory(&isDir);
    MOZ_ASSERT(isDir == (mMode == nsIFilePicker::modeGetFolder));
#endif
    domFile->Impl()->SetIsDirectory(mMode == nsIFilePicker::modeGetFolder);

    nsCOMPtr<nsIDOMBlob>(domFile).forget(aResult);
    return NS_OK;
  }

  NS_IMETHOD
  HasMoreElements(bool* aResult) override
  {
    return mIterator->HasMoreElements(aResult);
  }

protected:
  virtual ~nsBaseFilePickerEnumerator()
  {}

private:
  nsCOMPtr<nsISimpleEnumerator> mIterator;
  nsCOMPtr<nsPIDOMWindow> mParent;
  int16_t mMode;
};

NS_IMPL_ISUPPORTS(nsBaseFilePickerEnumerator, nsISimpleEnumerator)

nsBaseFilePicker::nsBaseFilePicker()
  : mAddToRecentDocs(true)
  , mMode(nsIFilePicker::modeOpen)
{

}

nsBaseFilePicker::~nsBaseFilePicker()
{

}

NS_IMETHODIMP nsBaseFilePicker::Init(nsIDOMWindow *aParent,
                                     const nsAString& aTitle,
                                     int16_t aMode)
{
  NS_PRECONDITION(aParent, "Null parent passed to filepicker, no file "
                  "picker for you!");
  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(aParent);
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  mParent = do_QueryInterface(aParent);
  if (!mParent->IsInnerWindow()) {
    mParent = mParent->GetCurrentInnerWindow();
  }

  mMode = aMode;
  InitNative(widget, aTitle);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::Open(nsIFilePickerShownCallback *aCallback)
{
  nsCOMPtr<nsIRunnable> filePickerEvent =
    new AsyncShowFilePicker(this, aCallback);
  return NS_DispatchToMainThread(filePickerEvent);
}

NS_IMETHODIMP
nsBaseFilePicker::AppendFilters(int32_t aFilterMask)
{
  nsCOMPtr<nsIStringBundleService> stringService =
    mozilla::services::GetStringBundleService();
  if (!stringService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> titleBundle, filterBundle;

  nsresult rv = stringService->CreateBundle(FILEPICKER_TITLES,
                                            getter_AddRefs(titleBundle));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  rv = stringService->CreateBundle(FILEPICKER_FILTERS, getter_AddRefs(filterBundle));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsXPIDLString title;
  nsXPIDLString filter;

  if (aFilterMask & filterAll) {
    titleBundle->GetStringFromName(MOZ_UTF16("allTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("allFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterHTML) {
    titleBundle->GetStringFromName(MOZ_UTF16("htmlTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("htmlFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterText) {
    titleBundle->GetStringFromName(MOZ_UTF16("textTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("textFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterImages) {
    titleBundle->GetStringFromName(MOZ_UTF16("imageTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("imageFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterAudio) {
    titleBundle->GetStringFromName(MOZ_UTF16("audioTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("audioFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterVideo) {
    titleBundle->GetStringFromName(MOZ_UTF16("videoTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("videoFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXML) {
    titleBundle->GetStringFromName(MOZ_UTF16("xmlTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("xmlFilter"), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXUL) {
    titleBundle->GetStringFromName(MOZ_UTF16("xulTitle"), getter_Copies(title));
    filterBundle->GetStringFromName(MOZ_UTF16("xulFilter"), getter_Copies(filter));
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterApps) {
    titleBundle->GetStringFromName(MOZ_UTF16("appsTitle"), getter_Copies(title));
    // Pass the magic string "..apps" to the platform filepicker, which it
    // should recognize and do the correct platform behavior for.
    AppendFilter(title, NS_LITERAL_STRING("..apps"));
  }
  return NS_OK;
}

// Set the filter index
NS_IMETHODIMP nsBaseFilePicker::GetFilterIndex(int32_t *aFilterIndex)
{
  *aFilterIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::SetFilterIndex(int32_t aFilterIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);
  nsCOMArray <nsIFile> files;
  nsresult rv;

  // if we get into the base class, the platform
  // doesn't implement GetFiles() yet.
  // so we fake it.
  nsCOMPtr <nsIFile> file;
  rv = GetFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);

  files.AppendObject(file);

  return NS_NewArrayEnumerator(aFiles, files);
}

// Set the display directory
NS_IMETHODIMP nsBaseFilePicker::SetDisplayDirectory(nsIFile *aDirectory)
{
  if (!aDirectory) {
    mDisplayDirectory = nullptr;
    return NS_OK;
  }
  nsCOMPtr<nsIFile> directory;
  nsresult rv = aDirectory->Clone(getter_AddRefs(directory));
  if (NS_FAILED(rv))
    return rv;
  mDisplayDirectory = do_QueryInterface(directory, &rv);
  return rv;
}

// Get the display directory
NS_IMETHODIMP nsBaseFilePicker::GetDisplayDirectory(nsIFile **aDirectory)
{
  *aDirectory = nullptr;
  if (!mDisplayDirectory)
    return NS_OK;
  nsCOMPtr<nsIFile> directory;
  nsresult rv = mDisplayDirectory->Clone(getter_AddRefs(directory));
  if (NS_FAILED(rv)) {
    return rv;
  }
  directory.forget(aDirectory);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetAddToRecentDocs(bool *aFlag)
{
  *aFlag = mAddToRecentDocs;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::SetAddToRecentDocs(bool aFlag)
{
  mAddToRecentDocs = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetMode(int16_t* aMode)
{
  *aMode = mMode;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomfile(nsISupports** aDomfile)
{
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = GetFile(getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!localFile) {
    *aDomfile = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMBlob> domFile = File::CreateFromFile(mParent, localFile);
  domFile.forget(aDomfile);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomfiles(nsISimpleEnumerator** aDomfiles)
{
  nsCOMPtr<nsISimpleEnumerator> iter;
  nsresult rv = GetFiles(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsBaseFilePickerEnumerator> retIter =
    new nsBaseFilePickerEnumerator(mParent, iter, mMode);

  retIter.forget(aDomfiles);
  return NS_OK;
}

