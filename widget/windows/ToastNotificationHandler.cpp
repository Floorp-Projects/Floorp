/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotificationHandler.h"

#include "imgIRequest.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/WindowsVersion.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "WinTaskbar.h"
#include "WinUtils.h"

#include "ToastNotification.h"

namespace mozilla {
namespace widget {

typedef ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::UI::Notifications::ToastNotification*, IInspectable*>
    ToastActivationHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::UI::Notifications::ToastNotification*,
    ABI::Windows::UI::Notifications::ToastDismissedEventArgs*>
    ToastDismissedHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::UI::Notifications::ToastNotification*,
    ABI::Windows::UI::Notifications::ToastFailedEventArgs*>
    ToastFailedHandler;

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla;

NS_IMPL_ISUPPORTS(ToastNotificationHandler, nsIAlertNotificationImageListener)

static bool SetNodeValueString(const nsString& aString, IXmlNode* node,
                               IXmlDocument* xml) {
  ComPtr<IXmlText> inputText;
  if (NS_WARN_IF(FAILED(xml->CreateTextNode(
          HStringReference(static_cast<const wchar_t*>(aString.get())).Get(),
          &inputText)))) {
    return false;
  }
  ComPtr<IXmlNode> inputTextNode;
  if (NS_WARN_IF(FAILED(inputText.As(&inputTextNode)))) {
    return false;
  }
  ComPtr<IXmlNode> appendedChild;
  if (NS_WARN_IF(
          FAILED(node->AppendChild(inputTextNode.Get(), &appendedChild)))) {
    return false;
  }
  return true;
}

static bool SetAttribute(IXmlElement* element, const HSTRING name,
                         const nsAString& value) {
  HSTRING valueStr = HStringReference(static_cast<const wchar_t*>(
                                          PromiseFlatString(value).get()))
                         .Get();
  if (NS_WARN_IF(FAILED(element->SetAttribute(name, valueStr)))) {
    return false;
  }
  return true;
}

static bool AddActionNode(IXmlDocument* toastXml, IXmlNode* actionsNode,
                          const nsAString& actionTitle,
                          const nsAString& actionArgs) {
  ComPtr<IXmlElement> action;
  HRESULT hr =
      toastXml->CreateElement(HStringReference(L"action").Get(), &action);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  if (NS_WARN_IF(!SetAttribute(action.Get(), HStringReference(L"content").Get(),
                               actionTitle))) {
    return false;
  }

  if (NS_WARN_IF(!SetAttribute(
          action.Get(), HStringReference(L"arguments").Get(), actionArgs))) {
    return false;
  }
  if (NS_WARN_IF(!SetAttribute(action.Get(),
                               HStringReference(L"placement").Get(),
                               NS_LITERAL_STRING("contextmenu")))) {
    return false;
  }

  // Add <action> to <actions>
  ComPtr<IXmlNode> actionNode;
  hr = action.As(&actionNode);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IXmlNode> appendedChild;
  hr = actionsNode->AppendChild(actionNode.Get(), &appendedChild);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  return true;
}

static ComPtr<IToastNotificationManagerStatics>
GetToastNotificationManagerStatics() {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics;
  if (NS_WARN_IF(FAILED(GetActivationFactory(
          HStringReference(
              RuntimeClass_Windows_UI_Notifications_ToastNotificationManager)
              .Get(),
          &toastNotificationManagerStatics)))) {
    return nullptr;
  }

  return toastNotificationManagerStatics;
}

ToastNotificationHandler::~ToastNotificationHandler() {
  if (mImageRequest) {
    mImageRequest->Cancel(NS_BINDING_ABORTED);
    mImageRequest = nullptr;
  }

  if (mHasImage) {
    DebugOnly<nsresult> rv = mImageFile->Remove(false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cannot remove temporary image file");
  }

  if (mNotification && mNotifier) {
    mNotification->remove_Dismissed(mDismissedToken);
    mNotification->remove_Activated(mActivatedToken);
    mNotification->remove_Failed(mFailedToken);
    mNotifier->Hide(mNotification.Get());
  }
}

ComPtr<IXmlDocument> ToastNotificationHandler::InitializeXmlForTemplate(
    ToastTemplateType templateType) {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();

  ComPtr<IXmlDocument> toastXml;
  toastNotificationManagerStatics->GetTemplateContent(templateType, &toastXml);

  return toastXml;
}

nsresult ToastNotificationHandler::InitAlertAsync(
    nsIAlertNotification* aAlert) {
  return aAlert->LoadImage(/* aTimeout = */ 0, this, /* aUserData = */ nullptr,
                           getter_AddRefs(mImageRequest));
}

bool ToastNotificationHandler::ShowAlert() {
  if (!mBackend->IsActiveHandler(mName, this)) {
    return true;
  }

  ToastTemplateType toastTemplate;
  if (mHostPort.IsEmpty()) {
    toastTemplate =
        mHasImage ? ToastTemplateType::ToastTemplateType_ToastImageAndText03
                  : ToastTemplateType::ToastTemplateType_ToastText03;
  } else {
    toastTemplate =
        mHasImage ? ToastTemplateType::ToastTemplateType_ToastImageAndText04
                  : ToastTemplateType::ToastTemplateType_ToastText04;
  }

  ComPtr<IXmlDocument> toastXml = InitializeXmlForTemplate(toastTemplate);
  if (!toastXml) {
    return false;
  }

  HRESULT hr;

  if (mHasImage) {
    ComPtr<IXmlNodeList> toastImageElements;
    hr = toastXml->GetElementsByTagName(HStringReference(L"image").Get(),
                                        &toastImageElements);
    if (NS_WARN_IF(FAILED(hr))) {
      return false;
    }
    ComPtr<IXmlNode> imageNode;
    hr = toastImageElements->Item(0, &imageNode);
    if (NS_WARN_IF(FAILED(hr))) {
      return false;
    }
    ComPtr<IXmlElement> image;
    hr = imageNode.As(&image);
    if (NS_WARN_IF(FAILED(hr))) {
      return false;
    }
    if (NS_WARN_IF(!SetAttribute(image.Get(), HStringReference(L"src").Get(),
                                 mImageUri))) {
      return false;
    }
  }

  ComPtr<IXmlNodeList> toastTextElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"text").Get(),
                                      &toastTextElements);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IXmlNode> titleTextNodeRoot;
  hr = toastTextElements->Item(0, &titleTextNodeRoot);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }
  ComPtr<IXmlNode> msgTextNodeRoot;
  hr = toastTextElements->Item(1, &msgTextNodeRoot);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  if (NS_WARN_IF(!SetNodeValueString(mTitle, titleTextNodeRoot.Get(),
                                     toastXml.Get()))) {
    return false;
  }
  if (NS_WARN_IF(
          !SetNodeValueString(mMsg, msgTextNodeRoot.Get(), toastXml.Get()))) {
    return false;
  }

