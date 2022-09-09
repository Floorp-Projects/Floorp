/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotificationHandler.h"

#include <windows.foundation.h>

#include "imgIContainer.h"
#include "imgIRequest.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Result.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/WindowsVersion.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIDUtils.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsXREDirProvider.h"
#include "WidgetUtils.h"
#include "WinUtils.h"

#include "ToastNotification.h"

namespace mozilla {
namespace widget {

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla;

// Needed to disambiguate internal and Windows `ToastNotification` classes.
typedef ABI::Windows::UI::Notifications::ToastNotification WinToastNotification;
typedef ITypedEventHandler<WinToastNotification*, IInspectable*>
    ToastActivationHandler;
typedef ITypedEventHandler<WinToastNotification*, ToastDismissedEventArgs*>
    ToastDismissedHandler;
typedef ITypedEventHandler<WinToastNotification*, ToastFailedEventArgs*>
    ToastFailedHandler;
typedef Collections::IVectorView<WinToastNotification*>
    IVectorView_ToastNotification;

NS_IMPL_ISUPPORTS(ToastNotificationHandler, nsIAlertNotificationImageListener)

static bool SetNodeValueString(const nsString& aString, IXmlNode* node,
                               IXmlDocument* xml) {
  ComPtr<IXmlText> inputText;
  HRESULT hr;
  hr = xml->CreateTextNode(HStringReference(aString.get()).Get(), &inputText);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IXmlNode> inputTextNode;
  hr = inputText.As(&inputTextNode);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IXmlNode> appendedChild;
  hr = node->AppendChild(inputTextNode.Get(), &appendedChild);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  return true;
}

static bool SetAttribute(IXmlElement* element, const HStringReference& name,
                         const nsAString& value) {
  HString valueStr;
  valueStr.Set(PromiseFlatString(value).get());

  HRESULT hr = element->SetAttribute(name.Get(), valueStr.Get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  return true;
}

static bool AddActionNode(IXmlDocument* toastXml, IXmlNode* actionsNode,
                          const nsAString& actionTitle,
                          const nsAString& launchArg,
                          const nsAString& actionArgs,
                          const nsAString& actionPlacement = u""_ns) {
  ComPtr<IXmlElement> action;
  HRESULT hr =
      toastXml->CreateElement(HStringReference(L"action").Get(), &action);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  bool success =
      SetAttribute(action.Get(), HStringReference(L"content"), actionTitle);
  NS_ENSURE_TRUE(success, false);

  // Action arguments overwrite the toast's launch arguments, so we need to
  // prepend the launch arguments necessary for the Notification Server to
  // reconstruct the toast's origin.
  //
  // Web Notification actions are arbitrary strings; to prevent breaking launch
  // argument parsing the action argument must be last. All delimiters after
  // `action` are part of the action arugment.
  nsAutoString args = launchArg + u"\naction\n"_ns + actionArgs;
  success = SetAttribute(action.Get(), HStringReference(L"arguments"), args);
  NS_ENSURE_TRUE(success, false);

  if (!actionPlacement.IsEmpty()) {
    success = SetAttribute(action.Get(), HStringReference(L"placement"),
                           actionPlacement);
    NS_ENSURE_TRUE(success, false);
  }

  // Add <action> to <actions>
  ComPtr<IXmlNode> actionNode;
  hr = action.As(&actionNode);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IXmlNode> appendedChild;
  hr = actionsNode->AppendChild(actionNode.Get(), &appendedChild);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  return true;
}

// clang - format off
/* Populate the launch argument so the COM server can reconstruct the toast
 * origin.
 *
 *   program
 *   {MOZ_APP_NAME}
 *   profile
 *   {path to profile}
 */
// clang-format on
static Result<nsString, nsresult> GetLaunchArgument() {
  nsString launchArg;

  // When the preference is false, the COM notification server will be invoked,
  // discover that there is no `program`, and exit (successfully), after which
  // Windows will invoke the in-product Windows 8-style callbacks.  When true,
  // the COM notification server will launch Firefox with sufficient arguments
  // for Firefox to handle the notification.
  if (!Preferences::GetBool(
          "alerts.useSystemBackend.windows.notificationserver.enabled",
          false)) {
    // Include dummy key/value so that newline appended arguments aren't off by
    // one line.
    launchArg += u"invalid key\ninvalid value"_ns;
    return launchArg;
  }

  // `program` argument.
  launchArg += u"program\n"_ns MOZ_APP_NAME;

  // `profile` argument
  nsCOMPtr<nsIFile> profDir;
  MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                 getter_AddRefs(profDir)));
  nsAutoString profilePath;
  MOZ_TRY(profDir->GetPath(profilePath));
  launchArg += u"\nprofile\n"_ns + profilePath;

