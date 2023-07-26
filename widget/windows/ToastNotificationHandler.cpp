/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotificationHandler.h"

#include <windows.foundation.h>

#include "gfxUtils.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "mozilla/gfx/2D.h"
#ifdef MOZ_BACKGROUNDTASKS
#  include "mozilla/BackgroundTasks.h"
#endif
#include "mozilla/HashFunctions.h"
#include "mozilla/JSONStringWriteFuncs.h"
#include "mozilla/Result.h"
#include "mozilla/Logging.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/Unused.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/intl/Localization.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIDUtils.h"
#include "nsIStringBundle.h"
#include "nsIToolkitProfile.h"
#include "nsIToolkitProfileService.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsXREDirProvider.h"
#include "ToastNotificationHeaderOnlyUtils.h"
#include "WidgetUtils.h"
#include "WinUtils.h"

#include "ToastNotification.h"

namespace mozilla {
namespace widget {

extern LazyLogModule sWASLog;

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace toastnotification;

// Needed to disambiguate internal and Windows `ToastNotification` classes.
using WinToastNotification = ABI::Windows::UI::Notifications::ToastNotification;
using ToastActivationHandler =
    ITypedEventHandler<WinToastNotification*, IInspectable*>;
using ToastDismissedHandler =
    ITypedEventHandler<WinToastNotification*, ToastDismissedEventArgs*>;
using ToastFailedHandler =
    ITypedEventHandler<WinToastNotification*, ToastFailedEventArgs*>;
using IVectorView_ToastNotification =
    Collections::IVectorView<WinToastNotification*>;

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

static bool SetAttribute(ComPtr<IXmlElement>& element,
                         const HStringReference& name, const nsAString& value) {
  HString valueStr;
  valueStr.Set(PromiseFlatString(value).get());

  HRESULT hr = element->SetAttribute(name.Get(), valueStr.Get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  return true;
}

static bool AddActionNode(ComPtr<IXmlDocument>& toastXml,
                          ComPtr<IXmlNode>& actionsNode,
                          const nsAString& actionTitle,
                          const nsAString& launchArg,
                          const nsAString& actionArgs,
                          const nsAString& actionPlacement = u""_ns,
                          const nsAString& activationType = u""_ns) {
  ComPtr<IXmlElement> action;
  HRESULT hr =
      toastXml->CreateElement(HStringReference(L"action").Get(), &action);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  bool success =
      SetAttribute(action, HStringReference(L"content"), actionTitle);
  NS_ENSURE_TRUE(success, false);

  // Action arguments overwrite the toast's launch arguments, so we need to
  // prepend the launch arguments necessary for the Notification Server to
  // reconstruct the toast's origin.
  //
  // Web Notification actions are arbitrary strings; to prevent breaking launch
  // argument parsing the action argument must be last. All delimiters after
  // `action` are part of the action arugment.
  nsAutoString args = launchArg + u"\n"_ns +
                      nsDependentString(kLaunchArgAction) + u"\n"_ns +
                      actionArgs;
  success = SetAttribute(action, HStringReference(L"arguments"), args);
  NS_ENSURE_TRUE(success, false);

  if (!actionPlacement.IsEmpty()) {
    success =
        SetAttribute(action, HStringReference(L"placement"), actionPlacement);
    NS_ENSURE_TRUE(success, false);
  }

  if (!activationType.IsEmpty()) {
    success = SetAttribute(action, HStringReference(L"activationType"),
                           activationType);
    NS_ENSURE_TRUE(success, false);

    // No special argument handling: when `activationType="system"`, `action` is
    // a Windows-specific keyword, generally "dismiss" or "snooze".
    success = SetAttribute(action, HStringReference(L"arguments"), actionArgs);
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

nsresult ToastNotificationHandler::GetWindowsTag(nsAString& aWindowsTag) {
  aWindowsTag.Assign(mWindowsTag);
  return NS_OK;
}

nsresult ToastNotificationHandler::SetWindowsTag(const nsAString& aWindowsTag) {
  mWindowsTag.Assign(aWindowsTag);
  return NS_OK;
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
Result<nsString, nsresult> ToastNotificationHandler::GetLaunchArgument() {
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
  launchArg += nsDependentString(kLaunchArgProgram) + u"\n"_ns MOZ_APP_NAME;

  // `profile` argument.
  nsCOMPtr<nsIFile> profDir;
  bool wantCurrentProfile = true;
#ifdef MOZ_BACKGROUNDTASKS
  if (BackgroundTasks::IsBackgroundTaskMode()) {
    // Notifications popped from a background task want to invoke Firefox with a
    // different profile -- the default browsing profile.  We'd prefer to not
    // specify a profile, so that the Firefox invoked by the notification server
    // chooses its default profile, but this might pop the profile chooser in
    // some configurations.
    wantCurrentProfile = false;

    nsCOMPtr<nsIToolkitProfileService> profileSvc =
        do_GetService(NS_PROFILESERVICE_CONTRACTID);
    if (profileSvc) {
      nsCOMPtr<nsIToolkitProfile> defaultProfile;
      nsresult rv =
          profileSvc->GetDefaultProfile(getter_AddRefs(defaultProfile));
      if (NS_SUCCEEDED(rv) && defaultProfile) {
        // Not all installations have a default profile.  But if one is set,
        // then it should have a profile directory.
        MOZ_TRY(defaultProfile->GetRootDir(getter_AddRefs(profDir)));
      }
    }
  }
#endif
  if (wantCurrentProfile) {
    MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                   getter_AddRefs(profDir)));
  }

  if (profDir) {
    nsAutoString profilePath;
    MOZ_TRY(profDir->GetPath(profilePath));
    launchArg += u"\n"_ns + nsDependentString(kLaunchArgProfile) + u"\n"_ns +
                 profilePath;
  }

  // `windowsTag` argument.
  launchArg +=
      u"\n"_ns + nsDependentString(kLaunchArgTag) + u"\n"_ns + mWindowsTag;

  // `logging` argument.
  if (Preferences::GetBool(
          "alerts.useSystemBackend.windows.notificationserver.verbose",
          false)) {
    // Signal notification to log verbose messages.
    launchArg +=
        u"\n"_ns + nsDependentString(kLaunchArgLogging) + u"\nverbose"_ns;
  }

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
  if (mNotification) {
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
  MOZ_TRY(InitWindowsTag());

  return aAlert->LoadImage(/* aTimeout = */ 0, this, /* aUserData = */ nullptr,
                           getter_AddRefs(mImageRequest));
}

// Uniquely identify this toast to Windows.  Existing names and cookies are not
// suitable: we want something generated and unique.  This is needed to check if
// toast is still present in the Windows Action Center when we receive a dismiss
// timeout.
//
// Local testing reveals that the space of tags is not global but instead is per
// AUMID.  Since an installation uses a unique AUMID incorporating the install
// directory hash, it should not witness another installation's tag.
nsresult ToastNotificationHandler::InitWindowsTag() {
  mWindowsTag.Truncate();

  nsAutoString tag;

  // Multiple profiles might overwrite each other's toast messages when a
  // common name is used for a given host port. We prevent this by including
  // the profile directory as part of the toast hash.
  nsCOMPtr<nsIFile> profDir;
  MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                 getter_AddRefs(profDir)));
  MOZ_TRY(profDir->GetPath(tag));

  if (!mHostPort.IsEmpty()) {
    // Notification originated from a web notification.
    // `mName` will be in the form `{mHostPort}#tag:{tag}` if the notification
    // was created with a tag and `{mHostPort}#notag:{uuid}` otherwise.
    tag += mName;
  } else {
    // Notification originated from the browser chrome.
    if (!mName.IsEmpty()) {
      tag += u"chrome#tag:"_ns;
      // Browser chrome notifications don't follow any convention for naming.
      tag += mName;
    } else {
      // No associated name, append a UUID to prevent reuse of the same tag.
      nsIDToCString uuidString(nsID::GenerateUUID());
      size_t len = strlen(uuidString.get());
      MOZ_ASSERT(len == NSID_LENGTH - 1);
      nsAutoString uuid;
      CopyASCIItoUTF16(nsDependentCSubstring(uuidString.get(), len), uuid);

      tag += u"chrome#notag:"_ns;
      tag += uuid;
    }
  }

  // Windows notification tags are limited to 16 characters, or 64 characters
  // after the Creators Update; therefore we hash the tag to fit the minimum
  // range.
  HashNumber hash = HashString(tag);
  mWindowsTag.AppendPrintf("%010u", hash);

  return NS_OK;
}

nsString ToastNotificationHandler::ActionArgsJSONString(
    const nsString& aAction, const nsString& aOpaqueRelaunchData = u""_ns) {
  nsAutoCString actionArgsData;

  JSONStringRefWriteFunc js(actionArgsData);
  JSONWriter w(js, JSONWriter::SingleLineStyle);
  w.Start();

  w.StringProperty("action", NS_ConvertUTF16toUTF8(aAction));

  if (mIsSystemPrincipal) {
    // Privileged/chrome alerts (not activated by Windows) can have custom
    // relaunch data.
    if (!aOpaqueRelaunchData.IsEmpty()) {
      w.StringProperty("opaqueRelaunchData",
                       NS_ConvertUTF16toUTF8(aOpaqueRelaunchData));
    }

    // Privileged alerts include any provided name for metrics.
    if (!mName.IsEmpty()) {
      w.StringProperty("privilegedName", NS_ConvertUTF16toUTF8(mName));
    }
  } else {
    if (!mHostPort.IsEmpty()) {
      w.StringProperty("launchUrl", NS_ConvertUTF16toUTF8(mHostPort));
    }
  }

  w.End();

  return NS_ConvertUTF8toUTF16(actionArgsData);
}

ComPtr<IXmlDocument> ToastNotificationHandler::CreateToastXmlDocument() {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();
  NS_ENSURE_TRUE(toastNotificationManagerStatics, nullptr);

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

    success = SetAttribute(image, HStringReference(L"src"), mImageUri);
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

  ComPtr<IXmlElement> toastElement;
  hr = toastNodeRoot.As(&toastElement);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  if (mRequireInteraction) {
    success = SetAttribute(toastElement, HStringReference(L"scenario"),
                           u"reminder"_ns);
    NS_ENSURE_TRUE(success, nullptr);
  }

  auto maybeLaunchArg = GetLaunchArgument();
  NS_ENSURE_TRUE(maybeLaunchArg.isOk(), nullptr);
  nsString launchArg = maybeLaunchArg.unwrap();

  nsString launchArgWithoutAction = launchArg;

  if (!mIsSystemPrincipal) {
    // Unprivileged/content alerts can't have custom relaunch data.
    NS_WARNING_ASSERTION(mOpaqueRelaunchData.IsEmpty(),
                         "unprivileged/content alert "
                         "should have trivial `mOpaqueRelaunchData`");
  }

  launchArg += u"\n"_ns + nsDependentString(kLaunchArgAction) + u"\n"_ns +
               ActionArgsJSONString(u""_ns, mOpaqueRelaunchData);

  success = SetAttribute(toastElement, HStringReference(L"launch"), launchArg);
  NS_ENSURE_TRUE(success, nullptr);

  MOZ_LOG(sWASLog, LogLevel::Debug,
          ("launchArg: '%s'", NS_ConvertUTF16toUTF8(launchArg).get()));

  // Use newer toast layout, which makes images larger, for system
  // (chrome-privileged) toasts.
  if (mIsSystemPrincipal) {
    ComPtr<IXmlNodeList> bindingElements;
    hr = toastXml->GetElementsByTagName(HStringReference(L"binding").Get(),
                                        &bindingElements);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    ComPtr<IXmlNode> bindingNodeRoot;
    hr = bindingElements->Item(0, &bindingNodeRoot);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    ComPtr<IXmlElement> bindingElement;
    hr = bindingNodeRoot.As(&bindingElement);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    success = SetAttribute(bindingElement, HStringReference(L"template"),
                           u"ToastGeneric"_ns);
    NS_ENSURE_TRUE(success, nullptr);
  }

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
        SetAttribute(placementText, HStringReference(L"placement"),
                     u"attribution"_ns);
      }
    }

