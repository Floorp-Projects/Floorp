/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "mozilla/ModuleUtils.h"

#include "nsWidgetsCID.h"

#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsLookAndFeel.h"
#include "nsScreenManager.h"
#include "nsWindow.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(UIKitScreenManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)

#include "GfxInfo.h"
namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}
}

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
  { &kNS_WINDOW_CID, false, nullptr, nsWindowConstructor },
  { &kNS_CHILD_CID, false, nullptr, nsWindowConstructor },
  { &kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor },
  { &kNS_SCREENMANAGER_CID, false, nullptr, UIKitScreenManagerConstructor },
  { &kNS_GFXINFO_CID, false, nullptr, mozilla::widget::GfxInfoConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widgets/window/uikit;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widgets/childwindow/uikit;1", &kNS_CHILD_CID },
  { "@mozilla.org/widget/appshell/uikit;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { nullptr }
};

static void
nsWidgetUIKitModuleDtor()
{
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
  nsWidgetUIKitModuleDtor
};

NSMODULE_DEFN(nsWidgetUIKitModule) = &kWidgetModule;
