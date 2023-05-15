/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -------------------------------------------------------------------
To Build This:

  You need to add this to the the makefile.win in mozilla/dom/base:

        .\$(OBJDIR)\nsFlyOwnPrintDialog.obj	\


  And this to the makefile.win in mozilla/content/build:

WIN_LIBS=                                       \
        winspool.lib                           \
        comctl32.lib                           \
        comdlg32.lib

---------------------------------------------------------------------- */

#include <windows.h>
#include <tchar.h>

#include <unknwn.h>
#include <commdlg.h>

#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Span.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsWin.h"
#include "nsIPrinterList.h"
#include "nsServiceManagerUtils.h"

#include "nsRect.h"

#include "nsCRT.h"
#include "prenv.h" /* for PR_GetEnv */

#include <windows.h>
#include <winspool.h>

// For Localization

// For NS_CopyUnicodeToNative
#include "nsNativeCharsetUtils.h"

// This is for extending the dialog
#include <dlgs.h>

#include "nsWindowsHelpers.h"
#include "WinUtils.h"

//-----------------------------------------------
// Global Data
//-----------------------------------------------

static HWND gParentWnd = nullptr;

//----------------------------------------------------------------------------------
// Returns a Global Moveable Memory Handle to a DevMode
// from the Printer by the name of aPrintName
//
// NOTE:
//   This function assumes that aPrintName has already been converted from
//   unicode
//
static nsReturnRef<nsHGLOBAL> CreateGlobalDevModeAndInit(
    const nsString& aPrintName, nsIPrintSettings* aPS) {
  nsHPRINTER hPrinter = nullptr;
  // const cast kludge for silly Win32 api's
  LPWSTR printName =
      const_cast<wchar_t*>(static_cast<const wchar_t*>(aPrintName.get()));
  BOOL status = ::OpenPrinterW(printName, &hPrinter, nullptr);
  if (!status) {
    return nsReturnRef<nsHGLOBAL>();
  }

  // Make sure hPrinter is closed on all paths
  nsAutoPrinter autoPrinter(hPrinter);

  // Get the buffer size
  LONG needed = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, nullptr,
                                      nullptr, 0);
  if (needed < 0) {
    return nsReturnRef<nsHGLOBAL>();
  }

  // Some drivers do not return the correct size for their DEVMODE, so we
  // over-allocate to try and compensate.
  // (See https://bugzilla.mozilla.org/show_bug.cgi?id=1664530#c5)
  needed *= 2;
  nsAutoDevMode newDevMode(
      (LPDEVMODEW)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, needed));
  if (!newDevMode) {
    return nsReturnRef<nsHGLOBAL>();
  }

  nsHGLOBAL hDevMode = ::GlobalAlloc(GHND, needed);
  nsAutoGlobalMem globalDevMode(hDevMode);
  if (!hDevMode) {
    return nsReturnRef<nsHGLOBAL>();
  }

  LONG ret = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, newDevMode,
                                   nullptr, DM_OUT_BUFFER);
  if (ret != IDOK) {
    return nsReturnRef<nsHGLOBAL>();
  }

  // Lock memory and copy contents from DEVMODE (current printer)
  // to Global Memory DEVMODE
  LPDEVMODEW devMode = (DEVMODEW*)::GlobalLock(hDevMode);
  if (!devMode) {
    return nsReturnRef<nsHGLOBAL>();
  }

  memcpy(devMode, newDevMode.get(), needed);
  // Initialize values from the PrintSettings
  nsCOMPtr<nsIPrintSettingsWin> psWin = do_QueryInterface(aPS);
  MOZ_ASSERT(psWin);
  psWin->CopyToNative(devMode);

  // Sets back the changes we made to the DevMode into the Printer Driver
  ret = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, devMode, devMode,
                              DM_IN_BUFFER | DM_OUT_BUFFER);
  if (ret != IDOK) {
    ::GlobalUnlock(hDevMode);
    return nsReturnRef<nsHGLOBAL>();
  }

  ::GlobalUnlock(hDevMode);

  return globalDevMode.out();
}

//------------------------------------------------------------------
// helper
static void GetDefaultPrinterNameFromGlobalPrinters(nsAString& aPrinterName) {
  aPrinterName.Truncate();
  nsCOMPtr<nsIPrinterList> printerList =
      do_GetService("@mozilla.org/gfx/printerlist;1");
  if (printerList) {
    printerList->GetSystemDefaultPrinterName(aPrinterName);
  }
}