    nsAutoString disableButtonTitle;
    ns = bundle->FormatStringFromName("webActions.disableForOrigin.label",
                                      formatStrings, disableButtonTitle);
    NS_ENSURE_SUCCESS(ns, nullptr);

    AddActionNode(toastXml, actionsNode, disableButtonTitle,
                  // TODO: launch into `about:preferences`?
                  launchArgWithoutAction, ActionArgsJSONString(u"snooze"_ns),
                  u"contextmenu"_ns);
  }

  bool wantSettings = true;
#ifdef MOZ_BACKGROUNDTASKS
  if (BackgroundTasks::IsBackgroundTaskMode()) {
    // Notifications popped from a background task want to invoke Firefox with a
    // different profile -- the default browsing profile.  Don't link to Firefox
    // settings in some different profile: the relevant Firefox settings won't
    // take effect.
    wantSettings = false;
  }
#endif
  if (MOZ_LIKELY(wantSettings)) {
    nsAutoString settingsButtonTitle;
    bundle->GetStringFromName("webActions.settings.label", settingsButtonTitle);
    success = AddActionNode(
        toastXml, actionsNode, settingsButtonTitle, launchArgWithoutAction,
        // TODO: launch into `about:preferences`?
        ActionArgsJSONString(u"settings"_ns), u"contextmenu"_ns);
    NS_ENSURE_TRUE(success, nullptr);
  }

