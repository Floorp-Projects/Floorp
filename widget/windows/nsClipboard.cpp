/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboard.h"

#include <shlobj.h>
#include <intshcut.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#include <functional>
#include <thread>
#include <chrono>

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_clipboard.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDataObj.h"
#include "nsString.h"
#include "nsNativeCharsetUtils.h"
#include "nsIInputStream.h"
#include "nsITransferable.h"
#include "nsXPCOM.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIFileProtocolHandler.h"
#include "nsEscape.h"
#include "nsIObserverService.h"
#include "nsMimeTypes.h"
#include "imgITools.h"

using mozilla::LogLevel;

static mozilla::LazyLogModule gWin32ClipboardLog("nsClipboard");

/* static */
UINT nsClipboard::GetHtmlClipboardFormat() {
  static UINT format = ::RegisterClipboardFormatW(L"HTML Format");
  return format;
}

/* static */
UINT nsClipboard::GetCustomClipboardFormat() {
  static UINT format =
      ::RegisterClipboardFormatW(L"application/x-moz-custom-clipdata");
  return format;
}

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard() {
  mIgnoreEmptyNotification = false;
  mWindow = nullptr;

  // Register for a shutdown notification so that we can flush data
  // to the OS clipboard.
  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                 PR_FALSE);
  }
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard() {}

NS_IMPL_ISUPPORTS_INHERITED(nsClipboard, nsBaseClipboard, nsIObserver)

NS_IMETHODIMP
nsClipboard::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  // This will be called on shutdown.
  ::OleFlushClipboard();
  ::CloseClipboard();

  return NS_OK;
}

//-------------------------------------------------------------------------
UINT nsClipboard::GetFormat(const char* aMimeStr, bool aMapHTMLMime) {
  UINT format;

  if (strcmp(aMimeStr, kTextMime) == 0) {
    format = CF_TEXT;
  } else if (strcmp(aMimeStr, kUnicodeMime) == 0) {
    format = CF_UNICODETEXT;
  } else if (strcmp(aMimeStr, kRTFMime) == 0) {
    format = ::RegisterClipboardFormat(L"Rich Text Format");
  } else if (strcmp(aMimeStr, kJPEGImageMime) == 0 ||
             strcmp(aMimeStr, kJPGImageMime) == 0 ||
             strcmp(aMimeStr, kPNGImageMime) == 0) {
    format = CF_DIBV5;
  } else if (strcmp(aMimeStr, kFileMime) == 0 ||
             strcmp(aMimeStr, kFilePromiseMime) == 0) {
    format = CF_HDROP;
  } else if ((strcmp(aMimeStr, kNativeHTMLMime) == 0) ||
             (aMapHTMLMime && strcmp(aMimeStr, kHTMLMime) == 0)) {
    format = GetHtmlClipboardFormat();
  } else if (strcmp(aMimeStr, kCustomTypesMime) == 0) {
    format = GetCustomClipboardFormat();
  } else {
    format = ::RegisterClipboardFormatW(NS_ConvertASCIItoUTF16(aMimeStr).get());
  }

  return format;
}

