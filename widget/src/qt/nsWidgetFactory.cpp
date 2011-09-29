/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * John C. Griggs <johng@corel.com>.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Zack Rusin <zack@kde.org>
 *   Lars Knoll <knoll@kde.org>
 *   John C. Griggs <johng@corel.com>
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWindow.h"
#include "nsAppShell.h"

#include "mozilla/ModuleUtils.h"
#include "nsIModule.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsToolkit.h"
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
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceQt)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)

static nsresult
nsFilePickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                        void **aResult)
{
  *aResult = nsnull;
  if (aOuter != nsnull) {
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

    *aResult = NULL;
    if (NULL != aOuter)
        return NS_ERROR_NO_AGGREGATION;

    inst = new nsNativeThemeQt();
    if (NULL == inst)
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
NS_DEFINE_NAMED_CID(NS_TOOLKIT_CID);
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
    { &kNS_WINDOW_CID, false, NULL, nsWindowConstructor },
    { &kNS_CHILD_CID, false, NULL, nsChildWindowConstructor },
    { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
    { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor },
    { &kNS_SOUND_CID, false, NULL, nsSoundConstructor },
    { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
    { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor },
    { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
    { &kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor },
    { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
    { &kNS_BIDIKEYBOARD_CID, false, NULL, nsBidiKeyboardConstructor },
    { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerQtConstructor },
    { &kNS_THEMERENDERER_CID, false, NULL, nsNativeThemeQtConstructor },
    { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceQtConstructor },
    { &kNS_POPUP_CID, false, NULL, nsPopupWindowConstructor },
    { &kNS_TOOLKIT_CID, false, NULL, nsToolkitConstructor },
#ifdef NS_PRINTING
    { &kNS_PRINTSETTINGSSERVICE_CID, false, NULL, nsPrintOptionsQtConstructor },
    { &kNS_PRINTER_ENUMERATOR_CID, false, NULL, nsPrinterEnumeratorQtConstructor },
    { &kNS_PRINTSESSION_CID, false, NULL, nsPrintSessionConstructor },
    { &kNS_DEVICE_CONTEXT_SPEC_CID, false, NULL, nsDeviceContextSpecQtConstructor },
    { &kNS_PRINTDIALOGSERVICE_CID, false, NULL, nsPrintDialogServiceQtConstructor },
#endif
#if defined(MOZ_X11)
    { &kNS_GFXINFO_CID, false, NULL, mozilla::widget::GfxInfoConstructor },
#endif
    { NULL }
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
    { "@mozilla.org/widget/toolkit/qt;1", &kNS_TOOLKIT_CID },
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
    { NULL }
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
    NULL,
    NULL,
    nsAppShellInit,
    nsWidgetQtModuleDtor
};

NSMODULE_DEFN(nsWidgetQtModule) = &kWidgetModule;
