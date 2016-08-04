/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "mozilla/WidgetUtils.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "AndroidBridge.h"

#include "nsWindow.h"
#include "nsLookAndFeel.h"
#include "nsAppShellSingleton.h"
#include "nsScreenManagerAndroid.h"

#include "nsIdleServiceAndroid.h"
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsTransferable.h"
#include "nsPrintOptionsAndroid.h"
#include "nsPrintSession.h"
#include "nsDeviceContextAndroid.h"
#include "nsHTMLFormatConverter.h"
#include "nsXULAppAPI.h"
#include "nsAndroidProtocolHandler.h"

#include "nsToolkitCompsCID.h"
#include "AndroidAlerts.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerAndroid)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceAndroid, nsIdleServiceAndroid::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsAndroid, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecAndroid)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAndroidBridge)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAndroidProtocolHandler)

#include "GfxInfo.h"
namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(AndroidAlerts)
}
}

NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
NS_DEFINE_NAMED_CID(NS_ANDROIDBRIDGE_CID);
NS_DEFINE_NAMED_CID(NS_ANDROIDPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_SYSTEMALERTSSERVICE_CID);

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
  { &kNS_WINDOW_CID, false, nullptr, nsWindowConstructor },
  { &kNS_CHILD_CID, false, nullptr, nsWindowConstructor },
  { &kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor },
  { &kNS_SCREENMANAGER_CID, false, nullptr, nsScreenManagerAndroidConstructor },
  { &kNS_IDLE_SERVICE_CID, false, nullptr, nsIdleServiceAndroidConstructor },
  { &kNS_TRANSFERABLE_CID, false, nullptr, nsTransferableConstructor },
  { &kNS_CLIPBOARD_CID, false, nullptr, nsClipboardConstructor },
  { &kNS_CLIPBOARDHELPER_CID, false, nullptr, nsClipboardHelperConstructor },
  { &kNS_PRINTSETTINGSSERVICE_CID, false, nullptr, nsPrintOptionsAndroidConstructor },
  { &kNS_PRINTSESSION_CID, false, nullptr, nsPrintSessionConstructor },
  { &kNS_DEVICE_CONTEXT_SPEC_CID, false, nullptr, nsDeviceContextSpecAndroidConstructor },
  { &kNS_HTMLFORMATCONVERTER_CID, false, nullptr, nsHTMLFormatConverterConstructor },
  { &kNS_GFXINFO_CID, false, nullptr, mozilla::widget::GfxInfoConstructor },
  { &kNS_ANDROIDBRIDGE_CID, false, nullptr, nsAndroidBridgeConstructor },
  { &kNS_ANDROIDPROTOCOLHANDLER_CID, false, nullptr, nsAndroidProtocolHandlerConstructor },
  { &kNS_SYSTEMALERTSSERVICE_CID, false, nullptr, mozilla::widget::AndroidAlertsConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widgets/window/android;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widgets/child_window/android;1", &kNS_CHILD_CID },
  { "@mozilla.org/widget/appshell/android;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
  { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
  { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { "@mozilla.org/android/bridge;1", &kNS_ANDROIDBRIDGE_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "android", &kNS_ANDROIDPROTOCOLHANDLER_CID },
  { NS_SYSTEMALERTSERVICE_CONTRACTID, &kNS_SYSTEMALERTSSERVICE_CID },
  { nullptr }
};

static void
nsWidgetAndroidModuleDtor()
{
    // Shutdown all XP level widget classes.
    mozilla::widget::WidgetUtils::Shutdown();

    nsLookAndFeel::Shutdown();
    nsAppShellShutdown();
}

static const mozilla::Module kWidgetModule = {
    mozilla::Module::kVersion,
    kWidgetCIDs,
    kWidgetContracts,
    nullptr,
    nullptr,
    nsAppShellInit,
    nsWidgetAndroidModuleDtor
};

NSMODULE_DEFN(nsWidgetAndroidModule) = &kWidgetModule;
