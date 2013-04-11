/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameworkView.h"
#include "MetroUtils.h"
#include "nsICommandLineRunner.h"
#include "nsNetUtil.h"
#include "nsIDOMChromeWindow.h"
#include "nsIURI.h"
#include "nsPrintfCString.h"
#include "mozilla/Services.h"
#include <wrl/wrappers/corewrappers.h>
#include <shellapi.h>
#include <DXGIFormat.h>
#include <d2d1_1.h>
#include <printpreview.h>
#include <D3D10.h>
#include "MetroUIUtils.h"
#include "nsIStringBundle.h"

using namespace mozilla;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

// Play to contract
using namespace ABI::Windows::Media::PlayTo;

// Activation contracts
using namespace ABI::Windows::ApplicationModel::Activation;
using namespace ABI::Windows::ApplicationModel::DataTransfer;
using namespace ABI::Windows::ApplicationModel::Search;

// Settings contract
using namespace ABI::Windows::UI::ApplicationSettings;
using namespace ABI::Windows::UI::Popups;

// Print contract
using namespace ABI::Windows::Graphics::Printing;

namespace mozilla {
namespace widget {
namespace winrt {

extern nsTArray<nsString>* sSettingsArray;

void
FrameworkView::SearchActivated(ComPtr<ISearchActivatedEventArgs>& aArgs, bool aStartup)
{
  if (!aArgs)
    return;

  HString data;
  AssertHRESULT(aArgs->get_QueryText(data.GetAddressOf()));
  if (WindowsIsStringEmpty(data.Get()))
    return;

  unsigned int length;
  LogW(L"SearchActivated text=%s", data.GetRawBuffer(&length));
  if (aStartup) {
    mActivationURI = data.GetRawBuffer(&length);
  } else {
    PerformURILoadOrSearch(data);
  }
}

void
FrameworkView::FileActivated(ComPtr<IFileActivatedEventArgs>& aArgs, bool aStartup)
{
  if (!aArgs)
    return;

  ComPtr<IVectorView<ABI::Windows::Storage::IStorageItem*>> list;
  AssertHRESULT(aArgs->get_Files(list.GetAddressOf()));
  ComPtr<ABI::Windows::Storage::IStorageItem> item;
  AssertHRESULT(list->GetAt(0, item.GetAddressOf()));
  HString filePath;
  AssertHRESULT(item->get_Path(filePath.GetAddressOf()));

  if (aStartup) {
    unsigned int length;
    mActivationURI = filePath.GetRawBuffer(&length);
  } else {
    PerformURILoad(filePath);
  }
}

void
FrameworkView::LaunchActivated(ComPtr<ILaunchActivatedEventArgs>& aArgs, bool aStartup)
{
  if (!aArgs)
    return;
  HString data;
  AssertHRESULT(aArgs->get_Arguments(data.GetAddressOf()));
  if (WindowsIsStringEmpty(data.Get()))
    return;

  // If we're being launched from a secondary tile then we have a 2nd command line param of -url
  // and a third of the secondary tile.  We want it in mActivationURI so that browser.js will
  // load it in without showing the start UI.
  int argc;
  unsigned int length;
  LPWSTR* argv = CommandLineToArgvW(data.GetRawBuffer(&length), &argc);
  if (aStartup && argc == 3 && !wcsicmp(argv[1], L"-url")) {
    mActivationURI = argv[2];
  } else {
    // Some other command line or this is not a startup.
    // If it is startup we process it later when XPCOM is initialilzed.
    mActivationCommandLine = data.GetRawBuffer(&length);
    if (!aStartup) {
      ProcessLaunchArguments();
    }
  }
}

void
FrameworkView::ProcessLaunchArguments()
{
  if (!mActivationCommandLine.Length())
    return;

  int argc;
  LPWSTR* argv = CommandLineToArgvW(mActivationCommandLine.BeginReading(), &argc);
  nsCOMPtr<nsICommandLineRunner> cmdLine =
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
  if (!cmdLine) {
    NS_WARNING("Unable to instantiate command line runner.");
    return;
  }

  LPSTR *argvUTF8 = new LPSTR[argc];
  for (int i = 0; i < argc; ++i) {
    NS_ConvertUTF16toUTF8 arg(argv[i]);
    argvUTF8[i] = new char[arg.Length() + 1];
    strcpy(argvUTF8[i], const_cast<char *>(arg.BeginReading()));
    LogW(L"Launch arg[%d]: '%s'", i, argv[i]);
  }

  nsresult rv = cmdLine->Init(argc,
                              argvUTF8,
                              nullptr,
                              nsICommandLine::STATE_REMOTE_EXPLICIT);
  if (NS_SUCCEEDED(rv)) {
    cmdLine->Run();
  } else {
    NS_WARNING("cmdLine->Init failed.");
  }

  for (int i = 0; i < argc; ++i) {
    delete[] argvUTF8[i];
  }
  delete[] argvUTF8;
}

void
FrameworkView::ProcessActivationArgs(IActivatedEventArgs* aArgs, bool aStartup)
{
  ActivationKind kind;
  if (!aArgs || FAILED(aArgs->get_Kind(&kind)))
    return;
  ComPtr<IActivatedEventArgs> args(aArgs);
  if (kind == ActivationKind::ActivationKind_Protocol) {
    Log("Activation argument kind: Protocol");
    ComPtr<IProtocolActivatedEventArgs> protoArgs;
    AssertHRESULT(args.As(&protoArgs));
    ComPtr<IUriRuntimeClass> uri;
    AssertHRESULT(protoArgs->get_Uri(uri.GetAddressOf()));
    if (!uri)
      return;

    HString data;
    AssertHRESULT(uri->get_AbsoluteUri(data.GetAddressOf()));
    if (WindowsIsStringEmpty(data.Get()))
      return;

    if (aStartup) {
      unsigned int length;
      mActivationURI = data.GetRawBuffer(&length);
    } else {
      PerformURILoad(data);
    }
  } else if (kind == ActivationKind::ActivationKind_Search) {
    Log("Activation argument kind: Search");
    ComPtr<ISearchActivatedEventArgs> searchArgs;
    args.As(&searchArgs);
    SearchActivated(searchArgs, aStartup);
  } else if (kind == ActivationKind::ActivationKind_File) {
    Log("Activation argument kind: File");
    ComPtr<IFileActivatedEventArgs> fileArgs;
    args.As(&fileArgs);
    FileActivated(fileArgs, aStartup);
  } else if (kind == ActivationKind::ActivationKind_Launch) {
    Log("Activation argument kind: Launch");
    ComPtr<ILaunchActivatedEventArgs> launchArgs;
    args.As(&launchArgs);
    LaunchActivated(launchArgs, aStartup);
  }
}

void
FrameworkView::SetupContracts()
{
  LogFunction();
  HRESULT hr;

  // Add support for the share charm to indicate that we share data to other apps
  ComPtr<IDataTransferManagerStatics> transStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager).Get(),
                            transStatics.GetAddressOf());
  AssertHRESULT(hr);
  ComPtr<IDataTransferManager> trans;
  AssertHRESULT(transStatics->GetForCurrentView(trans.GetAddressOf()));
  trans->add_DataRequested(Callback<__FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CDataRequestedEventArgs_t>(
    this, &FrameworkView::OnDataShareRequested).Get(), &mDataTransferRequested);

