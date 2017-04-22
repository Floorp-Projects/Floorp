/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "mozilla/WidgetUtils.h"
#include "NativeKeyBindings.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsBaseWidget.h"
#include "nsGtkKeyUtils.h"
#include "nsLookAndFeel.h"
#include "nsWindow.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"
#ifdef MOZ_X11
#include "nsClipboardHelper.h"
#include "nsClipboard.h"
#include "nsDragService.h"
#endif
#if (MOZ_WIDGET_GTK == 3)
#include "nsApplicationChooser.h"
#endif
#include "nsColorPicker.h"
#include "nsFilePicker.h"
#include "nsSound.h"
#include "nsBidiKeyboard.h"
#include "nsGTKToolkit.h"
#include "WakeLockListener.h"

#ifdef NS_PRINTING
#include "nsPrintOptionsGTK.h"
#include "nsPrintSession.h"
#include "nsDeviceContextSpecG.h"
#endif

#include "mozilla/Preferences.h"

#include "nsImageToPixbuf.h"
#include "nsPrintDialogGTK.h"

#if defined(MOZ_X11)
#include "nsIdleServiceGTK.h"
#include "GfxInfoX11.h"
#endif

#include "nsNativeThemeGTK.h"

#include "nsIComponentRegistrar.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/ScreenManager.h"
#include <gtk/gtk.h>

using namespace mozilla;
using namespace mozilla::widget;

/* from nsFilePicker.js */
#define XULFILEPICKER_CID \
  { 0x54ae32f8, 0x1dd2, 0x11b2, \
    { 0xa2, 0x09, 0xdf, 0x7c, 0x50, 0x53, 0x70, 0xf8} }
static NS_DEFINE_CID(kXULFilePickerCID, XULFILEPICKER_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
#ifdef MOZ_X11
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceGTK, nsIdleServiceGTK::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIClipboard, nsClipboard::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsDragService, nsDragService::GetInstance)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(ScreenManager, ScreenManager::GetAddRefedSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageToPixbuf)


// from nsWindow.cpp
extern bool gDisableNativeTheme;

static nsresult
nsNativeThemeGTKConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
    if (gfxPlatform::IsHeadless()) {
        return NS_ERROR_NO_INTERFACE;
    }
    nsresult rv;
    nsNativeThemeGTK * inst;

    if (gDisableNativeTheme)
        return NS_ERROR_NO_INTERFACE;

    *aResult = nullptr;
    if (nullptr != aOuter) {
        rv = NS_ERROR_NO_AGGREGATION;
        return rv;
    }

    inst = new nsNativeThemeGTK();
    if (nullptr == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        return rv;
    }
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return rv;
}

#if defined(MOZ_X11)
namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}
}
#endif

#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsGTK, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintDialogServiceGTK, Init)
#endif

static nsresult
nsFilePickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                        void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  bool allowPlatformPicker =
      Preferences::GetBool("ui.allow_platform_file_picker", true);

  nsCOMPtr<nsIFilePicker> picker;
  if (allowPlatformPicker) {
      picker = new nsFilePicker;
  } else {
    picker = do_CreateInstance(kXULFilePickerCID);
  }

  if (!picker) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return picker->QueryInterface(aIID, aResult);
}

#if (MOZ_WIDGET_GTK == 3)
static nsresult
nsApplicationChooserConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIApplicationChooser> chooser = new nsApplicationChooser;

  if (!chooser) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return chooser->QueryInterface(aIID, aResult);
}
#endif

static nsresult
nsColorPickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                         void **aResult)
{
    *aResult = nullptr;
    if (aOuter != nullptr) {
        return NS_ERROR_NO_AGGREGATION;
    }

    nsCOMPtr<nsIColorPicker> picker = new nsColorPicker;

    if (!picker) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return picker->QueryInterface(aIID, aResult);
}

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_COLORPICKER_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
#if (MOZ_WIDGET_GTK == 3)
NS_DEFINE_NAMED_CID(NS_APPLICATIONCHOOSER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
#ifdef MOZ_X11
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_THEMERENDERER_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_PRINTDIALOGSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_IMAGE_TO_PIXBUF_CID);
#if defined(MOZ_X11)
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
#endif


static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    { &kNS_WINDOW_CID, false, nullptr, nsWindowConstructor },
    { &kNS_CHILD_CID, false, nullptr, nsChildWindowConstructor },
    { &kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor, Module::ALLOW_IN_GPU_PROCESS },
    { &kNS_COLORPICKER_CID, false, nullptr, nsColorPickerConstructor, Module::MAIN_PROCESS_ONLY },
    { &kNS_FILEPICKER_CID, false, nullptr, nsFilePickerConstructor, Module::MAIN_PROCESS_ONLY },
#if (MOZ_WIDGET_GTK == 3)
    { &kNS_APPLICATIONCHOOSER_CID, false, nullptr, nsApplicationChooserConstructor, Module::MAIN_PROCESS_ONLY },