  ComPtr<IXmlNodeList> toastElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"toast").Get(),
                                      &toastElements);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IXmlNode> toastNodeRoot;
  hr = toastElements->Item(0, &toastNodeRoot);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IXmlElement> actions;
  hr = toastXml->CreateElement(HStringReference(L"actions").Get(), &actions);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IXmlNode> actionsNode;
  hr = actions.As(&actionsNode);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  nsCOMPtr<nsIStringBundleService> sbs =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (NS_WARN_IF(!sbs)) {
    return false;
  }

  nsCOMPtr<nsIStringBundle> bundle;
  sbs->CreateBundle("chrome://alerts/locale/alert.properties",
                    getter_AddRefs(bundle));
  if (NS_WARN_IF(!bundle)) {
    return false;
  }

  if (!mHostPort.IsEmpty()) {
    const char16_t* formatStrings[] = {mHostPort.get()};

    ComPtr<IXmlNode> urlTextNodeRoot;
    hr = toastTextElements->Item(2, &urlTextNodeRoot);
    if (NS_WARN_IF(FAILED(hr))) {
      return false;
    }

    nsAutoString urlReference;
    bundle->FormatStringFromName("source.label", formatStrings,
                                 ArrayLength(formatStrings), urlReference);

    if (NS_WARN_IF(!SetNodeValueString(urlReference, urlTextNodeRoot.Get(),
                                       toastXml.Get()))) {
      return false;
    }

    if (IsWin10AnniversaryUpdateOrLater()) {
      ComPtr<IXmlElement> placementText;
      hr = urlTextNodeRoot.As(&placementText);
      if (SUCCEEDED(hr)) {
        // placement is supported on Windows 10 Anniversary Update or later
        SetAttribute(placementText.Get(), HStringReference(L"placement").Get(),
                     NS_LITERAL_STRING("attribution"));
      }
    }

    nsAutoString disableButtonTitle;
    bundle->FormatStringFromName("webActions.disableForOrigin.label",
                                 formatStrings, ArrayLength(formatStrings),
                                 disableButtonTitle);

    AddActionNode(toastXml.Get(), actionsNode.Get(), disableButtonTitle,
                  NS_LITERAL_STRING("snooze"));
  }

  nsAutoString settingsButtonTitle;
  bundle->GetStringFromName("webActions.settings.label", settingsButtonTitle);
  AddActionNode(toastXml.Get(), actionsNode.Get(), settingsButtonTitle,
                NS_LITERAL_STRING("settings"));

  ComPtr<IXmlNode> appendedChild;
  hr = toastNodeRoot->AppendChild(actionsNode.Get(), &appendedChild);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  return CreateWindowsNotificationFromXml(toastXml.Get());
}