  // Add support for the search charm to indicate that you can search using our app.
  ComPtr<ISearchPaneStatics> searchStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_ApplicationModel_Search_SearchPane).Get(),
                            searchStatics.GetAddressOf());
  AssertHRESULT(hr);
  ComPtr<ISearchPane> searchPane;
  AssertHRESULT(searchStatics->GetForCurrentView(searchPane.GetAddressOf()));
  searchPane->add_QuerySubmitted(Callback<__FITypedEventHandler_2_Windows__CApplicationModel__CSearch__CSearchPane_Windows__CApplicationModel__CSearch__CSearchPaneQuerySubmittedEventArgs_t>(
    this, &FrameworkView::OnSearchQuerySubmitted).Get(), &mSearchQuerySubmitted);

  // Add support for the devices play to charm
  ComPtr<IPlayToManagerStatics> playToStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Media_PlayTo_PlayToManager).Get(),
                            playToStatics.GetAddressOf());
  AssertHRESULT(hr);
  ComPtr<IPlayToManager> playTo;
  AssertHRESULT(playToStatics->GetForCurrentView(playTo.GetAddressOf()));
  playTo->add_SourceRequested(Callback<__FITypedEventHandler_2_Windows__CMedia__CPlayTo__CPlayToManager_Windows__CMedia__CPlayTo__CPlayToSourceRequestedEventArgs_t>(
    this, &FrameworkView::OnPlayToSourceRequested).Get(), &mPlayToRequested);

  // Add support for the settings charm
  ComPtr<ISettingsPaneStatics> settingsPaneStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ApplicationSettings_SettingsPane).Get(),
                            settingsPaneStatics.GetAddressOf());
  AssertHRESULT(hr);
  ComPtr<ISettingsPane> settingsPane;
  AssertHRESULT(settingsPaneStatics->GetForCurrentView(settingsPane.GetAddressOf()));
  settingsPane->add_CommandsRequested(Callback<__FITypedEventHandler_2_Windows__CUI__CApplicationSettings__CSettingsPane_Windows__CUI__CApplicationSettings__CSettingsPaneCommandsRequestedEventArgs_t>(
    this, &FrameworkView::OnSettingsCommandsRequested).Get(), &mSettingsPane);

  // Add support for the settings print charm
  ComPtr<IPrintManagerStatic> printStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Printing_PrintManager).Get(),
                            printStatics.GetAddressOf());
  AssertHRESULT(hr);
  ComPtr<IPrintManager> printManager;
  AssertHRESULT(printStatics->GetForCurrentView(printManager.GetAddressOf()));
  printManager->add_PrintTaskRequested(Callback<__FITypedEventHandler_2_Windows__CGraphics__CPrinting__CPrintManager_Windows__CGraphics__CPrinting__CPrintTaskRequestedEventArgs_t>(
    this, &FrameworkView::OnPrintTaskRequested).Get(), &mPrintManager);
}