  return launchArg;
}

static ComPtr<IToastNotificationManagerStatics>
GetToastNotificationManagerStatics() {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics;
  HRESULT hr = GetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_UI_Notifications_ToastNotificationManager)
          .Get(),
      &toastNotificationManagerStatics);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return toastNotificationManagerStatics;
}

ToastNotificationHandler::~ToastNotificationHandler() {
  if (mImageRequest) {
    mImageRequest->Cancel(NS_BINDING_ABORTED);
    mImageRequest = nullptr;
  }

  if (mHasImage && mImageFile) {
    DebugOnly<nsresult> rv = mImageFile->Remove(false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cannot remove temporary image file");
  }

  UnregisterHandler();
}

void ToastNotificationHandler::UnregisterHandler() {
  if (mNotification && mNotifier) {
    mNotification->remove_Dismissed(mDismissedToken);
    mNotification->remove_Activated(mActivatedToken);
    mNotification->remove_Failed(mFailedToken);
  }

  mNotification = nullptr;
  mNotifier = nullptr;

  SendFinished();
}

nsresult ToastNotificationHandler::InitAlertAsync(
    nsIAlertNotification* aAlert) {
  return aAlert->LoadImage(/* aTimeout = */ 0, this, /* aUserData = */ nullptr,
                           getter_AddRefs(mImageRequest));
}

ComPtr<IXmlDocument> ToastNotificationHandler::CreateToastXmlDocument() {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();

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

  ComPtr<IXmlDocument> toastXml;
  toastNotificationManagerStatics->GetTemplateContent(toastTemplate, &toastXml);

  if (!toastXml) {
    return nullptr;
  }

  nsresult ns;
  HRESULT hr;
  bool success;

  if (mHasImage) {
    ComPtr<IXmlNodeList> toastImageElements;
    hr = toastXml->GetElementsByTagName(HStringReference(L"image").Get(),
                                        &toastImageElements);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    ComPtr<IXmlNode> imageNode;
    hr = toastImageElements->Item(0, &imageNode);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    ComPtr<IXmlElement> image;
    hr = imageNode.As(&image);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    success = SetAttribute(image.Get(), HStringReference(L"src"), mImageUri);
    NS_ENSURE_TRUE(success, nullptr);
  }

  ComPtr<IXmlNodeList> toastTextElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"text").Get(),
                                      &toastTextElements);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> titleTextNodeRoot;
  hr = toastTextElements->Item(0, &titleTextNodeRoot);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> msgTextNodeRoot;
  hr = toastTextElements->Item(1, &msgTextNodeRoot);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  success = SetNodeValueString(mTitle, titleTextNodeRoot.Get(), toastXml.Get());
  NS_ENSURE_TRUE(success, nullptr);

  success = SetNodeValueString(mMsg, msgTextNodeRoot.Get(), toastXml.Get());
  NS_ENSURE_TRUE(success, nullptr);

  ComPtr<IXmlNodeList> toastElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"toast").Get(),
                                      &toastElements);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> toastNodeRoot;
  hr = toastElements->Item(0, &toastNodeRoot);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  if (mRequireInteraction) {
    ComPtr<IXmlElement> toastNode;
    hr = toastNodeRoot.As(&toastNode);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    success = SetAttribute(toastNode.Get(), HStringReference(L"scenario"),
                           u"reminder"_ns);
    NS_ENSURE_TRUE(success, nullptr);
  }
  ComPtr<IXmlElement> toastElement;
  hr = toastNodeRoot.As(&toastElement);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  auto maybeLaunchArg = GetLaunchArgument();
  NS_ENSURE_TRUE(maybeLaunchArg.isOk(), nullptr);
  nsString launchArg = maybeLaunchArg.unwrap();

  success =
      SetAttribute(toastElement.Get(), HStringReference(L"launch"), launchArg);
  NS_ENSURE_TRUE(success, nullptr);

  ComPtr<IXmlElement> actions;
  hr = toastXml->CreateElement(HStringReference(L"actions").Get(), &actions);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> actionsNode;
  hr = actions.As(&actionsNode);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  nsCOMPtr<nsIStringBundleService> sbs =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  NS_ENSURE_TRUE(sbs, nullptr);

  nsCOMPtr<nsIStringBundle> bundle;
  sbs->CreateBundle("chrome://alerts/locale/alert.properties",
                    getter_AddRefs(bundle));
  NS_ENSURE_TRUE(bundle, nullptr);

  if (!mHostPort.IsEmpty()) {
    AutoTArray<nsString, 1> formatStrings = {mHostPort};

    ComPtr<IXmlNode> urlTextNodeRoot;
    hr = toastTextElements->Item(2, &urlTextNodeRoot);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    nsAutoString urlReference;
    bundle->FormatStringFromName("source.label", formatStrings, urlReference);

    success =
        SetNodeValueString(urlReference, urlTextNodeRoot.Get(), toastXml.Get());
    NS_ENSURE_TRUE(success, nullptr);

    if (IsWin10AnniversaryUpdateOrLater()) {
      ComPtr<IXmlElement> placementText;
      hr = urlTextNodeRoot.As(&placementText);
      if (SUCCEEDED(hr)) {
        // placement is supported on Windows 10 Anniversary Update or later
        SetAttribute(placementText.Get(), HStringReference(L"placement"),
                     u"attribution"_ns);
      }
    }

    nsAutoString disableButtonTitle;
    ns = bundle->FormatStringFromName("webActions.disableForOrigin.label",
                                      formatStrings, disableButtonTitle);
    NS_ENSURE_SUCCESS(ns, nullptr);

    AddActionNode(toastXml.Get(), actionsNode.Get(), disableButtonTitle,
                  launchArg, u"snooze"_ns, u"contextmenu"_ns);
  }

  nsAutoString settingsButtonTitle;
  bundle->GetStringFromName("webActions.settings.label", settingsButtonTitle);
  success =
      AddActionNode(toastXml.Get(), actionsNode.Get(), settingsButtonTitle,
                    launchArg, u"settings"_ns, u"contextmenu"_ns);
  NS_ENSURE_TRUE(success, nullptr);

  for (const auto& action : mActions) {
    // Bug 1778596: include per-action icon from image URL.
    nsString title;
    ns = action->GetTitle(title);
    NS_ENSURE_SUCCESS(ns, nullptr);

    nsString actionString;
    ns = action->GetAction(actionString);
    NS_ENSURE_SUCCESS(ns, nullptr);

    success = AddActionNode(toastXml.Get(), actionsNode.Get(), title, launchArg,
                            actionString);
    NS_ENSURE_TRUE(success, nullptr);
  }

  ComPtr<IXmlNode> appendedChild;
  hr = toastNodeRoot->AppendChild(actionsNode.Get(), &appendedChild);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return toastXml;
}