bool ToastNotificationHandler::CreateWindowsNotificationFromXml(
    IXmlDocument* aXml) {
  ComPtr<IToastNotificationFactory> factory;
  HRESULT hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification)
          .Get(),
      &factory);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = factory->CreateToastNotification(aXml, &mNotification);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  RefPtr<ToastNotificationHandler> self = this;

  hr = mNotification->add_Activated(
      Callback<ToastActivationHandler>([self](IToastNotification* aNotification,
                                              IInspectable* aInspectable) {
        return self->OnActivate(aNotification, aInspectable);
      }).Get(),
      &mActivatedToken);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = mNotification->add_Dismissed(
      Callback<ToastDismissedHandler>([self](IToastNotification* aNotification,
                                             IToastDismissedEventArgs* aArgs) {
        return self->OnDismiss(aNotification, aArgs);
      }).Get(),
      &mDismissedToken);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = mNotification->add_Failed(
      Callback<ToastFailedHandler>([self](IToastNotification* aNotification,
                                          IToastFailedEventArgs* aArgs) {
        return self->OnFail(aNotification, aArgs);
      }).Get(),
      &mFailedToken);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();
  if (NS_WARN_IF(!toastNotificationManagerStatics)) {
    return false;
  }

  nsAutoString uid;
  if (NS_WARN_IF(!WinTaskbar::GetAppUserModelID(uid))) {
    return false;
  }

  HSTRING uidStr =
      HStringReference(static_cast<const wchar_t*>(uid.get())).Get();
  hr = toastNotificationManagerStatics->CreateToastNotifierWithId(uidStr,
                                                                  &mNotifier);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = mNotifier->Show(mNotification.Get());
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  if (mAlertListener) {
    mAlertListener->Observe(nullptr, "alertshow", mCookie.get());
  }

  return true;
}

