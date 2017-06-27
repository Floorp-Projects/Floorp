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
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/Services.h"
#include "WidgetUtils.h"
#include "nsThreadUtils.h"

#include "nsBaseFilePicker.h"

using namespace mozilla::widget;
using namespace mozilla::dom;

#define FILEPICKER_TITLES "chrome://global/locale/filepicker.properties"
#define FILEPICKER_FILTERS "chrome://global/content/filepicker.properties"

namespace {

nsresult
LocalFileToDirectoryOrBlob(nsPIDOMWindowInner* aWindow,
                           bool aIsDirectory,
                           nsIFile* aFile,
                           nsISupports** aResult)
{
  if (aIsDirectory) {
#ifdef DEBUG
    bool isDir;
    aFile->IsDirectory(&isDir);
    MOZ_ASSERT(isDir);
#endif

    RefPtr<Directory> directory = Directory::Create(aWindow, aFile);
    MOZ_ASSERT(directory);

    directory.forget(aResult);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMBlob> blob = File::CreateFromFile(aWindow, aFile);
  blob.forget(aResult);
  return NS_OK;
}

} // anonymous namespace

/**
 * A runnable to dispatch from the main thread to the main thread to display
 * the file picker while letting the showAsync method return right away.
*/
class AsyncShowFilePicker : public mozilla::Runnable
{
public:
  AsyncShowFilePicker(nsIFilePicker* aFilePicker,
                      nsIFilePickerShownCallback* aCallback)
    : mozilla::Runnable("AsyncShowFilePicker")
    , mFilePicker(aFilePicker)
    , mCallback(aCallback)
  {
  }

  NS_IMETHOD Run() override
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
  RefPtr<nsIFilePicker> mFilePicker;
  RefPtr<nsIFilePickerShownCallback> mCallback;
};

class nsBaseFilePickerEnumerator : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS

  nsBaseFilePickerEnumerator(nsPIDOMWindowOuter* aParent,
                             nsISimpleEnumerator* iterator,
                             int16_t aMode)
    : mIterator(iterator)
    , mParent(aParent->GetCurrentInnerWindow())
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

    return LocalFileToDirectoryOrBlob(mParent,
                                      mMode == nsIFilePicker::modeGetFolder,
                                      localFile,
                                      aResult);
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
  nsCOMPtr<nsPIDOMWindowInner> mParent;
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

NS_IMETHODIMP nsBaseFilePicker::Init(mozIDOMWindowProxy* aParent,
                                     const nsAString& aTitle,
                                     int16_t aMode)
{
  NS_PRECONDITION(aParent, "Null parent passed to filepicker, no file "
                  "picker for you!");

  mParent = nsPIDOMWindowOuter::From(aParent);

  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(mParent->GetOuterWindow());
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);


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
    titleBundle->GetStringFromName(u"allTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"allFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterHTML) {
    titleBundle->GetStringFromName(u"htmlTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"htmlFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterText) {
    titleBundle->GetStringFromName(u"textTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"textFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterImages) {
    titleBundle->GetStringFromName(u"imageTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"imageFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterAudio) {
    titleBundle->GetStringFromName(u"audioTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"audioFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterVideo) {
    titleBundle->GetStringFromName(u"videoTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"videoFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXML) {
    titleBundle->GetStringFromName(u"xmlTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"xmlFilter", getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXUL) {
    titleBundle->GetStringFromName(u"xulTitle", getter_Copies(title));
    filterBundle->GetStringFromName(u"xulFilter", getter_Copies(filter));
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterApps) {
    titleBundle->GetStringFromName(u"appsTitle", getter_Copies(title));
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
  // if displaySpecialDirectory has been previously called, let's abort this
  // operation.
  if (!mDisplaySpecialDirectory.IsEmpty()) {
    return NS_OK;
  }

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

  // if displaySpecialDirectory has been previously called, let's abort this
  // operation.
  if (!mDisplaySpecialDirectory.IsEmpty()) {
    return NS_OK;
  }

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

// Set the display special directory
NS_IMETHODIMP nsBaseFilePicker::SetDisplaySpecialDirectory(const nsAString& aDirectory)
{
  // if displayDirectory has been previously called, let's abort this operation.
  if (mDisplayDirectory && mDisplaySpecialDirectory.IsEmpty()) {
    return NS_OK;
  }

  mDisplaySpecialDirectory = aDirectory;
  if (mDisplaySpecialDirectory.IsEmpty()) {
    mDisplayDirectory = nullptr;
    return NS_OK;
  }

  return NS_GetSpecialDirectory(NS_ConvertUTF16toUTF8(mDisplaySpecialDirectory).get(),
                                getter_AddRefs(mDisplayDirectory));
}

// Get the display special directory
NS_IMETHODIMP nsBaseFilePicker::GetDisplaySpecialDirectory(nsAString& aDirectory)
{
  aDirectory = mDisplaySpecialDirectory;
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
nsBaseFilePicker::SetOkButtonLabel(const nsAString& aLabel)
{
  mOkButtonLabel = aLabel;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetOkButtonLabel(nsAString& aLabel)
{
  aLabel = mOkButtonLabel;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomFileOrDirectory(nsISupports** aValue)
{
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = GetFile(getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!localFile) {
    *aValue = nullptr;
    return NS_OK;
  }

  auto* innerParent = mParent ? mParent->GetCurrentInnerWindow() : nullptr;

  return LocalFileToDirectoryOrBlob(innerParent,
                                    mMode == nsIFilePicker::modeGetFolder,
                                    localFile,
                                    aValue);
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomFileOrDirectoryEnumerator(nsISimpleEnumerator** aValue)
{
  nsCOMPtr<nsISimpleEnumerator> iter;
  nsresult rv = GetFiles(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsBaseFilePickerEnumerator> retIter =
    new nsBaseFilePickerEnumerator(mParent, iter, mMode);

  retIter.forget(aValue);
  return NS_OK;
}