  for (const auto& action : mActions) {
    // Bug 1778596: include per-action icon from image URL.
    nsString title;
    ns = action->GetTitle(title);
    NS_ENSURE_SUCCESS(ns, nullptr);

    nsString actionString;
    ns = action->GetAction(actionString);
    NS_ENSURE_SUCCESS(ns, nullptr);

    nsString opaqueRelaunchData;
    ns = action->GetOpaqueRelaunchData(opaqueRelaunchData);
    NS_ENSURE_SUCCESS(ns, nullptr);

    MOZ_LOG(sWASLog, LogLevel::Debug,
            ("launchArgWithoutAction for '%s': '%s'",
             NS_ConvertUTF16toUTF8(actionString).get(),
             NS_ConvertUTF16toUTF8(launchArgWithoutAction).get()));

    // Privileged/chrome alerts can have actions that are activated by Windows.
    // Recognize these actions and enable these activations.
    bool activationType(false);
    ns = action->GetWindowsSystemActivationType(&activationType);
    NS_ENSURE_SUCCESS(ns, nullptr);

    nsString activationTypeString(
        (mIsSystemPrincipal && activationType) ? u"system"_ns : u""_ns);

    nsString actionArgs;
    if (mIsSystemPrincipal && activationType) {
      // Privileged/chrome alerts that are activated by Windows can't have
      // custom relaunch data.
      actionArgs = actionString;

      NS_WARNING_ASSERTION(opaqueRelaunchData.IsEmpty(),
                           "action with `windowsSystemActivationType=true` "
                           "should have trivial `opaqueRelaunchData`");
    } else {
      actionArgs = ActionArgsJSONString(actionString, opaqueRelaunchData);
    }

    success = AddActionNode(toastXml, actionsNode, title,
                            /* launchArg */ launchArgWithoutAction,
                            /* actionArgs */ actionArgs,
                            /* actionPlacement */ u""_ns,
                            /* activationType */ activationTypeString);
    NS_ENSURE_TRUE(success, nullptr);
  }