HRESULT
ToastNotificationHandler::OnActivate(IToastNotification* notification,
                                     IInspectable* inspectable) {
  if (mAlertListener) {
    nsAutoString argString;
    if (inspectable) {
      ComPtr<IToastActivatedEventArgs> eventArgs;
      HRESULT hr = inspectable->QueryInterface(
          __uuidof(IToastActivatedEventArgs), (void**)&eventArgs);
      if (SUCCEEDED(hr)) {
        HSTRING arguments;
        hr = eventArgs->get_Arguments(&arguments);
        if (SUCCEEDED(hr)) {
          uint32_t len = 0;
          const wchar_t* buffer = WindowsGetStringRawBuffer(arguments, &len);
          if (buffer) {
            argString.Assign(buffer, len);
          }
        }
      }
    }

    if (argString.EqualsLiteral("settings")) {
      mAlertListener->Observe(nullptr, "alertsettingscallback", mCookie.get());
    } else if (argString.EqualsLiteral("snooze")) {
      mAlertListener->Observe(nullptr, "alertdisablecallback", mCookie.get());
    } else if (mClickable) {
      mAlertListener->Observe(nullptr, "alertclickcallback", mCookie.get());
    }
  }
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnDismiss(IToastNotification* notification,
                                    IToastDismissedEventArgs* aArgs) {
  if (mAlertListener) {
    mAlertListener->Observe(nullptr, "alertfinished", mCookie.get());
  }
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnFail(IToastNotification* notification,
                                 IToastFailedEventArgs* aArgs) {
  if (mAlertListener) {
    mAlertListener->Observe(nullptr, "alertfinished", mCookie.get());
  }
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

nsresult ToastNotificationHandler::TryShowAlert() {
  if (NS_WARN_IF(!ShowAlert())) {
    mBackend->RemoveHandler(mName, this);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}
NS_IMETHODIMP
ToastNotificationHandler::OnImageMissing(nsISupports*) {
  return TryShowAlert();
}

NS_IMETHODIMP
ToastNotificationHandler::OnImageReady(nsISupports*, imgIRequest* aRequest) {
  nsresult rv = AsyncSaveImage(aRequest);
  if (NS_FAILED(rv)) {
    return TryShowAlert();
  }
  return rv;
}

nsresult ToastNotificationHandler::AsyncSaveImage(imgIRequest* aRequest) {
  nsresult rv =
      NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mImageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mImageFile->Append(NS_LITERAL_STRING("notificationimages"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mImageFile->Create(nsIFile::DIRECTORY_TYPE, 0500);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
    return rv;
  }

  nsCOMPtr<nsIUUIDGenerator> idGen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsID uuid;
  rv = idGen->GenerateUUIDInPlace(&uuid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  char uuidChars[NSID_LENGTH];
  uuid.ToProvidedString(uuidChars);
  // Remove the brackets at the beginning and ending of the generated UUID.
  nsAutoCString uuidStr(Substring(uuidChars + 1, uuidChars + NSID_LENGTH - 2));
  uuidStr.AppendLiteral(".bmp");
  mImageFile->AppendNative(uuidStr);

  nsCOMPtr<imgIContainer> imgContainer;
  rv = aRequest->GetImage(getter_AddRefs(imgContainer));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsMainThreadPtrHandle<ToastNotificationHandler> self(
      new nsMainThreadPtrHolder<ToastNotificationHandler>(
          "ToastNotificationHandler", this));

  nsCOMPtr<nsIFile> imageFile(mImageFile);
  RefPtr<mozilla::gfx::SourceSurface> surface = imgContainer->GetFrame(
      imgIContainer::FRAME_FIRST, imgIContainer::FLAG_SYNC_DECODE);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "ToastNotificationHandler::AsyncWriteBitmap",
      [self, imageFile, surface]() -> void {
        nsresult rv;
        if (!surface) {
          rv = NS_ERROR_FAILURE;
        } else {
          rv = WinUtils::WriteBitmap(imageFile, surface);
        }

        nsCOMPtr<nsIRunnable> cbRunnable = NS_NewRunnableFunction(
            "ToastNotificationHandler::AsyncWriteBitmapCb",
            [self, rv]() -> void {
              auto handler = const_cast<ToastNotificationHandler*>(self.get());
              handler->OnWriteBitmapFinished(rv);
            });

        NS_DispatchToMainThread(cbRunnable);
      });

  return mBackend->BackgroundDispatch(r);
}

void ToastNotificationHandler::OnWriteBitmapFinished(nsresult rv) {
  if (NS_SUCCEEDED(rv)) {
    OnWriteBitmapSuccess();
  }
  TryShowAlert();
}

nsresult ToastNotificationHandler::OnWriteBitmapSuccess() {
  nsresult rv;

  nsCOMPtr<nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), mImageFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString uriStr;
  rv = fileURI->GetSpec(uriStr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AppendUTF8toUTF16(uriStr, mImageUri);

  mHasImage = true;

  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
