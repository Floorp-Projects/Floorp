/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindow.h"
#include "nsAppShell.h"

#include "mozilla/ModuleUtils.h"
#include "nsIModule.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsHTMLFormatConverter.h"
#include "nsTransferable.h"
#include "nsLookAndFeel.h"
#include "nsAppShellSingleton.h"
#include "nsScreenManagerQt.h"
#include "nsFilePicker.h"
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsIdleServiceQt.h"
#include "nsDragService.h"
#include "nsSound.h"
#include "nsBidiKeyboard.h"
#include "nsNativeThemeQt.h"
#ifdef NS_PRINTING
#include "nsDeviceContextSpecQt.h"
#include "nsPrintSession.h"
#include "nsPrintOptionsQt.h"
#include "nsPrintDialogQt.h"
#endif
#include "nsFilePickerProxy.h"
#include "nsXULAppAPI.h"

#if defined(MOZ_X11)
#include "GfxInfoX11.h"
#endif

// from nsWindow.cpp
extern bool gDisableNativeTheme;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPopupWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceQt, nsIdleServiceQt::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)

static nsresult
nsFilePickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                        void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
      return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIFilePicker> picker;
  
    if (XRE_GetProcessType() == GeckoProcessType_Content)
        picker = new nsFilePickerProxy();
    else 
        picker = new nsFilePicker;

  return picker->QueryInterface(aIID, aResult);
}


#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecQt)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsQt, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorQt)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintDialogServiceQt, Init)
#endif

#if defined(MOZ_X11)
namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}
}
#endif


static nsresult
nsNativeThemeQtConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
    nsresult rv;
    nsNativeThemeQt *inst;

    if (gDisableNativeTheme)
        return NS_ERROR_NO_INTERFACE;

    *aResult = nullptr;
    if (nullptr != aOuter)
        return NS_ERROR_NO_AGGREGATION;

    inst = new nsNativeThemeQt();
    if (nullptr == inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return rv;
}

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_THEMERENDERER_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_POPUP_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_PRINTDIALOGSERVICE_CID);
#endif

#if defined(MOZ_X11)
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
#endif

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    { &kNS_WINDOW_CID, false, nullptr, nsWindowConstructor },
    { &kNS_CHILD_CID, false, nullptr, nsChildWindowConstructor },
    { &kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor },
    { &kNS_FILEPICKER_CID, false, nullptr, nsFilePickerConstructor },
    { &kNS_SOUND_CID, false, nullptr, nsSoundConstructor },
    { &kNS_TRANSFERABLE_CID, false, nullptr, nsTransferableConstructor },
    { &kNS_CLIPBOARD_CID, false, nullptr, nsClipboardConstructor },
    { &kNS_CLIPBOARDHELPER_CID, false, nullptr, nsClipboardHelperConstructor },
    { &kNS_DRAGSERVICE_CID, false, nullptr, nsDragServiceConstructor },
    { &kNS_HTMLFORMATCONVERTER_CID, false, nullptr, nsHTMLFormatConverterConstructor },
    { &kNS_BIDIKEYBOARD_CID, false, nullptr, nsBidiKeyboardConstructor },
    { &kNS_SCREENMANAGER_CID, false, nullptr, nsScreenManagerQtConstructor },
    { &kNS_THEMERENDERER_CID, false, nullptr, nsNativeThemeQtConstructor },
    { &kNS_IDLE_SERVICE_CID, false, nullptr, nsIdleServiceQtConstructor },
    { &kNS_POPUP_CID, false, nullptr, nsPopupWindowConstructor },
#ifdef NS_PRINTING
    { &kNS_PRINTSETTINGSSERVICE_CID, false, nullptr, nsPrintOptionsQtConstructor },
    { &kNS_PRINTER_ENUMERATOR_CID, false, nullptr, nsPrinterEnumeratorQtConstructor },
    { &kNS_PRINTSESSION_CID, false, nullptr, nsPrintSessionConstructor },
    { &kNS_DEVICE_CONTEXT_SPEC_CID, false, nullptr, nsDeviceContextSpecQtConstructor },
    { &kNS_PRINTDIALOGSERVICE_CID, false, nullptr, nsPrintDialogServiceQtConstructor },
#endif
#if defined(MOZ_X11)
    { &kNS_GFXINFO_CID, false, nullptr, mozilla::widget::GfxInfoConstructor },
#endif
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    { "@mozilla.org/widget/window/qt;1", &kNS_WINDOW_CID },
    { "@mozilla.org/widgets/child_window/qt;1", &kNS_CHILD_CID },
    { "@mozilla.org/widget/appshell/qt;1", &kNS_APPSHELL_CID },
    { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID },
    { "@mozilla.org/sound;1", &kNS_SOUND_CID },
    { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
    { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
    { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
    { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID },
    { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
    { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
    { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
    { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
    { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
    { "@mozilla.org/widgets/popup_window/qt;1", &kNS_POPUP_CID },
#ifdef NS_PRINTING
    { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
    { "@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID },
    { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
    { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
    { NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID },
#endif
#if defined(MOZ_X11)
    { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
#endif
    { nullptr }
};

static void
nsWidgetQtModuleDtor()
{
    nsLookAndFeel::Shutdown();
    nsSound::Shutdown();
    nsWindow::ReleaseGlobals();
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