nsresult ToastNotificationHandler::CreateToastXmlString(
    const nsAString& aImageURL, nsAString& aString) {
  HRESULT hr;

  if (!aImageURL.IsEmpty()) {
    // For testing: don't fetch and write image to disk, just include the URL.
    mHasImage = true;
    mImageUri.Assign(aImageURL);
  }

  ComPtr<IXmlDocument> toastXml = CreateToastXmlDocument();
  if (!toastXml) {
    return NS_ERROR_FAILURE;
  }

  ComPtr<IXmlNodeSerializer> ser;
  hr = toastXml.As(&ser);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  HString data;
  hr = ser->GetXml(data.GetAddressOf());
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  uint32_t len = 0;
  const wchar_t* rawData = data.GetRawBuffer(&len);
  NS_ENSURE_TRUE(rawData, NS_ERROR_FAILURE);
  aString.Assign(rawData, len);

  return NS_OK;
}

bool ToastNotificationHandler::ShowAlert() {
  if (!mBackend->IsActiveHandler(mName, this)) {
    return false;
  }

  ComPtr<IXmlDocument> toastXml = CreateToastXmlDocument();

  if (!toastXml) {
    return false;
  }

  return CreateWindowsNotificationFromXml(toastXml.Get());
}

bool ToastNotificationHandler::CreateWindowsNotificationFromXml(
    IXmlDocument* aXml) {
  ComPtr<IToastNotificationFactory> factory;
  HRESULT hr;

  hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification)
          .Get(),
      &factory);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = factory->CreateToastNotification(aXml, &mNotification);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  RefPtr<ToastNotificationHandler> self = this;

  hr = mNotification->add_Activated(
      Callback<ToastActivationHandler>([self](IToastNotification* aNotification,
                                              IInspectable* aInspectable) {
        return self->OnActivate(aNotification, aInspectable);
      }).Get(),
      &mActivatedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotification->add_Dismissed(
      Callback<ToastDismissedHandler>([self](IToastNotification* aNotification,
                                             IToastDismissedEventArgs* aArgs) {
        return self->OnDismiss(aNotification, aArgs);
      }).Get(),
      &mDismissedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotification->add_Failed(
      Callback<ToastFailedHandler>([self](IToastNotification* aNotification,
                                          IToastFailedEventArgs* aArgs) {
        return self->OnFail(aNotification, aArgs);
      }).Get(),
      &mFailedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IToastNotification2> notification2;
  hr = mNotification.As(&notification2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  // Add tag, needed to check if toast is still present in the action center
  // when we receive a dismiss timeout.
  //
  // Note that the Web Notifications API also exposes a "tag" element which can
  // be used to override a current notification. This should be emulated at the
  // DOM level as allowing webpages to overwrite Windows tags directly allows
  // pages to override each other's notifications.
  nsIDToCString uuidString(nsID::GenerateUUID());
  size_t len = strlen(uuidString.get());
  MOZ_ASSERT(len == NSID_LENGTH - 1);
  nsAutoString tag;
  CopyASCIItoUTF16(nsDependentCSubstring(uuidString.get(), len), tag);
  HString hTag;
  hTag.Set(tag.get());
  notification2->put_Tag(hTag.Get());

  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  HString aumid;
  hr = aumid.Set(mAumid.get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  hr = toastNotificationManagerStatics->CreateToastNotifierWithId(aumid.Get(),
                                                                  &mNotifier);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotifier->Show(mNotification.Get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  if (mAlertListener) {
    mAlertListener->Observe(nullptr, "alertshow", mCookie.get());
  }

  return true;
}

void ToastNotificationHandler::SendFinished() {
  if (!mSentFinished && mAlertListener) {
    mAlertListener->Observe(nullptr, "alertfinished", mCookie.get());
  }

  mSentFinished = true;
}

HRESULT
ToastNotificationHandler::OnActivate(IToastNotification* notification,
                                     IInspectable* inspectable) {
  if (mAlertListener) {
    // Extract the `action` value from the argument string.
    nsAutoString actionString;
    if (inspectable) {
      ComPtr<IToastActivatedEventArgs> eventArgs;
      HRESULT hr = inspectable->QueryInterface(
          __uuidof(IToastActivatedEventArgs), (void**)&eventArgs);
      if (SUCCEEDED(hr)) {
        HString arguments;
        hr = eventArgs->get_Arguments(arguments.GetAddressOf());
        if (SUCCEEDED(hr)) {
          uint32_t len = 0;
          const char16_t* buffer = (char16_t*)arguments.GetRawBuffer(&len);
          if (buffer) {
            // Toast arguments are a newline separated key/value combination of
            // launch arguments and an optional action argument provided as an
            // argument to the toast's constructor. After the `action` key is
            // found, the remainder of toast argument (including newlines) is
            // the `action` value.
            Tokenizer16 parse(buffer);
            nsDependentSubstring token;

            while (parse.ReadUntil(Tokenizer16::Token::NewLine(), token)) {
              if (token == u"action"_ns) {
                Unused << parse.ReadUntil(Tokenizer16::Token::EndOfFile(),
                                          actionString);
              } else {
                // Next line is a value in a key/value pair, skip.
                parse.SkipUntil(Tokenizer16::Token::NewLine());
              }
              // Skip newline.
              Tokenizer16::Token unused;
              Unused << parse.Next(unused);
            }
          }
        }
      }
    }

    if (actionString.EqualsLiteral("settings")) {
      mAlertListener->Observe(nullptr, "alertsettingscallback", mCookie.get());
    } else if (actionString.EqualsLiteral("snooze")) {
      mAlertListener->Observe(nullptr, "alertdisablecallback", mCookie.get());
    } else if (mClickable) {
      // When clicking toast, focus moves to another process, but we want to set
      // focus on Firefox process.
      nsCOMPtr<nsIWindowMediator> winMediator(
          do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
      if (winMediator) {
        nsCOMPtr<mozIDOMWindowProxy> navWin;
        winMediator->GetMostRecentWindow(u"navigator:browser",
                                         getter_AddRefs(navWin));
        if (navWin) {
          nsCOMPtr<nsIWidget> widget =
              WidgetUtils::DOMWindowToWidget(nsPIDOMWindowOuter::From(navWin));
          if (widget) {
            SetForegroundWindow(
                static_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW)));
          }
        }
      }
      mAlertListener->Observe(nullptr, "alertclickcallback", mCookie.get());
    }
  }
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

// A single toast message can receive multiple dismiss events, at most one for
// the popup and at most one for the action center. We can't simply count
// dismiss events as the user may have disabled either popups or action center
// notifications, therefore we have to check if the toast remains in the history
// (action center) to determine if the toast is fully dismissed.
static bool NotificationStillPresent(
    ComPtr<IToastNotification>& current_notification, nsString& nsAumid) {
  HRESULT hr = S_OK;

  ComPtr<IToastNotification2> current_notification2;
  hr = current_notification.As(&current_notification2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  HString current_id;
  hr = current_notification2->get_Tag(current_id.GetAddressOf());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IToastNotificationManagerStatics> manager =
      GetToastNotificationManagerStatics();
  if (!manager) {
    return false;
  }

  ComPtr<IToastNotificationManagerStatics2> manager2;
  hr = manager.As(&manager2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IToastNotificationHistory> history;
  hr = manager2->get_History(&history);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  ComPtr<IToastNotificationHistory2> history2;
  hr = history.As(&history2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  HString aumid;
  hr = aumid.Set(nsAumid.get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IVectorView_ToastNotification> toasts;
  hr = history2->GetHistoryWithId(aumid.Get(), &toasts);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  unsigned int hist_size;
  hr = toasts->get_Size(&hist_size);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  for (unsigned int i = 0; i < hist_size; i++) {
    ComPtr<IToastNotification> hist_toast;
    hr = toasts->GetAt(i, &hist_toast);
    if (NS_WARN_IF(FAILED(hr))) {
      continue;
    }

    ComPtr<IToastNotification2> hist_toast2;
    hr = hist_toast.As(&hist_toast2);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);

    HString history_id;
    hr = hist_toast2->get_Tag(history_id.GetAddressOf());
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);

    // We can not directly compare IToastNotification objects; their IUnknown
    // pointers should be equivalent but under inspection were not. Therefore we
    // use the notification's tag instead.
    if (current_id == history_id) {
      return true;
    }
  }

  return false;
}

HRESULT
ToastNotificationHandler::OnDismiss(IToastNotification* notification,
                                    IToastDismissedEventArgs* aArgs) {
  // AddRef as ComPtr doesn't own the object and shouldn't release.
  notification->AddRef();
  ComPtr<IToastNotification> comptrNotification(notification);
  // Don't dismiss notifications when they are still in the action center. We
  // can receive multiple dismiss events.
  if (NotificationStillPresent(comptrNotification, mAumid)) {
    return S_OK;
  }

  SendFinished();
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnFail(IToastNotification* notification,
                                 IToastFailedEventArgs* aArgs) {
  SendFinished();
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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mImageFile->Append(u"notificationimages"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mImageFile->Create(nsIFile::DIRECTORY_TYPE, 0500);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
    return rv;
  }

  nsID uuid;
  rv = nsID::GenerateUUIDInPlace(uuid);
  NS_ENSURE_SUCCESS(rv, rv);

  NSID_TrimBracketsASCII uuidStr(uuid);
  uuidStr.AppendLiteral(".bmp");
  mImageFile->AppendNative(uuidStr);

  nsCOMPtr<imgIContainer> imgContainer;
  rv = aRequest->GetImage(getter_AddRefs(imgContainer));
  NS_ENSURE_SUCCESS(rv, rv);

  nsMainThreadPtrHandle<ToastNotificationHandler> self(
      new nsMainThreadPtrHolder<ToastNotificationHandler>(
          "ToastNotificationHandler", this));

  nsCOMPtr<nsIFile> imageFile(mImageFile);
  RefPtr<mozilla::gfx::SourceSurface> surface = imgContainer->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
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
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString uriStr;
  rv = fileURI->GetSpec(uriStr);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendUTF8toUTF16(uriStr, mImageUri);

  mHasImage = true;

  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