#endif
    { &kNS_SOUND_CID, false, nullptr, nsSoundConstructor, Module::MAIN_PROCESS_ONLY },
    { &kNS_TRANSFERABLE_CID, false, nullptr, nsTransferableConstructor },
#ifdef MOZ_X11
    { &kNS_CLIPBOARD_CID, false, nullptr, nsIClipboardConstructor, Module::MAIN_PROCESS_ONLY },
    { &kNS_CLIPBOARDHELPER_CID, false, nullptr, nsClipboardHelperConstructor },
    { &kNS_DRAGSERVICE_CID, false, nullptr, nsDragServiceConstructor, Module::MAIN_PROCESS_ONLY },
#endif
    { &kNS_HTMLFORMATCONVERTER_CID, false, nullptr, nsHTMLFormatConverterConstructor },
    { &kNS_BIDIKEYBOARD_CID, false, nullptr, nsBidiKeyboardConstructor },
    { &kNS_SCREENMANAGER_CID, false, nullptr, ScreenManagerConstructor,
      Module::MAIN_PROCESS_ONLY },
    { &kNS_THEMERENDERER_CID, false, nullptr, nsNativeThemeGTKConstructor },
#ifdef NS_PRINTING
    { &kNS_PRINTSETTINGSSERVICE_CID, false, nullptr, nsPrintOptionsGTKConstructor },
    { &kNS_PRINTER_ENUMERATOR_CID, false, nullptr, nsPrinterEnumeratorGTKConstructor },
    { &kNS_PRINTSESSION_CID, false, nullptr, nsPrintSessionConstructor },
    { &kNS_DEVICE_CONTEXT_SPEC_CID, false, nullptr, nsDeviceContextSpecGTKConstructor },
    { &kNS_PRINTDIALOGSERVICE_CID, false, nullptr, nsPrintDialogServiceGTKConstructor },
#endif
    { &kNS_IMAGE_TO_PIXBUF_CID, false, nullptr, nsImageToPixbufConstructor },
#if defined(MOZ_X11)
    { &kNS_IDLE_SERVICE_CID, false, nullptr, nsIdleServiceGTKConstructor },
    { &kNS_GFXINFO_CID, false, nullptr, mozilla::widget::GfxInfoConstructor },
#endif
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    { "@mozilla.org/widget/window/gtk;1", &kNS_WINDOW_CID },
    { "@mozilla.org/widgets/child_window/gtk;1", &kNS_CHILD_CID },
    { "@mozilla.org/widget/appshell/gtk;1", &kNS_APPSHELL_CID, Module::ALLOW_IN_GPU_PROCESS },
    { "@mozilla.org/colorpicker;1", &kNS_COLORPICKER_CID, Module::MAIN_PROCESS_ONLY },
    { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID, Module::MAIN_PROCESS_ONLY },
#if (MOZ_WIDGET_GTK == 3)
    { "@mozilla.org/applicationchooser;1", &kNS_APPLICATIONCHOOSER_CID, Module::MAIN_PROCESS_ONLY },
#endif
    { "@mozilla.org/sound;1", &kNS_SOUND_CID, Module::MAIN_PROCESS_ONLY },
    { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
#ifdef MOZ_X11
    { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID, Module::MAIN_PROCESS_ONLY },
    { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
    { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID, Module::MAIN_PROCESS_ONLY },
#endif
    { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
    { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID,
      Module::MAIN_PROCESS_ONLY },
    { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID,
      Module::MAIN_PROCESS_ONLY },
    { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
#ifdef NS_PRINTING
    { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
    { "@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID },
    { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
    { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
    { NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID },
#endif
    { "@mozilla.org/widget/image-to-gdk-pixbuf;1", &kNS_IMAGE_TO_PIXBUF_CID },
#if defined(MOZ_X11)
    { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
    { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
#endif
    { nullptr }
};

static void
nsWidgetGtk2ModuleDtor()
{
  // Shutdown all XP level widget classes.
  WidgetUtils::Shutdown();

  NativeKeyBindings::Shutdown();
  nsLookAndFeel::Shutdown();
  nsFilePicker::Shutdown();
  nsSound::Shutdown();
  nsWindow::ReleaseGlobals();
  KeymapWrapper::Shutdown();
  nsGTKToolkit::Shutdown();
  nsAppShellShutdown();
#ifdef MOZ_ENABLE_DBUS
  WakeLockListener::Shutdown();
#endif
}

static const mozilla::Module kWidgetModule = {
    mozilla::Module::kVersion,
    kWidgetCIDs,
    kWidgetContracts,
    nullptr,
    nullptr,
    nsAppShellInit,
    nsWidgetGtk2ModuleDtor,
    Module::ALLOW_IN_GPU_PROCESS
};

NSMODULE_DEFN(nsWidgetGtk2Module) = &kWidgetModule;