//-------------------------------------------------------------------------
// static
nsresult nsClipboard::CreateNativeDataObject(
    nsITransferable* aTransferable, IDataObject** aDataObj, nsIURI* aUri,
    MightNeedToFlush* aMightNeedToFlush) {
  MOZ_ASSERT(aTransferable);
  if (!aTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Create our native DataObject that implements the OLE IDataObject interface
  RefPtr<nsDataObj> dataObj = new nsDataObj(aUri);

  // Now set it up with all the right data flavors & enums
  nsresult res =
      SetupNativeDataObject(aTransferable, dataObj, aMightNeedToFlush);
  if (NS_SUCCEEDED(res)) {
    dataObj.forget(aDataObj);
  }
  return res;
}

static nsresult StoreValueInDataObject(nsDataObj* aObj,
                                       LPCWSTR aClipboardFormat, DWORD value) {
  HGLOBAL hGlobalMemory = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (!hGlobalMemory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  DWORD* pdw = (DWORD*)::GlobalLock(hGlobalMemory);
  *pdw = value;
  ::GlobalUnlock(hGlobalMemory);

  STGMEDIUM stg;
  stg.tymed = TYMED_HGLOBAL;
  stg.pUnkForRelease = nullptr;
  stg.hGlobal = hGlobalMemory;

  FORMATETC fe;
  SET_FORMATETC(fe, ::RegisterClipboardFormat(aClipboardFormat), 0,
                DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
  aObj->SetData(&fe, &stg, TRUE);

  return NS_OK;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::SetupNativeDataObject(
    nsITransferable* aTransferable, IDataObject* aDataObj,
    MightNeedToFlush* aMightNeedToFlush) {
  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(aDataObj);
  if (!aTransferable || !aDataObj) {
    return NS_ERROR_FAILURE;
  }

  auto* dObj = static_cast<nsDataObj*>(aDataObj);

  if (aMightNeedToFlush) {
    *aMightNeedToFlush = MightNeedToFlush::No;
  }

  auto MarkAsPotentiallyNeedingToFlush = [&] {
    if (aMightNeedToFlush) {
      *aMightNeedToFlush = MightNeedToFlush::Yes;
    }
  };

  // Now give the Transferable to the DataObject
  // for getting the data out of it
  dObj->SetTransferable(aTransferable);

  // Get the transferable list of data flavors
  nsTArray<nsCString> flavors;
  aTransferable->FlavorsTransferableCanExport(flavors);

  // Walk through flavors that contain data and register them
  // into the DataObj as supported flavors
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];

    // When putting data onto the clipboard, we want to maintain kHTMLMime
    // ("text/html") and not map it to CF_HTML here since this will be done
    // below.
    UINT format = GetFormat(flavorStr.get(), false);

    // Now tell the native IDataObject about both our mime type and
    // the native data format
    FORMATETC fe;
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
    dObj->AddDataFlavor(flavorStr.get(), &fe);

    // Do various things internal to the implementation, like map one
    // flavor to another or add additional flavors based on what's required
    // for the win32 impl.
    if (flavorStr.EqualsLiteral(kUnicodeMime)) {
      // if we find text/unicode, also advertise text/plain (which we will
      // convert on our own in nsDataObj::GetText().
      FORMATETC textFE;
      SET_FORMATETC(textFE, CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
      dObj->AddDataFlavor(kTextMime, &textFE);
      MarkAsPotentiallyNeedingToFlush();
    } else if (flavorStr.EqualsLiteral(kHTMLMime)) {
      // if we find text/html, also advertise win32's html flavor (which we will
      // convert on our own in nsDataObj::GetText().
      FORMATETC htmlFE;
      SET_FORMATETC(htmlFE, GetHtmlClipboardFormat(), 0, DVASPECT_CONTENT, -1,
                    TYMED_HGLOBAL);
      dObj->AddDataFlavor(kHTMLMime, &htmlFE);
      MarkAsPotentiallyNeedingToFlush();
    } else if (flavorStr.EqualsLiteral(kURLMime)) {
      // if we're a url, in addition to also being text, we need to register
      // the "file" flavors so that the win32 shell knows to create an internet
      // shortcut when it sees one of these beasts.
      FORMATETC shortcutFE;
      SET_FORMATETC(shortcutFE,
                    ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE,
                    ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILECONTENTS),
                    0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_INETURLA), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_INETURLW), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      MarkAsPotentiallyNeedingToFlush();
    } else if (flavorStr.EqualsLiteral(kPNGImageMime) ||
               flavorStr.EqualsLiteral(kJPEGImageMime) ||
               flavorStr.EqualsLiteral(kJPGImageMime) ||
               flavorStr.EqualsLiteral(kGIFImageMime) ||
               flavorStr.EqualsLiteral(kNativeImageMime)) {
      // if we're an image, register the native bitmap flavor
      FORMATETC imageFE;
      // Add DIBv5
      SET_FORMATETC(imageFE, CF_DIBV5, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(flavorStr.get(), &imageFE);
      // Add DIBv3
      SET_FORMATETC(imageFE, CF_DIB, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(flavorStr.get(), &imageFE);
    } else if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
      // if we're a file promise flavor, also register the
      // CFSTR_PREFERREDDROPEFFECT format.  The data object
      // returns a value of DROPEFFECTS_MOVE to the drop target
      // when it asks for the value of this format.  This causes
      // the file to be moved from the temporary location instead
      // of being copied.  The right thing to do here is to call
      // SetData() on the data object and set the value of this format
      // to DROPEFFECTS_MOVE on this particular data object.  But,
      // since all the other clipboard formats follow the model of setting
      // data on the data object only when the drop object calls GetData(),
      // I am leaving this format's value hard coded in the data object.
      // We can change this if other consumers of this format get added to this
      // codebase and they need different values.
      FORMATETC shortcutFE;
      SET_FORMATETC(shortcutFE,
                    ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kFilePromiseMime, &shortcutFE);
    }
  }

  if (!StaticPrefs::clipboard_copyPrivateDataToClipboardCloudOrHistory()) {
    // Let Clipboard know that data is sensitive and must not be copied to
    // the Cloud Clipboard, Clipboard History and similar.
    // https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats#cloud-clipboard-and-clipboard-history-formats
    if (aTransferable->GetIsPrivateData()) {
      nsresult rv =
          StoreValueInDataObject(dObj, TEXT("CanUploadToCloudClipboard"), 0);
      NS_ENSURE_SUCCESS(rv, rv);
      rv =
          StoreValueInDataObject(dObj, TEXT("CanIncludeInClipboardHistory"), 0);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = StoreValueInDataObject(
          dObj, TEXT("ExcludeClipboardContentFromMonitorProcessing"), 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// See methods listed at
// <https://docs.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-idataobject#methods>.
static void IDataObjectMethodResultToString(const HRESULT aHres,
                                            nsACString& aResult) {
  switch (aHres) {
    case E_INVALIDARG:
      aResult = "E_INVALIDARG";
      break;
    case E_UNEXPECTED:
      aResult = "E_UNEXPECTED";
      break;
    case E_OUTOFMEMORY:
      aResult = "E_OUTOFMEMORY";
      break;
    case DV_E_LINDEX:
      aResult = "DV_E_LINDEX";
      break;
    case DV_E_FORMATETC:
      aResult = "DV_E_FORMATETC";
      break;
    case DV_E_TYMED:
      aResult = "DV_E_TYMED";
      break;
    case DV_E_DVASPECT:
      aResult = "DV_E_DVASPECT";
      break;
    case OLE_E_NOTRUNNING:
      aResult = "OLE_E_NOTRUNNING";
      break;
    case STG_E_MEDIUMFULL:
      aResult = "STG_E_MEDIUMFULL";
      break;
    case DV_E_CLIPFORMAT:
      aResult = "DV_E_CLIPFORMAT";
      break;
    case S_OK:
      aResult = "S_OK";
      break;
    default:
      // Explicit template instantiaton, because otherwise the call is
      // ambiguous.
      constexpr int kRadix = 16;
      aResult = IntToCString<int32_t>(aHres, kRadix);
      break;
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olegetclipboard>.
static void OleGetClipboardResultToString(const HRESULT aHres,
                                          nsACString& aResult) {
  switch (aHres) {
    case S_OK:
      aResult = "S_OK";
      break;
    case CLIPBRD_E_CANT_OPEN:
      aResult = "CLIPBRD_E_CANT_OPEN";
      break;
    case CLIPBRD_E_CANT_CLOSE:
      aResult = "CLIPBRD_E_CANT_CLOSE";
      break;
    default:
      // Explicit template instantiaton, because otherwise the call is
      // ambiguous.
      constexpr int kRadix = 16;
      aResult = IntToCString<int32_t>(aHres, kRadix);
      break;
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olegetclipboard>.
static void LogOleGetClipboardResult(const HRESULT aHres) {
  if (MOZ_LOG_TEST(gWin32ClipboardLog, LogLevel::Debug)) {
    nsAutoCString hresString;
    OleGetClipboardResultToString(aHres, hresString);
    MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug,
            ("OleGetClipboard result: %s", hresString.get()));
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olesetclipboard>.
static void OleSetClipboardResultToString(HRESULT aHres, nsACString& aResult) {
  switch (aHres) {
    case S_OK:
      aResult = "S_OK";
      break;
    case CLIPBRD_E_CANT_OPEN:
      aResult = "CLIPBRD_E_CANT_OPEN";
      break;
    case CLIPBRD_E_CANT_EMPTY:
      aResult = "CLIPBRD_E_CANT_EMPTY";
      break;
    case CLIPBRD_E_CANT_CLOSE:
      aResult = "CLIPBRD_E_CANT_CLOSE";
      break;
    case CLIPBRD_E_CANT_SET:
      aResult = "CLIPBRD_E_CANT_SET";
      break;
    default:
      // Explicit template instantiaton, because otherwise the call is
      // ambiguous.
      constexpr int kRadix = 16;
      aResult = IntToCString<int32_t>(aHres, kRadix);
      break;
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olesetclipboard>.
static void LogOleSetClipboardResult(const HRESULT aHres) {
  if (MOZ_LOG_TEST(gWin32ClipboardLog, LogLevel::Debug)) {
    nsAutoCString hresString;
    OleSetClipboardResultToString(aHres, hresString);
    MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug,
            ("OleSetClipboard result: %s", hresString.get()));
  }
}

template <typename Function, typename LogFunction, typename... Args>
static HRESULT RepeatedlyTry(Function aFunction, LogFunction aLogFunction,
                             Args... aArgs) {
  // These are magic values based on local testing. They are chosen not higher
  // to avoid jank (<https://developer.mozilla.org/en-US/docs/Glossary/Jank>).
  // When changing them, be careful.
  static constexpr int kNumberOfTries = 3;
  static constexpr int kDelayInMs = 3;

  HRESULT hres;
  for (int i = 0; i < kNumberOfTries; ++i) {
    hres = aFunction(aArgs...);
    aLogFunction(hres);

    if (hres == S_OK) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kDelayInMs));
  }

  return hres;
}

// Other apps can block access to the clipboard. This repeatedly
// calls `::OleSetClipboard` for a fixed number of times and should be called
// instead of `::OleSetClipboard`.
static void RepeatedlyTryOleSetClipboard(IDataObject* aDataObj) {
  RepeatedlyTry(::OleSetClipboard, LogOleSetClipboardResult, aDataObj);
}

static bool ShouldFlushClipboardAfterWriting() {
  switch (StaticPrefs::widget_windows_sync_clipboard_flush()) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      // Bug 1774285: Windows Suggested Actions (introduced in Windows 11 22H2)
      // walks the entire a11y tree using UIA if something is placed on the
      // clipboard using delayed rendering. (The OLE clipboard always uses
      // delayed rendering.) This a11y tree walk causes an unacceptable hang,
      // particularly when the a11y cache is disabled. We choose the lesser of
      // the two performance/memory evils here and force immediate rendering.
      return NeedsWindows11SuggestedActionsWorkaround();
  }
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(int32_t aWhichClipboard) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug, ("%s", __FUNCTION__));

  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_FAILURE;
  }

  mIgnoreEmptyNotification = true;

  // make sure we have a good transferable
  if (!mTransferable) {
    return NS_ERROR_FAILURE;
  }

#ifdef ACCESSIBILITY
  a11y::Compatibility::SuppressA11yForClipboardCopy();
#endif

  RefPtr<IDataObject> dataObj;
  auto mightNeedToFlush = MightNeedToFlush::No;
  if (NS_SUCCEEDED(CreateNativeDataObject(mTransferable,
                                          getter_AddRefs(dataObj), nullptr,
                                          &mightNeedToFlush))) {
    RepeatedlyTryOleSetClipboard(dataObj);
    if (mightNeedToFlush == MightNeedToFlush::Yes &&
        ShouldFlushClipboardAfterWriting()) {
      RepeatedlyTry(::OleFlushClipboard, [](HRESULT) {});
    }
  } else {
    // Clear the native clipboard
    RepeatedlyTryOleSetClipboard(nullptr);
  }

  mIgnoreEmptyNotification = false;

  return NS_OK;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetGlobalData(HGLOBAL aHGBL, void** aData,
                                    uint32_t* aLen) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Verbose, ("%s", __FUNCTION__));

  // Allocate a new memory buffer and copy the data from global memory.
  // Recall that win98 allocates to nearest DWORD boundary. As a safety
  // precaution, allocate an extra 3 bytes (but don't report them in |aLen|!)
  // and null them out to ensure that all of our NS_strlen calls will succeed.
  // NS_strlen operates on char16_t, so we need 3 NUL bytes to ensure it finds
  // a full NUL char16_t when |*aLen| is odd.
  nsresult result = NS_ERROR_FAILURE;
  if (aHGBL != nullptr) {
    LPSTR lpStr = (LPSTR)GlobalLock(aHGBL);
    mozilla::CheckedInt<uint32_t> allocSize =
        mozilla::CheckedInt<uint32_t>(GlobalSize(aHGBL)) + 3;
    if (!allocSize.isValid()) {
      return NS_ERROR_INVALID_ARG;
    }
    char* data = static_cast<char*>(malloc(allocSize.value()));
    if (data) {
      uint32_t size = allocSize.value() - 3;
      memcpy(data, lpStr, size);
      // null terminate for safety
      data[size] = data[size + 1] = data[size + 2] = '\0';

      GlobalUnlock(aHGBL);
      *aData = data;
      *aLen = size;

      result = NS_OK;
    }
  } else {
    // We really shouldn't ever get here
    // but just in case
    *aData = nullptr;
    *aLen = 0;
    LPVOID lpMsgBuf;

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
        (LPWSTR)&lpMsgBuf, 0, nullptr);

    // Display the string.
    MessageBoxW(nullptr, (LPCWSTR)lpMsgBuf, L"GetLastError",
                MB_OK | MB_ICONINFORMATION);

    // Free the buffer.
    LocalFree(lpMsgBuf);
  }

  return result;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetNativeDataOffClipboard(nsIWidget* aWidget,
                                                UINT /*aIndex*/, UINT aFormat,
                                                void** aData, uint32_t* aLen) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug,
          ("%s: overload taking nsIWidget*.", __FUNCTION__));

  HGLOBAL hglb;
  nsresult result = NS_ERROR_FAILURE;

  HWND nativeWin = nullptr;
  if (::OpenClipboard(nativeWin)) {
    hglb = ::GetClipboardData(aFormat);
    result = GetGlobalData(hglb, aData, aLen);
    ::CloseClipboard();
  }
  return result;
}

// See methods listed at
// <https://docs.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-idataobject#methods>.
static void LogIDataObjectMethodResult(const HRESULT aHres,
                                       const nsCString& aMethodName) {
  if (MOZ_LOG_TEST(gWin32ClipboardLog, LogLevel::Debug)) {
    nsAutoCString hresString;
    IDataObjectMethodResultToString(aHres, hresString);
    MOZ_LOG(
        gWin32ClipboardLog, LogLevel::Debug,
        ("IDataObject::%s result: %s", aMethodName.get(), hresString.get()));
  }
}

// Other apps can block access to the clipboard. This repeatedly calls
// `GetData` for a fixed number of times and should be called instead of
// `GetData`. See
// <https://docs.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-getdata>.
// While Microsoft's documentation doesn't include `CLIPBRD_E_CANT_OPEN`
// explicitly, it allows it implicitly and in local experiments it was indeed
// returned.
static HRESULT RepeatedlyTryGetData(IDataObject& aDataObject, LPFORMATETC pFE,
                                    LPSTGMEDIUM pSTM) {
  return RepeatedlyTry(
      [&aDataObject, &pFE, &pSTM]() { return aDataObject.GetData(pFE, pSTM); },
      std::bind(LogIDataObjectMethodResult, std::placeholders::_1,
                "GetData"_ns));
}

//-------------------------------------------------------------------------
// static
HRESULT nsClipboard::FillSTGMedium(IDataObject* aDataObject, UINT aFormat,
                                   LPFORMATETC pFE, LPSTGMEDIUM pSTM,
                                   DWORD aTymed) {
  SET_FORMATETC(*pFE, aFormat, 0, DVASPECT_CONTENT, -1, aTymed);

  // Starting by querying for the data to see if we can get it as from global
  // memory
  HRESULT hres = S_FALSE;
  hres = aDataObject->QueryGetData(pFE);
  LogIDataObjectMethodResult(hres, "QueryGetData"_ns);
  if (S_OK == hres) {
    hres = RepeatedlyTryGetData(*aDataObject, pFE, pSTM);
  }
  return hres;
}

//-------------------------------------------------------------------------
// If aFormat is CF_DIBV5, aMIMEImageFormat must be a type for which we have
// an image encoder (e.g. image/png).
// For other values of aFormat, it is OK to pass null for aMIMEImageFormat.
nsresult nsClipboard::GetNativeDataOffClipboard(IDataObject* aDataObject,
                                                UINT aIndex, UINT aFormat,
                                                const char* aMIMEImageFormat,
                                                void** aData, uint32_t* aLen) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug,
          ("%s: overload taking IDataObject*.", __FUNCTION__));

  nsresult result = NS_ERROR_FAILURE;
  *aData = nullptr;
  *aLen = 0;

  if (!aDataObject) {
    return result;
  }

  UINT format = aFormat;
  HRESULT hres = S_FALSE;

  // XXX at the moment we only support global memory transfers
  // It is here where we will add support for native images
  // and IStream
  FORMATETC fe;
  STGMEDIUM stm;
  hres = FillSTGMedium(aDataObject, format, &fe, &stm, TYMED_HGLOBAL);

  // Currently this is only handling TYMED_HGLOBAL data
  // For Text, Dibs, Files, and generic data (like HTML)
  if (S_OK == hres) {
    static CLIPFORMAT fileDescriptorFlavorA =
        ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA);
    static CLIPFORMAT fileDescriptorFlavorW =
        ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
    static CLIPFORMAT fileFlavor =
        ::RegisterClipboardFormat(CFSTR_FILECONTENTS);
    static CLIPFORMAT preferredDropEffect =
        ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);

    switch (stm.tymed) {
      case TYMED_HGLOBAL: {
        switch (fe.cfFormat) {
          case CF_TEXT: {
            // Get the data out of the global data handle. The size we
            // return should not include the null because the other
            // platforms don't use nulls, so just return the length we get
            // back from strlen(), since we know CF_TEXT is null
            // terminated. Recall that GetGlobalData() returns the size of
            // the allocated buffer, not the size of the data (on 98, these
            // are not the same) so we can't use that.
            uint32_t allocLen = 0;
            if (NS_SUCCEEDED(GetGlobalData(stm.hGlobal, aData, &allocLen))) {
              *aLen = strlen(reinterpret_cast<char*>(*aData));
              result = NS_OK;
            }
          } break;

          case CF_UNICODETEXT: {
            // Get the data out of the global data handle. The size we
            // return should not include the null because the other
            // platforms don't use nulls, so just return the length we get
            // back from strlen(), since we know CF_UNICODETEXT is null
            // terminated. Recall that GetGlobalData() returns the size of
            // the allocated buffer, not the size of the data (on 98, these
            // are not the same) so we can't use that.
            uint32_t allocLen = 0;
            if (NS_SUCCEEDED(GetGlobalData(stm.hGlobal, aData, &allocLen))) {
              *aLen = NS_strlen(reinterpret_cast<char16_t*>(*aData)) * 2;
              result = NS_OK;
            }
          } break;

          case CF_DIBV5:
            if (aMIMEImageFormat) {
              uint32_t allocLen = 0;
              const char* clipboardData;
              if (NS_SUCCEEDED(GetGlobalData(
                      stm.hGlobal, (void**)&clipboardData, &allocLen))) {
                nsCOMPtr<imgIContainer> container;
                nsCOMPtr<imgITools> imgTools =
                    do_CreateInstance("@mozilla.org/image/tools;1");
                result = imgTools->DecodeImageFromBuffer(
                    clipboardData, allocLen,
                    nsLiteralCString(IMAGE_BMP_MS_CLIPBOARD),
                    getter_AddRefs(container));
                if (NS_FAILED(result)) {
                  break;
                }

                nsAutoCString mimeType;
                if (strcmp(aMIMEImageFormat, kJPGImageMime) == 0) {
                  mimeType.Assign(IMAGE_JPEG);
                } else {
                  mimeType.Assign(aMIMEImageFormat);
                }

                nsCOMPtr<nsIInputStream> inputStream;
                result = imgTools->EncodeImage(container, mimeType, u""_ns,
                                               getter_AddRefs(inputStream));
                if (NS_FAILED(result)) {
                  break;
                }

                if (!inputStream) {
                  result = NS_ERROR_FAILURE;
                  break;
                }

                *aData = inputStream.forget().take();
                *aLen = sizeof(nsIInputStream*);
              }
            }
            break;

          case CF_HDROP: {
            // in the case of a file drop, multiple files are stashed within a
            // single data object. In order to match mozilla's D&D apis, we
            // just pull out the file at the requested index, pretending as
            // if there really are multiple drag items.
            HDROP dropFiles = (HDROP)GlobalLock(stm.hGlobal);

            UINT numFiles = ::DragQueryFileW(dropFiles, 0xFFFFFFFF, nullptr, 0);
            NS_ASSERTION(numFiles > 0,
                         "File drop flavor, but no files...hmmmm");
            NS_ASSERTION(aIndex < numFiles,
                         "Asked for a file index out of range of list");
            if (numFiles > 0) {
              UINT fileNameLen =
                  ::DragQueryFileW(dropFiles, aIndex, nullptr, 0);
              wchar_t* buffer = reinterpret_cast<wchar_t*>(
                  moz_xmalloc((fileNameLen + 1) * sizeof(wchar_t)));
              ::DragQueryFileW(dropFiles, aIndex, buffer, fileNameLen + 1);
              *aData = buffer;
              *aLen = fileNameLen * sizeof(char16_t);
              result = NS_OK;
            }
            GlobalUnlock(stm.hGlobal);

          } break;

          default: {
            if (fe.cfFormat == fileDescriptorFlavorA ||
                fe.cfFormat == fileDescriptorFlavorW ||
                fe.cfFormat == fileFlavor) {
              NS_WARNING(
                  "Mozilla doesn't yet understand how to read this type of "
                  "file flavor");
            } else {
              // Get the data out of the global data handle. The size we
              // return should not include the null because the other
              // platforms don't use nulls, so just return the length we get
              // back from strlen(), since we know CF_UNICODETEXT is null
              // terminated. Recall that GetGlobalData() returns the size of
              // the allocated buffer, not the size of the data (on 98, these
              // are not the same) so we can't use that.
              //
              // NOTE: we are assuming that anything that falls into this
              //        default case is unicode. As we start to get more
              //        kinds of binary data, this may become an incorrect
              //        assumption. Stay tuned.
              uint32_t allocLen = 0;
              if (NS_SUCCEEDED(GetGlobalData(stm.hGlobal, aData, &allocLen))) {
                if (fe.cfFormat == GetHtmlClipboardFormat()) {
                  // CF_HTML is actually UTF8, not unicode, so disregard the
                  // assumption above. We have to check the header for the
                  // actual length, and we'll do that in FindPlatformHTML().
                  // For now, return the allocLen. This case is mostly to
                  // ensure we don't try to call strlen on the buffer.
                  *aLen = allocLen;
                } else if (fe.cfFormat == GetCustomClipboardFormat()) {
                  // Binary data
                  *aLen = allocLen;
                } else if (fe.cfFormat == preferredDropEffect) {
                  // As per the MSDN doc entitled: "Shell Clipboard Formats"
                  // CFSTR_PREFERREDDROPEFFECT should return a DWORD
                  // Reference:
                  // http://msdn.microsoft.com/en-us/library/bb776902(v=vs.85).aspx
                  NS_ASSERTION(
                      allocLen == sizeof(DWORD),
                      "CFSTR_PREFERREDDROPEFFECT should return a DWORD");
                  *aLen = allocLen;
                } else {
                  *aLen = NS_strlen(reinterpret_cast<char16_t*>(*aData)) *
                          sizeof(char16_t);
                }
                result = NS_OK;
              }
            }
          } break;
        }  // switch
      } break;

      case TYMED_GDI: {
#ifdef DEBUG
        MOZ_LOG(gWin32ClipboardLog, LogLevel::Info,
                ("*********************** TYMED_GDI\n"));
#endif
      } break;

      default:
        break;
    }  // switch

    ReleaseStgMedium(&stm);
  }

  return result;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetDataFromDataObject(IDataObject* aDataObject,
                                            UINT anIndex, nsIWidget* aWindow,
                                            nsITransferable* aTransferable) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // make sure we have a good transferable
  if (!aTransferable) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult res = NS_ERROR_FAILURE;

  // get flavor list that includes all flavors that can be written (including
  // ones obtained through conversion)
  nsTArray<nsCString> flavors;
  res = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(res)) {
    return NS_ERROR_FAILURE;
  }

  // Walk through flavors and see which flavor is on the clipboard them on the
  // native clipboard,
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];
    UINT format = GetFormat(flavorStr.get());

    // Try to get the data using the desired flavor. This might fail, but all is
    // not lost.
    void* data = nullptr;
    uint32_t dataLen = 0;
    bool dataFound = false;
    if (nullptr != aDataObject) {
      if (NS_SUCCEEDED(GetNativeDataOffClipboard(aDataObject, anIndex, format,
                                                 flavorStr.get(), &data,
                                                 &dataLen))) {
        dataFound = true;
      }
    } else if (nullptr != aWindow) {
      if (NS_SUCCEEDED(GetNativeDataOffClipboard(aWindow, anIndex, format,
                                                 &data, &dataLen))) {
        dataFound = true;
      }
    }

    // This is our second chance to try to find some data, having not found it
    // when directly asking for the flavor. Let's try digging around in other
    // flavors to help satisfy our craving for data.
    if (!dataFound) {
      if (flavorStr.EqualsLiteral(kUnicodeMime)) {
        dataFound =
            FindUnicodeFromPlainText(aDataObject, anIndex, &data, &dataLen);
      } else if (flavorStr.EqualsLiteral(kURLMime)) {
        // drags from other windows apps expose the native
        // CFSTR_INETURL{A,W} flavor
        dataFound = FindURLFromNativeURL(aDataObject, anIndex, &data, &dataLen);
        if (!dataFound) {
          dataFound =
              FindURLFromLocalFile(aDataObject, anIndex, &data, &dataLen);
        }
      }
    }  // if we try one last ditch effort to find our data

    // Hopefully by this point we've found it and can go about our business
    if (dataFound) {
      nsCOMPtr<nsISupports> genericDataWrapper;
      if (flavorStr.EqualsLiteral(kFileMime)) {
        // we have a file path in |data|. Create an nsLocalFile object.
        nsDependentString filepath(reinterpret_cast<char16_t*>(data));
        nsCOMPtr<nsIFile> file;
        if (NS_SUCCEEDED(
                NS_NewLocalFile(filepath, false, getter_AddRefs(file)))) {
          genericDataWrapper = do_QueryInterface(file);
        }
        free(data);
      } else if (flavorStr.EqualsLiteral(kNativeHTMLMime)) {
        uint32_t dummy;
        // the editor folks want CF_HTML exactly as it's on the clipboard, no
        // conversions, no fancy stuff. Pull it off the clipboard, stuff it into
        // a wrapper and hand it back to them.
        if (FindPlatformHTML(aDataObject, anIndex, &data, &dummy, &dataLen)) {
          nsPrimitiveHelpers::CreatePrimitiveForData(
              flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper));
        } else {
          free(data);
          continue;  // something wrong with this flavor, keep looking for other
                     // data
        }
        free(data);
      } else if (flavorStr.EqualsLiteral(kHTMLMime)) {
        uint32_t startOfData = 0;
        // The JS folks want CF_HTML exactly as it is on the clipboard, but
        // minus the CF_HTML header index information.
        // It also needs to be converted to UTF16 and have linebreaks changed.
        if (FindPlatformHTML(aDataObject, anIndex, &data, &startOfData,
                             &dataLen)) {
          dataLen -= startOfData;
          nsPrimitiveHelpers::CreatePrimitiveForCFHTML(
              static_cast<char*>(data) + startOfData, &dataLen,
              getter_AddRefs(genericDataWrapper));
        } else {
          free(data);
          continue;  // something wrong with this flavor, keep looking for other
                     // data
        }
        free(data);
      } else if (flavorStr.EqualsLiteral(kJPEGImageMime) ||
                 flavorStr.EqualsLiteral(kJPGImageMime) ||
                 flavorStr.EqualsLiteral(kPNGImageMime)) {
        nsIInputStream* imageStream = reinterpret_cast<nsIInputStream*>(data);
        genericDataWrapper = do_QueryInterface(imageStream);
        NS_IF_RELEASE(imageStream);
      } else {
        // Treat custom types as a string of bytes.
        if (!flavorStr.EqualsLiteral(kCustomTypesMime)) {
          // we probably have some form of text. The DOM only wants LF, so
          // convert from Win32 line endings to DOM line endings.
          int32_t signedLen = static_cast<int32_t>(dataLen);
          nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, &data,
                                                             &signedLen);
          dataLen = signedLen;

          if (flavorStr.EqualsLiteral(kRTFMime)) {
            // RTF on Windows is known to sometimes deliver an extra null byte.
            if (dataLen > 0 && static_cast<char*>(data)[dataLen - 1] == '\0') {
              dataLen--;
            }
          }
        }

        nsPrimitiveHelpers::CreatePrimitiveForData(
            flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper));
        free(data);
      }

      NS_ASSERTION(genericDataWrapper,
                   "About to put null data into the transferable");
      aTransferable->SetTransferData(flavorStr.get(), genericDataWrapper);
      res = NS_OK;

      // we found one, get out of the loop
      break;
    }
  }  // foreach flavor

  return res;
}

