/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/basictypes.h"

#include "mozilla/ModuleUtils.h"
#include "mozilla/WidgetUtils.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"

#include "nsWindow.h"
#include "nsLookAndFeel.h"
#include "nsAppShellSingleton.h"
#include "nsScreenManagerQt.h"
#include "nsIdleServiceQt.h"
#include "nsTransferable.h"
#include "nsBidiKeyboard.h"

#include "nsHTMLFormatConverter.h"
#include "nsXULAppAPI.h"

#ifdef NS_PRINTING
#include "nsDeviceContextSpecQt.h"
#include "nsPrintSession.h"
#include "nsPrintOptionsQt.h"
#include "nsPrintDialogQt.h"
#endif

#include "nsClipboard.h"
#include "nsClipboardHelper.h"

#if defined(MOZ_X11)
#include "GfxInfoX11.h"
#else
#include "GfxInfo.h"
#endif

using namespace mozilla::widget;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceQt, nsIdleServiceQt::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecQt)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsQt, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorQt)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintDialogServiceQt, Init)
#endif

namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}
}


NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_PRINTDIALOGSERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    { &kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor },
    { &kNS_BIDIKEYBOARD_CID, false, nullptr, nsBidiKeyboardConstructor },
    { &kNS_CHILD_CID, false, nullptr, nsWindowConstructor },
    { &kNS_CLIPBOARD_CID, false, nullptr, nsClipboardConstructor },
    { &kNS_CLIPBOARDHELPER_CID, false, nullptr, nsClipboardHelperConstructor },
    { &kNS_HTMLFORMATCONVERTER_CID, false, nullptr, nsHTMLFormatConverterConstructor },
    { &kNS_IDLE_SERVICE_CID, false, nullptr, nsIdleServiceQtConstructor },
    { &kNS_SCREENMANAGER_CID, false, nullptr, nsScreenManagerQtConstructor },
    { &kNS_TRANSFERABLE_CID, false, nullptr, nsTransferableConstructor },
    { &kNS_WINDOW_CID, false, nullptr, nsWindowConstructor },
    { &kNS_GFXINFO_CID, false, nullptr, mozilla::widget::GfxInfoConstructor },
#ifdef NS_PRINTING
    { &kNS_DEVICE_CONTEXT_SPEC_CID, false, nullptr, nsDeviceContextSpecQtConstructor },
    { &kNS_PRINTDIALOGSERVICE_CID, false, nullptr, nsPrintDialogServiceQtConstructor },
    { &kNS_PRINTER_ENUMERATOR_CID, false, nullptr, nsPrinterEnumeratorQtConstructor },
    { &kNS_PRINTSESSION_CID, false, nullptr, nsPrintSessionConstructor },
    { &kNS_PRINTSETTINGSSERVICE_CID, false, nullptr, nsPrintOptionsQtConstructor },
#endif
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
    { "@mozilla.org/widget/appshell/qt;1", &kNS_APPSHELL_CID },
    { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
    { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
    { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
    { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
    { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
    { "@mozilla.org/widgets/child_window/qt;1", &kNS_CHILD_CID },
    { "@mozilla.org/widgets/window/qt;1", &kNS_WINDOW_CID },
    { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
    { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
#ifdef NS_PRINTING
    { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
    { "@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID },
    { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
    { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
    { NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID },
#endif
    { nullptr }
};

static void
nsWidgetQtModuleDtor()
{
    // Shutdown all XP level widget classes.
    WidgetUtils::Shutdown();

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
    nsWidgetQtModuleDtor
};

NSMODULE_DEFN(nsWidgetQtModule) = &kWidgetModule;
