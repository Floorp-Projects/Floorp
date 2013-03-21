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
#include "nsDOMFile.h"
#include "nsEnumeratorUtils.h"
#include "mozilla/Services.h"
#include "WidgetUtils.h"
#include "nsThreadUtils.h"

#include "nsBaseFilePicker.h"

using namespace mozilla::widget;

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

  nsBaseFilePickerEnumerator(nsISimpleEnumerator* iterator)
    : mIterator(iterator)
  {}

  virtual ~nsBaseFilePickerEnumerator()
  {}

  NS_IMETHOD
  GetNext(nsISupports** aResult)
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

    nsCOMPtr<nsIDOMFile> domFile = new nsDOMFileFile(localFile);
    domFile.forget(aResult);
    return NS_OK;
  }

  NS_IMETHOD
  HasMoreElements(bool* aResult)
  {
    return mIterator->HasMoreElements(aResult);
  }

private:
  nsCOMPtr<nsISimpleEnumerator> mIterator;
};

NS_IMPL_ISUPPORTS1(nsBaseFilePickerEnumerator, nsISimpleEnumerator)

nsBaseFilePicker::nsBaseFilePicker() :
  mAddToRecentDocs(true)
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

  InitNative(widget, aTitle, aMode);

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
    titleBundle->GetStringFromName(NS_LITERAL_STRING("allTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("allFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterHTML) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("htmlTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("htmlFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterText) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("textTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("textFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterImages) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("imageTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("imageFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterAudio) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("audioTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("audioFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterVideo) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("videoTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("videoFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXML) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("xmlTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("xmlFilter").get(), getter_Copies(filter));
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXUL) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("xulTitle").get(), getter_Copies(title));
    filterBundle->GetStringFromName(NS_LITERAL_STRING("xulFilter").get(), getter_Copies(filter));
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterApps) {
    titleBundle->GetStringFromName(NS_LITERAL_STRING("appsTitle").get(), getter_Copies(title));
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
  if (NS_FAILED(rv))
    return rv;
  return CallQueryInterface(directory, aDirectory);
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
nsBaseFilePicker::GetDomfile(nsIDOMFile** aDomfile)
{
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = GetFile(getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!localFile) {
    *aDomfile = nullptr;
    return NS_OK;
  }

  nsRefPtr<nsDOMFileFile> domFile = new nsDOMFileFile(localFile);
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
    new nsBaseFilePickerEnumerator(iter);

  retIter.forget(aDomfiles);
  return NS_OK;
}