//
// FindPlatformHTML
//
// Someone asked for the OS CF_HTML flavor. We give it back to them exactly
// as-is.
//
bool nsClipboard ::FindPlatformHTML(IDataObject* inDataObject, UINT inIndex,
                                    void** outData, uint32_t* outStartOfData,
                                    uint32_t* outDataLen) {
  // Reference: MSDN doc entitled "HTML Clipboard Format"
  // http://msdn.microsoft.com/en-us/library/aa767917(VS.85).aspx#unknown_854
  // CF_HTML is UTF8, not unicode. We also can't rely on it being
  // null-terminated so we have to check the CF_HTML header for the correct
  // length. The length we return is the bytecount from the beginning of the
  // selected data to the end of the selected data, without the null
  // termination. Because it's UTF8, we're guaranteed the header is ASCII.

  if (!outData || !*outData) {
    return false;
  }

  char version[8] = {0};
  int32_t startOfData = 0;
  int32_t endOfData = 0;
  int numFound =
      sscanf((char*)*outData, "Version:%7s\nStartHTML:%d\nEndHTML:%d", version,
             &startOfData, &endOfData);

  if (numFound != 3 || startOfData < -1 || endOfData < -1) {
    return false;
  }

  // Fixup the start and end markers if they have no context (set to -1)
  if (startOfData == -1) {
    startOfData = 0;
  }
  if (endOfData == -1) {
    endOfData = *outDataLen;
  }

  // Make sure we were passed sane values within our buffer size.
  // (Note that we've handled all cases of negative endOfData above, so we can
  // safely cast it to be unsigned here.)
  if (!endOfData || startOfData >= endOfData ||
      static_cast<uint32_t>(endOfData) > *outDataLen) {
    return false;
  }

  // We want to return the buffer not offset by startOfData because it will be
  // parsed out later (probably by HTMLEditor::ParseCFHTML) when it is still
  // in CF_HTML format.

  // We return the byte offset from the start of the data buffer to where the
  // HTML data starts. The caller might want to extract the HTML only.
  *outStartOfData = startOfData;
  *outDataLen = endOfData;
  return true;
}

