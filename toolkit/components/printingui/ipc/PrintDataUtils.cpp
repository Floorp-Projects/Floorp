/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintDataUtils.h"
#include "nsIPrintSettings.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserPrint.h"
#include "nsString.h"

namespace mozilla {
namespace embedding {

/**
 * MockWebBrowserPrint is a mostly useless implementation of nsIWebBrowserPrint,
 * but wraps a PrintData so that it's able to return information to print
 * settings dialogs that need an nsIWebBrowserPrint to interrogate.
 */

NS_IMPL_ISUPPORTS(MockWebBrowserPrint, nsIWebBrowserPrint);

MockWebBrowserPrint::MockWebBrowserPrint(const PrintData &aData)
  : mData(aData)
{
}

MockWebBrowserPrint::~MockWebBrowserPrint()
{
}

NS_IMETHODIMP
MockWebBrowserPrint::GetGlobalPrintSettings(nsIPrintSettings **aGlobalPrintSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetCurrentPrintSettings(nsIPrintSettings **aCurrentPrintSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetCurrentChildDOMWindow(mozIDOMWindowProxy **aCurrentPrintSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetDoingPrint(bool *aDoingPrint)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetDoingPrintPreview(bool *aDoingPrintPreview)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetIsFramesetDocument(bool *aIsFramesetDocument)
{
  *aIsFramesetDocument = mData.isFramesetDocument();
  return NS_OK;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetIsFramesetFrameSelected(bool *aIsFramesetFrameSelected)
{
  *aIsFramesetFrameSelected = mData.isFramesetFrameSelected();
  return NS_OK;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetIsIFrameSelected(bool *aIsIFrameSelected)
{
  *aIsIFrameSelected = mData.isIFrameSelected();
  return NS_OK;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetIsRangeSelection(bool *aIsRangeSelection)
{
  *aIsRangeSelection = mData.isRangeSelection();
  return NS_OK;
}

NS_IMETHODIMP
MockWebBrowserPrint::GetPrintPreviewNumPages(int32_t *aPrintPreviewNumPages)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::Print(nsIPrintSettings* aThePrintSettings,
                           nsIWebProgressListener* aWPListener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::PrintPreview(nsIPrintSettings* aThePrintSettings,
                                  mozIDOMWindowProxy* aChildDOMWin,
                                  nsIWebProgressListener* aWPListener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::PrintPreviewNavigate(int16_t aNavType,
                                          int32_t aPageNum)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::Cancel()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockWebBrowserPrint::EnumerateDocumentNames(uint32_t* aCount,
                                            char16_t*** aResult)
{
  *aCount = 0;
  *aResult = nullptr;

  if (mData.printJobName().IsEmpty()) {
    return NS_OK;
  }

  // The only consumer that cares about this is the OS X printing
  // dialog, and even then, it only cares about the first document
  // name. That's why we only send a single document name through
  // PrintData.
  char16_t** array = (char16_t**) moz_xmalloc(sizeof(char16_t*));
  array[0] = ToNewUnicode(mData.printJobName());

  *aCount = 1;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
MockWebBrowserPrint::ExitPrintPreview()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace embedding
} // namespace mozilla