  // Windows ignores scenario=reminder added by mRequiredInteraction if
  // there's no non-contextmenu activationType=background action.
  if (mRequireInteraction && !mActions.Length()) {
    nsTArray<nsCString> resIds = {
        "toolkit/global/alert.ftl"_ns,
    };
    RefPtr<intl::Localization> l10n = intl::Localization::Create(resIds, true);
    IgnoredErrorResult rv;
    nsAutoCString closeTitle;
    l10n->FormatValueSync("notification-default-dismiss"_ns, {}, closeTitle,
                          rv);
    NS_ENSURE_TRUE(!rv.Failed(), nullptr);

    NS_ENSURE_TRUE(
        AddActionNode(toastXml, actionsNode, NS_ConvertUTF8toUTF16(closeTitle),
                      launchArg, u""_ns, u""_ns, u"background"_ns),
        nullptr);
  }

  ComPtr<IXmlNode> appendedChild;
  hr = toastNodeRoot->AppendChild(actionsNode.Get(), &appendedChild);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  if (mIsSilent) {
    ComPtr<IXmlNode> audioNode;
    // Create <audio silent="true"/> for silent notifications.
    ComPtr<IXmlElement> audio;
    hr = toastXml->CreateElement(HStringReference(L"audio").Get(), &audio);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    SetAttribute(audio, HStringReference(L"silent"), u"true"_ns);

    hr = audio.As(&audioNode);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
    hr = toastNodeRoot->AppendChild(audioNode.Get(), &appendedChild);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  }

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