void
FrameworkView::PerformURILoad(HString& aURI)
{
  LogFunction();

  unsigned int length;
  LogW(L"PerformURILoad uri=%s", aURI.GetRawBuffer(&length));

  nsCOMPtr<nsICommandLineRunner> cmdLine =
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
  if (!cmdLine) {
    NS_WARNING("Unable to instantiate command line runner.");
    return;
  }

  nsAutoCString utf8data(NS_ConvertUTF16toUTF8(aURI.GetRawBuffer(&length)));
  const char *argv[] = { "metrobrowser",
                         "-url",
                         utf8data.BeginReading() };
  nsresult rv = cmdLine->Init(ArrayLength(argv),
                              const_cast<char **>(argv), nullptr,
                              nsICommandLine::STATE_REMOTE_EXPLICIT);
  if (NS_FAILED(rv)) {
    NS_WARNING("cmdLine->Init failed.");
    return;
  }
  cmdLine->Run();
}

void
FrameworkView::PerformSearch(HString& aQuery)
{
  LogFunction();

  nsCOMPtr<nsICommandLineRunner> cmdLine =
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
  if (!cmdLine) {
    NS_WARNING("Unable to instantiate command line runner.");
    return;
  }

  nsAutoCString parameter;
  parameter.AppendLiteral("\"");
  unsigned int length;
  parameter.Append(NS_ConvertUTF16toUTF8(aQuery.GetRawBuffer(&length)));
  parameter.AppendLiteral("\"");

  const char *argv[] = { "metrobrowser",
                         "-search",
                         parameter.BeginReading() };
  nsresult rv = cmdLine->Init(ArrayLength(argv),
                              const_cast<char **>(argv), nullptr,
                              nsICommandLine::STATE_REMOTE_EXPLICIT);
  if (NS_FAILED(rv)) {
    NS_WARNING("cmdLine->Init failed.");
    return;
  }
  cmdLine->Run();
}

void
FrameworkView::PerformURILoadOrSearch(HString& aString)
{
  LogFunction();

  if (WindowsIsStringEmpty(aString.Get())) {
    Log("Emptry string passed to PerformURILoadOrSearch");
    return;
  }

  // If we have a URI then devert to load the URI directly
  ComPtr<IUriRuntimeClass> uri;
  MetroUtils::CreateUri(aString.Get(), uri);
  if (uri) {
    PerformURILoad(aString);
  } else {
    PerformSearch(aString);
  }
}