//------------------------------------------------------------------
// Displays the native Print Dialog
nsresult NativeShowPrintDialog(HWND aHWnd, bool aHaveSelection,
                               nsIPrintSettings* aPrintSettings) {
  // NS_ENSURE_ARG_POINTER(aHWnd);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  // Get the Print Name to be used
  nsString printerName;
  aPrintSettings->GetPrinterName(printerName);

  // If there is no name then use the default printer
  if (printerName.IsEmpty()) {
    GetDefaultPrinterNameFromGlobalPrinters(printerName);
  } else {
    HANDLE hPrinter = nullptr;
    if (!::OpenPrinterW(const_cast<wchar_t*>(
                            static_cast<const wchar_t*>(printerName.get())),
                        &hPrinter, nullptr)) {
      // If the last used printer is not found, we should use default printer.
      GetDefaultPrinterNameFromGlobalPrinters(printerName);
    } else {
      ::ClosePrinter(hPrinter);
    }
  }

  // Now create a DEVNAMES struct so the the dialog is initialized correctly.

  uint32_t len = printerName.Length();
  nsHGLOBAL hDevNames =
      ::GlobalAlloc(GHND, sizeof(wchar_t) * (len + 1) + sizeof(DEVNAMES));
  nsAutoGlobalMem autoDevNames(hDevNames);
  if (!hDevNames) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  DEVNAMES* pDevNames = (DEVNAMES*)::GlobalLock(hDevNames);
  if (!pDevNames) {
    return NS_ERROR_FAILURE;
  }
  pDevNames->wDriverOffset = sizeof(DEVNAMES) / sizeof(wchar_t);
  pDevNames->wDeviceOffset = sizeof(DEVNAMES) / sizeof(wchar_t);
  pDevNames->wOutputOffset = sizeof(DEVNAMES) / sizeof(wchar_t) + len;
  pDevNames->wDefault = 0;

  memcpy(pDevNames + 1, printerName.get(), (len + 1) * sizeof(wchar_t));
  ::GlobalUnlock(hDevNames);

  // Create a Moveable Memory Object that holds a new DevMode
  // from the Printer Name
  // The PRINTDLG.hDevMode requires that it be a moveable memory object
  // NOTE: autoDevMode is automatically freed when any error occurred
  nsAutoGlobalMem autoDevMode(
      CreateGlobalDevModeAndInit(printerName, aPrintSettings));

  // Prepare to Display the Print Dialog
  // https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms646942(v=vs.85)
  // https://docs.microsoft.com/en-us/windows/win32/api/commdlg/ns-commdlg-printdlgexw
  PRINTDLGEXW prntdlg;
  memset(&prntdlg, 0, sizeof(prntdlg));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner = aHWnd;
  prntdlg.hDevMode = autoDevMode.get();
  prntdlg.hDevNames = hDevNames;
  prntdlg.hDC = nullptr;
  prntdlg.Flags = PD_ALLPAGES | PD_RETURNIC | PD_USEDEVMODECOPIESANDCOLLATE |
                  PD_COLLATE | PD_NOCURRENTPAGE;

  // If there is a current selection then enable the "Selection" radio button
  if (!aHaveSelection) {
    prntdlg.Flags |= PD_NOSELECTION;
  }

  // 10 seems like a reasonable max number of ranges to support by default if
  // the user doesn't choose a greater thing in the UI.
  constexpr size_t kMinSupportedRanges = 10;

  AutoTArray<PRINTPAGERANGE, kMinSupportedRanges> winPageRanges;
  // Set up the page ranges.
  {
    AutoTArray<int32_t, kMinSupportedRanges * 2> pageRanges;
    aPrintSettings->GetPageRanges(pageRanges);
    // If there is a specified page range then enable the "Custom" radio button
    if (!pageRanges.IsEmpty()) {
      prntdlg.Flags |= PD_PAGENUMS;
    }

    const size_t specifiedRanges = pageRanges.Length() / 2;
    const size_t maxRanges = std::max(kMinSupportedRanges, specifiedRanges);

    prntdlg.nMaxPageRanges = maxRanges;
    prntdlg.nPageRanges = specifiedRanges;

    winPageRanges.SetCapacity(maxRanges);
    for (size_t i = 0; i < pageRanges.Length(); i += 2) {
      PRINTPAGERANGE* range = winPageRanges.AppendElement();
      range->nFromPage = pageRanges[i];
      range->nToPage = pageRanges[i + 1];
    }
    prntdlg.lpPageRanges = winPageRanges.Elements();

    prntdlg.nMinPage = 1;
    // TODO(emilio): Could probably get the right page number here from the
    // new print UI.
    prntdlg.nMaxPage = 0xFFFF;
  }

  // NOTE(emilio): This can always be 1 because we use the DEVMODE copies
  // feature (see PD_USEDEVMODECOPIESANDCOLLATE).
  prntdlg.nCopies = 1;

  prntdlg.hInstance = nullptr;
  prntdlg.lpPrintTemplateName = nullptr;

  prntdlg.lpCallback = nullptr;
  prntdlg.nPropertyPages = 0;
  prntdlg.lphPropertyPages = nullptr;

  prntdlg.nStartPage = START_PAGE_GENERAL;
  prntdlg.dwResultAction = 0;

  HRESULT result;
  {
    mozilla::widget::WinUtils::AutoSystemDpiAware dpiAwareness;
    mozilla::BackgroundHangMonitor().NotifyWait();
    result = ::PrintDlgExW(&prntdlg);
  }

  auto cancelOnExit = mozilla::MakeScopeExit([&] { ::SetFocus(aHWnd); });

  if (NS_WARN_IF(!SUCCEEDED(result))) {
#ifdef DEBUG
    printf_stderr("PrintDlgExW failed with %lx\n", result);
#endif
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(prntdlg.dwResultAction != PD_RESULT_PRINT)) {
    return NS_ERROR_ABORT;
  }
  // check to make sure we don't have any nullptr pointers
  NS_ENSURE_TRUE(prntdlg.hDevMode, NS_ERROR_ABORT);
  NS_ENSURE_TRUE(prntdlg.hDevNames, NS_ERROR_ABORT);
  // Lock the deviceNames and check for nullptr
  DEVNAMES* devnames = (DEVNAMES*)::GlobalLock(prntdlg.hDevNames);
  NS_ENSURE_TRUE(devnames, NS_ERROR_ABORT);

  char16_t* device = &(((char16_t*)devnames)[devnames->wDeviceOffset]);
  char16_t* driver = &(((char16_t*)devnames)[devnames->wDriverOffset]);

  // Check to see if the "Print To File" control is checked
  // then take the name from devNames and set it in the PrintSettings
  //
  // NOTE:
  // As per Microsoft SDK documentation the returned value offset from
  // devnames->wOutputOffset is either "FILE:" or nullptr
  // if the "Print To File" checkbox is checked it MUST be "FILE:"
  // We assert as an extra safety check.
  if (prntdlg.Flags & PD_PRINTTOFILE) {
    char16ptr_t fileName = &(((wchar_t*)devnames)[devnames->wOutputOffset]);
    NS_ASSERTION(wcscmp(fileName, L"FILE:") == 0, "FileName must be `FILE:`");
    aPrintSettings->SetOutputDestination(
        nsIPrintSettings::kOutputDestinationFile);
    aPrintSettings->SetToFileName(nsDependentString(fileName));
  } else {
    // clear "print to file" info
    aPrintSettings->SetOutputDestination(
        nsIPrintSettings::kOutputDestinationPrinter);
    aPrintSettings->SetToFileName(u""_ns);
  }

  nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(aPrintSettings));
  MOZ_RELEASE_ASSERT(psWin);

  // Setup local Data members
  psWin->SetDeviceName(nsDependentString(device));
  psWin->SetDriverName(nsDependentString(driver));

  // Fill the print options with the info from the dialog
  aPrintSettings->SetPrinterName(nsDependentString(device));
  aPrintSettings->SetPrintSelectionOnly(prntdlg.Flags & PD_SELECTION);

  AutoTArray<int32_t, kMinSupportedRanges * 2> pageRanges;
  if (prntdlg.Flags & PD_PAGENUMS) {
    pageRanges.SetCapacity(prntdlg.nPageRanges * 2);
    for (const auto& range :
         mozilla::Span(prntdlg.lpPageRanges, prntdlg.nPageRanges)) {
      pageRanges.AppendElement(range.nFromPage);
      pageRanges.AppendElement(range.nToPage);
    }
  }
  aPrintSettings->SetPageRanges(pageRanges);

  // Unlock DeviceNames
  ::GlobalUnlock(prntdlg.hDevNames);

  // Transfer the settings from the native data to the PrintSettings
  LPDEVMODEW devMode = (LPDEVMODEW)::GlobalLock(prntdlg.hDevMode);
  if (!devMode || !prntdlg.hDC) {
    return NS_ERROR_FAILURE;
  }
  psWin->SetDevMode(devMode);  // copies DevMode
  psWin->CopyFromNative(prntdlg.hDC, devMode);
  ::GlobalUnlock(prntdlg.hDevMode);
  ::DeleteDC(prntdlg.hDC);

  cancelOnExit.release();
  return NS_OK;
}