//
// FindUnicodeFromPlainText
//
// we are looking for text/unicode and we failed to find it on the clipboard
// first, try again with text/plain. If that is present, convert it to unicode.
//
bool nsClipboard ::FindUnicodeFromPlainText(IDataObject* inDataObject,
                                            UINT inIndex, void** outData,
                                            uint32_t* outDataLen) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // we are looking for text/unicode and we failed to find it on the clipboard
  // first, try again with text/plain. If that is present, convert it to
  // unicode.
  nsresult rv =
      GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kTextMime),
                                nullptr, outData, outDataLen);
  if (NS_FAILED(rv) || !*outData) {
    return false;
  }

  const char* castedText = static_cast<char*>(*outData);
  nsAutoString tmp;
  rv = NS_CopyNativeToUnicode(nsDependentCSubstring(castedText, *outDataLen),
                              tmp);
  if (NS_FAILED(rv)) {
    return false;
  }

  // out with the old, in with the new
  free(*outData);
  *outData = ToNewUnicode(tmp);
  *outDataLen = tmp.Length() * sizeof(char16_t);

  return true;

}  // FindUnicodeFromPlainText

//
// FindURLFromLocalFile
//
// we are looking for a URL and couldn't find it, try again with looking for
// a local file. If we have one, it may either be a normal file or an internet
// shortcut. In both cases, however, we can get a URL (it will be a file:// url
// in the local file case).
//
bool nsClipboard ::FindURLFromLocalFile(IDataObject* inDataObject, UINT inIndex,
                                        void** outData, uint32_t* outDataLen) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug, ("%s", __FUNCTION__));

  bool dataFound = false;

  nsresult loadResult =
      GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kFileMime),
                                nullptr, outData, outDataLen);
  if (NS_SUCCEEDED(loadResult) && *outData) {
    // we have a file path in |data|. Is it an internet shortcut or a normal
    // file?
    const nsDependentString filepath(static_cast<char16_t*>(*outData));
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_NewLocalFile(filepath, true, getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      free(*outData);
      return dataFound;
    }

    if (IsInternetShortcut(filepath)) {
      free(*outData);
      nsAutoCString url;
      ResolveShortcut(file, url);
      if (!url.IsEmpty()) {
        // convert it to unicode and pass it out
        NS_ConvertUTF8toUTF16 urlString(url);
        // the internal mozilla URL format, text/x-moz-url, contains
        // URL\ntitle.  We can guess the title from the file's name.
        nsAutoString title;
        file->GetLeafName(title);
        // We rely on IsInternetShortcut check that file has a .url extension.
        title.SetLength(title.Length() - 4);
        if (title.IsEmpty()) {
          title = urlString;
        }
        *outData = ToNewUnicode(urlString + u"\n"_ns + title);
        *outDataLen =
            NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);

        dataFound = true;
      }
    } else {
      // we have a normal file, use some Necko objects to get our file path
      nsAutoCString urlSpec;
      NS_GetURLSpecFromFile(file, urlSpec);

      // convert it to unicode and pass it out
      free(*outData);
      *outData = UTF8ToNewUnicode(urlSpec);
      *outDataLen =
          NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);
      dataFound = true;
    }  // else regular file
  }

  return dataFound;
}  // FindURLFromLocalFile