HRESULT
FrameworkView::OnDataShareRequested(IDataTransferManager* aDTM,
                                    IDataRequestedEventArgs* aArg)
{
  // Only share pages that contain a title and a URI
  nsCOMPtr<nsIMetroUIUtils> metroUIUtils = do_CreateInstance("@mozilla.org/metro-ui-utils;1");
  if (!metroUIUtils)
      return E_FAIL;

  nsString url, title;
  nsresult rv = metroUIUtils->GetCurrentPageURI(url);
  nsresult rv2 = metroUIUtils->GetCurrentPageTitle(title);
  if (NS_FAILED(rv) || NS_FAILED(rv2)) {
    return E_FAIL;
  }

  // Get the package to share
  HRESULT hr;
  ComPtr<IDataRequest> request;
  AssertRetHRESULT(hr = aArg->get_Request(request.GetAddressOf()), hr);
  ComPtr<IDataPackage> dataPackage;
  AssertRetHRESULT(hr = request->get_Data(dataPackage.GetAddressOf()), hr);
  ComPtr<IDataPackagePropertySet> props;
  AssertRetHRESULT(hr = dataPackage->get_Properties(props.GetAddressOf()), hr);

  // Only add a URI to the package when there is no selected content.
  // This is because most programs treat URIs as highest priority to generate
  // their own preview, but we only want the selected content to show up.
  bool hasSelectedContent = false;
  metroUIUtils->GetHasSelectedContent(&hasSelectedContent);
  if (!hasSelectedContent) {
    ComPtr<IUriRuntimeClass> uri;
    AssertRetHRESULT(hr = MetroUtils::CreateUri(HStringReference(url.BeginReading()).Get(), uri), hr);

    // If there is no selection, then we don't support sharing for sites that
    // are not HTTP, HTTPS, FTP, and FILE.
    HString schemeHString;
    uri->get_SchemeName(schemeHString.GetAddressOf());
    unsigned int length;
    LPCWSTR scheme = schemeHString.GetRawBuffer(&length);
    if (!scheme || wcscmp(scheme, L"http") && wcscmp(scheme, L"https") &&
        wcscmp(scheme, L"ftp") && wcscmp(scheme, L"file")) {
      return S_OK;
    }

    AssertRetHRESULT(hr = dataPackage->SetUri(uri.Get()), hr);
  }

  // Add whatever content metroUIUtils wants us to for the text sharing
  nsString shareText;
  if (NS_SUCCEEDED(metroUIUtils->GetShareText(shareText)) && shareText.Length()) {
    AssertRetHRESULT(hr = dataPackage->SetText(HStringReference(shareText.BeginReading()).Get()) ,hr);
  }

  // Add whatever content metroUIUtils wants us to for the HTML sharing
  nsString shareHTML;
  if (NS_SUCCEEDED(metroUIUtils->GetShareHTML(shareHTML)) && shareHTML.Length()) {
    // The sharing format needs some special headers, so pass it through Windows
    ComPtr<IHtmlFormatHelperStatics> htmlFormatHelper;
    hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_ApplicationModel_DataTransfer_HtmlFormatHelper).Get(),
                              htmlFormatHelper.GetAddressOf());
    AssertRetHRESULT(hr, hr);
    HSTRING fixedHTML;
    htmlFormatHelper->CreateHtmlFormat(HStringReference(shareHTML.BeginReading()).Get(), &fixedHTML);

    // And now add the fixed HTML to the data package
    AssertRetHRESULT(hr = dataPackage->SetHtmlFormat(fixedHTML), hr);
  }

  // Obtain the brand name
  nsCOMPtr<nsIStringBundleService> bundleService = 
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  NS_ENSURE_TRUE(bundleService, E_FAIL);
  nsCOMPtr<nsIStringBundle> brandBundle;
  nsString brandName;
  bundleService->CreateBundle("chrome://branding/locale/brand.properties",
    getter_AddRefs(brandBundle));
  NS_ENSURE_TRUE(brandBundle, E_FAIL);
  if(brandBundle) {
    brandBundle->GetStringFromName(NS_LITERAL_STRING("brandFullName").get(),
                                   getter_Copies(brandName));
  }

  // Set these properties at the end.  Otherwise users can get a
  // "There was a problem with the data package" error when there
  // is simply nothing to share.
  props->put_ApplicationName(HStringReference(brandName.BeginReading()).Get());
  if (title.Length()) {
    props->put_Title(HStringReference(title.BeginReading()).Get());
  } else {
    props->put_Title(HStringReference(brandName.BeginReading()).Get());
  }
  props->put_Description(HStringReference(url.BeginReading()).Get());

  return S_OK;
}

HRESULT
FrameworkView::OnSearchQuerySubmitted(ISearchPane* aPane,
                                      ISearchPaneQuerySubmittedEventArgs* aArgs)
{
  LogFunction();
  HString aQuery;
  aArgs->get_QueryText(aQuery.GetAddressOf());
  PerformURILoadOrSearch(aQuery);
  return S_OK;
}