  return CreateWindowsNotificationFromXml(toastXml);
}

bool ToastNotificationHandler::IsPrivate() { return mInPrivateBrowsing; }

void ToastNotificationHandler::HideAlert() {
  if (mNotifier && mNotification) {
    mNotifier->Hide(mNotification.Get());
  }
}

bool ToastNotificationHandler::CreateWindowsNotificationFromXml(
    ComPtr<IXmlDocument>& aXml) {
  ComPtr<IToastNotificationFactory> factory;
  HRESULT hr;

  hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification)
          .Get(),
      &factory);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = factory->CreateToastNotification(aXml.Get(), &mNotification);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  RefPtr<ToastNotificationHandler> self = this;

  hr = mNotification->add_Activated(
      Callback<ToastActivationHandler>([self](IToastNotification* aNotification,
                                              IInspectable* aInspectable) {
        return self->OnActivate(ComPtr<IToastNotification>(aNotification),
                                ComPtr<IInspectable>(aInspectable));
      }).Get(),
      &mActivatedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotification->add_Dismissed(
      Callback<ToastDismissedHandler>([self](IToastNotification* aNotification,
                                             IToastDismissedEventArgs* aArgs) {
        return self->OnDismiss(ComPtr<IToastNotification>(aNotification),
                               ComPtr<IToastDismissedEventArgs>(aArgs));
      }).Get(),
      &mDismissedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotification->add_Failed(
      Callback<ToastFailedHandler>([self](IToastNotification* aNotification,
                                          IToastFailedEventArgs* aArgs) {
        return self->OnFail(ComPtr<IToastNotification>(aNotification),
                            ComPtr<IToastFailedEventArgs>(aArgs));
      }).Get(),
      &mFailedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IToastNotification2> notification2;
  hr = mNotification.As(&notification2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  HString hTag;
  hr = hTag.Set(mWindowsTag.get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = notification2->put_Tag(hTag.Get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();
  NS_ENSURE_TRUE(toastNotificationManagerStatics, false);

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
ToastNotificationHandler::OnActivate(
    const ComPtr<IToastNotification>& notification,
    const ComPtr<IInspectable>& inspectable) {
  MOZ_LOG(sWASLog, LogLevel::Info, ("OnActivate"));

  if (mAlertListener) {
    // Extract the `action` value from the argument string.
    nsAutoString actionString;
    if (inspectable) {
      ComPtr<IToastActivatedEventArgs> eventArgs;
      HRESULT hr = inspectable.As(&eventArgs);
      if (SUCCEEDED(hr)) {
        HString arguments;
        hr = eventArgs->get_Arguments(arguments.GetAddressOf());
        if (SUCCEEDED(hr)) {
          uint32_t len = 0;
          const char16_t* buffer = (char16_t*)arguments.GetRawBuffer(&len);
          if (buffer) {
            MOZ_LOG(sWASLog, LogLevel::Info,
                    ("OnActivate: arguments: %s",
                     NS_ConvertUTF16toUTF8(buffer).get()));

            // Toast arguments are a newline separated key/value combination of
            // launch arguments and an optional action argument provided as an
            // argument to the toast's constructor. After the `action` key is
            // found, the remainder of toast argument (including newlines) is
            // the `action` value.
            Tokenizer16 parse(buffer);
            nsDependentSubstring token;

            while (parse.ReadUntil(Tokenizer16::Token::NewLine(), token)) {
              if (token == nsDependentString(kLaunchArgAction)) {
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

    // TODO: extract `action` from `actionString`, which is now JSON.

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

// Returns `nullptr` if no such toast exists.
/* static */ ComPtr<IToastNotification>
ToastNotificationHandler::FindNotificationByTag(const nsAString& aWindowsTag,
                                                const nsAString& aAumid) {
  HRESULT hr = S_OK;

  HString current_id;
  current_id.Set(PromiseFlatString(aWindowsTag).get());

  ComPtr<IToastNotificationManagerStatics> manager =
      GetToastNotificationManagerStatics();
  NS_ENSURE_TRUE(manager, nullptr);

  ComPtr<IToastNotificationManagerStatics2> manager2;
  hr = manager.As(&manager2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IToastNotificationHistory> history;
  hr = manager2->get_History(&history);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  ComPtr<IToastNotificationHistory2> history2;
  hr = history.As(&history2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IVectorView_ToastNotification> toasts;
  hr = history2->GetHistoryWithId(
      HStringReference(PromiseFlatString(aAumid).get()).Get(), &toasts);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  unsigned int hist_size;
  hr = toasts->get_Size(&hist_size);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  for (unsigned int i = 0; i < hist_size; i++) {
    ComPtr<IToastNotification> hist_toast;
    hr = toasts->GetAt(i, &hist_toast);
    if (NS_WARN_IF(FAILED(hr))) {
      continue;
    }

    ComPtr<IToastNotification2> hist_toast2;
    hr = hist_toast.As(&hist_toast2);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    HString history_id;
    hr = hist_toast2->get_Tag(history_id.GetAddressOf());
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    // We can not directly compare IToastNotification objects; their IUnknown
    // pointers should be equivalent but under inspection were not. Therefore we
    // use the notification's tag instead.
    if (current_id == history_id) {
      return hist_toast;
    }
  }

  return nullptr;
}

// A single toast message can receive multiple dismiss events, at most one for
// the popup and at most one for the action center. We can't simply count
// dismiss events as the user may have disabled either popups or action center
// notifications, therefore we have to check if the toast remains in the history
// (action center) to determine if the toast is fully dismissed.
HRESULT
ToastNotificationHandler::OnDismiss(
    const ComPtr<IToastNotification>& notification,
    const ComPtr<IToastDismissedEventArgs>& aArgs) {
  ComPtr<IToastNotification2> notification2;
  HRESULT hr = notification.As(&notification2);
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  HString tagHString;
  hr = notification2->get_Tag(tagHString.GetAddressOf());
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  unsigned int len;
  const wchar_t* tagPtr = tagHString.GetRawBuffer(&len);
  nsAutoString tag(tagPtr, len);

  if (FindNotificationByTag(tag, mAumid)) {
    return S_OK;
  }

  SendFinished();
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnFail(const ComPtr<IToastNotification>& notification,
                                 const ComPtr<IToastFailedEventArgs>& aArgs) {
  HRESULT err;
  aArgs->get_ErrorCode(&err);
  MOZ_LOG(sWASLog, LogLevel::Error,
          ("Error creating notification, error: %ld", err));

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
  uuidStr.AppendLiteral(".png");
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
      "ToastNotificationHandler::AsyncWriteImage",
      [self, imageFile, surface]() -> void {
        nsresult rv = NS_ERROR_FAILURE;
        if (surface) {
          FILE* file = nullptr;
          rv = imageFile->OpenANSIFileDesc("wb", &file);
          if (NS_SUCCEEDED(rv)) {
            rv = gfxUtils::EncodeSourceSurface(surface, ImageType::PNG, u""_ns,
                                               gfxUtils::eBinaryEncode, file);
            fclose(file);
          }
        }

        nsCOMPtr<nsIRunnable> cbRunnable = NS_NewRunnableFunction(
            "ToastNotificationHandler::AsyncWriteImageCb",
            [self, rv]() -> void {
              auto handler = const_cast<ToastNotificationHandler*>(self.get());
              handler->OnWriteImageFinished(rv);
            });

        NS_DispatchToMainThread(cbRunnable);
      });

  return mBackend->BackgroundDispatch(r);
}

void ToastNotificationHandler::OnWriteImageFinished(nsresult rv) {
  if (NS_SUCCEEDED(rv)) {
    OnWriteImageSuccess();
  }
  TryShowAlert();
}

nsresult ToastNotificationHandler::OnWriteImageSuccess() {
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