//
// FindURLFromNativeURL
//
// we are looking for a URL and couldn't find it using our internal
// URL flavor, so look for it using the native URL flavor,
// CF_INETURLSTRW (We don't handle CF_INETURLSTRA currently)
//
bool nsClipboard ::FindURLFromNativeURL(IDataObject* inDataObject, UINT inIndex,
                                        void** outData, uint32_t* outDataLen) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug, ("%s", __FUNCTION__));

  bool dataFound = false;

  void* tempOutData = nullptr;
  uint32_t tempDataLen = 0;

  nsresult loadResult = GetNativeDataOffClipboard(
      inDataObject, inIndex, ::RegisterClipboardFormat(CFSTR_INETURLW), nullptr,
      &tempOutData, &tempDataLen);
  if (NS_SUCCEEDED(loadResult) && tempOutData) {
    nsDependentString urlString(static_cast<char16_t*>(tempOutData));
    // the internal mozilla URL format, text/x-moz-url, contains
    // URL\ntitle.  Since we don't actually have a title here,
    // just repeat the URL to fake it.
    *outData = ToNewUnicode(urlString + u"\n"_ns + urlString);
    *outDataLen =
        NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);
    free(tempOutData);
    dataFound = true;
  } else {
    loadResult = GetNativeDataOffClipboard(
        inDataObject, inIndex, ::RegisterClipboardFormat(CFSTR_INETURLA),
        nullptr, &tempOutData, &tempDataLen);
    if (NS_SUCCEEDED(loadResult) && tempOutData) {
      // CFSTR_INETURLA is (currently) equal to CFSTR_SHELLURL which is equal to
      // CF_TEXT which is by definition ANSI encoded.
      nsCString urlUnescapedA;
      bool unescaped =
          NS_UnescapeURL(static_cast<char*>(tempOutData), tempDataLen,
                         esc_OnlyNonASCII | esc_SkipControl, urlUnescapedA);

      nsString urlString;
      if (unescaped) {
        NS_CopyNativeToUnicode(urlUnescapedA, urlString);
      } else {
        NS_CopyNativeToUnicode(
            nsDependentCString(static_cast<char*>(tempOutData), tempDataLen),
            urlString);
      }

      // the internal mozilla URL format, text/x-moz-url, contains
      // URL\ntitle.  Since we don't actually have a title here,
      // just repeat the URL to fake it.
      *outData = ToNewUnicode(urlString + u"\n"_ns + urlString);
      *outDataLen =
          NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);
      free(tempOutData);
      dataFound = true;
    }
  }

  return dataFound;
}  // FindURLFromNativeURL