HRESULT
FrameworkView::OnSettingsCommandInvoked(IUICommand* aCommand)
{
  LogFunction();
  HRESULT hr;
  uint32_t id;
  ComPtr<IPropertyValue> prop;
  AssertRetHRESULT(hr = aCommand->get_Id((IInspectable**)prop.GetAddressOf()), hr);
  AssertRetHRESULT(hr = prop->GetUInt32(&id), hr);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    NS_ConvertASCIItoUTF16 idStr(nsPrintfCString("%lu", id));
    obs->NotifyObservers(nullptr, "metro-settings-entry-selected", idStr.BeginReading());
  }

  return S_OK;
}

void
FrameworkView::AddSetting(ISettingsPaneCommandsRequestedEventArgs* aArgs,
                          uint32_t aId, HString& aSettingName)
{
  HRESULT hr;

  ComPtr<ABI::Windows::UI::ApplicationSettings::ISettingsPaneCommandsRequest> request;
  AssertHRESULT(aArgs->get_Request(request.GetAddressOf()));

  // ApplicationCommands - vector that holds SettingsCommand to be invoked
  ComPtr<IVector<ABI::Windows::UI::ApplicationSettings::SettingsCommand*>> list;
  AssertHRESULT(request->get_ApplicationCommands(list.GetAddressOf()));

  ComPtr<IUICommand> command;
  ComPtr<ISettingsCommandFactory> factory;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ApplicationSettings_SettingsCommand).Get(),
                            factory.GetAddressOf());
  AssertHRESULT(hr);

  // Create the IInspectable string property that identifies this command
  ComPtr<IInspectable> prop;
  ComPtr<IPropertyValueStatics> propStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(),
                            propStatics.GetAddressOf());
  AssertHRESULT(hr);
  hr = propStatics->CreateUInt32(aId, prop.GetAddressOf());
  AssertHRESULT(hr);

  // Create the command
  hr = factory->CreateSettingsCommand(prop.Get(), aSettingName.Get(),
    Callback<ABI::Windows::UI::Popups::IUICommandInvokedHandler>(
      this, &FrameworkView::OnSettingsCommandInvoked).Get(), command.GetAddressOf());
  AssertHRESULT(hr);

  // Add it to the list
  hr = list->Append(command.Get());
  AssertHRESULT(hr);
}

HRESULT
FrameworkView::OnSettingsCommandsRequested(ISettingsPane* aPane,
                                           ISettingsPaneCommandsRequestedEventArgs* aArgs)
{
  if (!sSettingsArray)
    return E_FAIL;
  if (!sSettingsArray->Length())
    return S_OK;
  for (uint32_t i = 0; i < sSettingsArray->Length(); i++) {
    HString label;
    label.Set(sSettingsArray->ElementAt(i).BeginReading());
    AddSetting(aArgs, i, label);
  }
  return S_OK;
}

HRESULT
FrameworkView::OnPlayToSourceRequested(IPlayToManager* aPlayToManager,
                                       IPlayToSourceRequestedEventArgs* aArgs)
{
  // TODO: Implement PlayTo, find the element on the page and then do something similar to this:
  // PlayToReceiver::Dispatcher.Helper.BeginInvoke(
  // mMediaElement = ref new Windows::UI::Xaml::Controls::MediaElement();
  // mMediaElement->Source = ref new Uri("http://www.youtube.com/watch?v=2U0NFgoNI7s");
  // aArgs->SourceRequest->SetSource(mMediaElement->PlayToSource);
 return S_OK;
}

HRESULT
FrameworkView::OnPrintTaskSourceRequested(IPrintTaskSourceRequestedArgs* aArgs)
{
 return S_OK;
}

HRESULT
FrameworkView::OnPrintTaskRequested(IPrintManager* aPrintManager,
                                    IPrintTaskRequestedEventArgs* aArgs)
{
 return S_OK;
}

void
FrameworkView::CreatePrintControl(IPrintDocumentPackageTarget* docPackageTarget,
                                  D2D1_PRINT_CONTROL_PROPERTIES* printControlProperties)
{
}

HRESULT
FrameworkView::ClosePrintControl()
{
  return S_OK;
}

// Print out one page, with the given print ticket.
// This sample has only one page and we ignore pageNumber below.
void FrameworkView::PrintPage(uint32_t pageNumber,
                              D2D1_RECT_F imageableArea,
                              D2D1_SIZE_F pageSize,
                              IStream* pagePrintTicketStream)
{
}

} } }