// Other apps can block access to the clipboard. This repeatedly
// calls `::OleGetClipboard` for a fixed number of times and should be called
// instead of `::OleGetClipboard`.
static HRESULT RepeatedlyTryOleGetClipboard(IDataObject** aDataObj) {
  return RepeatedlyTry(::OleGetClipboard, LogOleGetClipboardResult, aDataObj);
}

//
// ResolveShortcut
//
void nsClipboard ::ResolveShortcut(nsIFile* aFile, nsACString& outURL) {
  nsCOMPtr<nsIFileProtocolHandler> fph;
  nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fph));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  rv = fph->ReadURLFile(aFile, getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return;
  }

  uri->GetSpec(outURL);
}  // ResolveShortcut

//
// IsInternetShortcut
//
// A file is an Internet Shortcut if it ends with .URL
//
bool nsClipboard ::IsInternetShortcut(const nsAString& inFileName) {
  return StringEndsWith(inFileName, u".url"_ns,
                        nsCaseInsensitiveStringComparator);
}  // IsInternetShortcut

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  MOZ_LOG(gWin32ClipboardLog, LogLevel::Debug,
          ("%s aWhichClipboard=%i", __FUNCTION__, aWhichClipboard));

  // make sure we have a good transferable
  if (!aTransferable || aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_FAILURE;
  }

  nsresult res;

  // This makes sure we can use the OLE functionality for the clipboard
  IDataObject* dataObj;
  if (S_OK == RepeatedlyTryOleGetClipboard(&dataObj)) {
    // Use OLE IDataObject for clipboard operations
    MOZ_LOG(gWin32ClipboardLog, LogLevel::Verbose,
            ("%s: use OLE IDataObject.", __FUNCTION__));
    res = GetDataFromDataObject(dataObj, 0, nullptr, aTransferable);
    dataObj->Release();
  } else {
    // do it the old manual way
    res = GetDataFromDataObject(nullptr, 0, mWindow, aTransferable);
  }
  return res;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  // Some programs such as ZoneAlarm monitor clipboard usage and then open the
  // clipboard to scan it.  If we i) empty and then ii) set data, then the
  // 'set data' can sometimes fail with access denied becacuse another program
  // has the clipboard open.  So to avoid this race condition for OpenClipboard
  // we do not empty the clipboard when we're setting it.
  if (aWhichClipboard == kGlobalClipboard && !mEmptyingForSetData) {
    RepeatedlyTryOleSetClipboard(nullptr);
  }
  return nsBaseClipboard::EmptyClipboard(aWhichClipboard);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    bool* _retval) {
  *_retval = false;
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_OK;
  }

  for (auto& flavor : aFlavorList) {
#ifdef DEBUG
    if (flavor.EqualsLiteral(kTextMime)) {
      NS_WARNING(
          "DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode "
          "INSTEAD");
    }
#endif

    UINT format = GetFormat(flavor.get());
    if (IsClipboardFormatAvailable(format)) {
      *_retval = true;
      break;
    } else {
      // We haven't found the exact flavor the client asked for, but maybe we
      // can still find it from something else that's on the clipboard...
      if (flavor.EqualsLiteral(kUnicodeMime)) {
        // client asked for unicode and it wasn't present, check if we have
        // CF_TEXT. We'll handle the actual data substitution in the data
        // object.
        if (IsClipboardFormatAvailable(GetFormat(kTextMime))) {
          *_retval = true;
          break;
        }
      }
    }
  }

  return NS_OK;
}
