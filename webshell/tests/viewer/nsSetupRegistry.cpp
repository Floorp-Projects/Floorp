/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#define NS_IMPL_IDS
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"

#include "nsIDocumentLoader.h"

#include "nsDOMCID.h"
#ifdef OJI
#include "nsILiveconnect.h"
#include "nsIJVMManager.h"
#endif
#include "nsIProperties.h"

#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIMemory.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIGenericFactory.h"
#include "nsGfxCIID.h"
#include "nsSpecialSystemDirectory.h"

#include "nsISound.h"
#include "nsIFileSpecWithUI.h"

#include "prprf.h"
#include "prmem.h"
#include "prlog.h"	// PR_ASSERT

#if defined(XP_OS2)
    #define WIDGET_DLL      "WDGTOS2"
    #define GFXWIN_DLL      "GFX_OS2"
    #define VIEW_DLL        "NGVIEW"
    #define WEB_DLL         "WEBSHELL"
    #define DOM_DLL         "JSDOM"
    #define CAPS_DLL        "CAPS"
    #define LIVECONNECT_DLL "JSJ"
    #define OJI_DLL         "OJI"
#elif defined(XP_PC)
    #define WIDGET_DLL "gkwidget.dll"
    #define GFXWIN_DLL "gkgfxwin.dll"
    #define VIEW_DLL   "gkview.dll"
    #define DOM_DLL    "jsdom.dll"
    #define CAPS_DLL   "caps.dll"
    #define LIVECONNECT_DLL    "jsj3250.dll"
    #define OJI_DLL    "oji.dll"
#elif defined(XP_MAC)
    #define WIDGET_DLL    "WIDGET_DLL"
    #define GFXWIN_DLL    "GFXWIN_DLL"
    #define VIEW_DLL        "VIEW_DLL"
    #define WEB_DLL            "WEB_DLL"
    #define DOM_DLL        "DOM_DLL"
    #define CAPS_DLL    "CAPS_DLL"
    #define LIVECONNECT_DLL "LIVECONNECT_DLL"
    #define OJI_DLL        "OJI_DLL"
#else
    #define VIEW_DLL   "libgkview"MOZ_DLL_SUFFIX
    #define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
    #define CAPS_DLL   "libcaps"MOZ_DLL_SUFFIX
    #define LIVECONNECT_DLL "libliveconnect"MOZ_DLL_SUFFIX
    #define OJI_DLL    "liboji"MOZ_DLL_SUFFIX
#endif

// Class ID's

// WIDGET
#ifndef XP_UNIX
static NS_DEFINE_IID(kCLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCVScrollbarCID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCHScrollbarCID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCDialogCID, NS_DIALOG_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkitCID, NS_TOOLKIT_CID);
static NS_DEFINE_IID(kClipboardCID,            NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kDataFlavorCID,           NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_IID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_IID(kCFontRetrieverServiceCID,  NS_FONTRETRIEVERSERVICE_CID);
static NS_DEFINE_IID(kCTimerCID,            NS_TIMER_CID);
static NS_DEFINE_IID(kCTimerManagerCID,            NS_TIMERMANAGER_CID);
static NS_DEFINE_IID(kSoundCID,            NS_SOUND_CID);
static NS_DEFINE_CID(kFileSpecWithUICID, NS_FILESPECWITHUI_CID);
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kCFilePickerCID, NS_FILEPICKER_CID);
static NS_DEFINE_IID(kCPopUpCID,NS_POPUP_CID);

// widgets
static NS_DEFINE_IID(kCLabelCID, NS_LABEL_CID);
static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
#endif

// unneeded widgets
#if 0
static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
#endif

// GFXWIN
#ifndef XP_UNIX
static NS_DEFINE_CID(kCRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_CID(kCDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kCFontMetricsCID, NS_FONT_METRICS_CID);
static NS_DEFINE_CID(kCFontEnumeratorCID, NS_FONT_ENUMERATOR_CID);
static NS_DEFINE_CID(kCImageCID, NS_IMAGE_CID);
static NS_DEFINE_CID(kCRegionCID, NS_REGION_CID);
static NS_DEFINE_CID(kCScriptableRegionCID, NS_SCRIPTABLE_REGION_CID);
static NS_DEFINE_CID(kCBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_CID(kCDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_CID(kCDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_CID(kImageManagerCID, NS_IMAGEMANAGER_CID);
static NS_DEFINE_CID(kScreenManagerCID, NS_SCREENMANAGER_CID);
#endif

// VIEW
static NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kCScrollPortViewCID, NS_SCROLL_PORT_VIEW_CID);

// DOM
static NS_DEFINE_IID(kCDOMScriptObjectFactory, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_IID(kCScriptNameSetRegistry, NS_SCRIPT_NAMESET_REGISTRY_CID);


// OJI
#ifdef OJI
static NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);
static NS_DEFINE_CID(kCJVMManagerCID, NS_JVMMANAGER_CID);
#endif

extern "C" void
NS_SetupRegistry()
{

#ifdef XP_UNIX  
#undef WIDGET_DLL
#undef GFXWIN_DLL
#endif /* defined(XP_UNIX) */

  // WIDGET
#ifndef XP_UNIX
  nsComponentManager::RegisterComponentLib(kCLookAndFeelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCWindowCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCVScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCHScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDialogCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFileWidgetCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCChildCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCPopUpCID,NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCAppShellCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCToolkitCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kClipboardCID,            "Clipboard", "component://netscape/widget/clipboard", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTransferableCID,        "Transferable", "component://netscape/widget/transferable", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kDataFlavorCID,           NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCXIFFormatConverterCID,  NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDragServiceCID,          "Drag Service", "component://netscape/widget/dragservice", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontRetrieverServiceCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kSoundCID,   "Sound Services", "component://netscape/sound", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kFileSpecWithUICID, NS_FILESPECWITHUI_CLASSNAME, 
                                           NS_FILESPECWITHUI_PROGID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFilePickerCID, "FilePicker", "component://mozilla/filepicker", WIDGET_DLL, PR_FALSE, PR_FALSE);
#ifdef XP_PC
  nsComponentManager::RegisterComponentLib(kCTimerCID, "Timer", "component://netscape/timer", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTimerManagerCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

#ifdef XP_MAC
  nsComponentManager::RegisterComponentLib(kCTimerCID, "Timer", "component://netscape/timer", WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

  // WIDGETS
  nsComponentManager::RegisterComponentLib(kCLabelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTextFieldCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCCheckButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

  // MAC ONLY WIDGETS
#ifdef XP_MAC
  static NS_DEFINE_IID(kCMenuBarCID,                NS_MENUBAR_CID);
  static NS_DEFINE_IID(kCMenuCID,                   NS_MENU_CID);
  static NS_DEFINE_IID(kCMenuItemCID,               NS_MENUITEM_CID);
  static NS_DEFINE_IID(kCContextMenuCID,            NS_CONTEXTMENU_CID);

  nsComponentManager::RegisterComponentLib(kCMenuBarCID,       NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuCID,          NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuItemCID,      NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

  // UNNEEDED WIDGETS
#if 0
  nsComponentManager::RegisterComponentLib(kCComboBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCContextMenuCID,   NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCListBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCRadioButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTextAreaCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

  // GFXWIN
#ifndef XP_UNIX
  nsComponentManager::RegisterComponentLib(kCRenderingContextCID, "Rendering Context", "component://netscape/gfx/renderingcontext", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextCID, "Device Context", "component://netscape/gfx/devicecontext", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontMetricsCID, "Font Metrics", "component://netscape/gfx/fontmetrics", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontEnumeratorCID, "Font Enumerator", "component://netscape/gfx/fontenumerator", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCImageCID, "Image", "component://netscape/gfx/image", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCRegionCID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScriptableRegionCID, "Region", "component://netscape/gfx/region", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCBlenderCID, "Blender", "component://netscape/gfx/blender", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextSpecCID, "Device Context Spec", "component://netscape/gfx/devicecontextspec", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextSpecFactoryCID, "Device Context Spec Factory", "component://netscape/gfx/devicecontextspecfactory", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kImageManagerCID, "Image Manager", "component://netscape/gfx/imagemanager", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kScreenManagerCID, "Screen Manager", "component://netscape/gfx/screenmanager", GFXWIN_DLL, PR_FALSE, PR_FALSE);
#endif

  // VIEW
  nsComponentManager::RegisterComponentLib(kCViewManagerCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScrollingViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScrollPortViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);

  // DOM
  nsComponentManager::RegisterComponentLib(kCDOMScriptObjectFactory, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScriptNameSetRegistry, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);

#ifdef OJI
  nsComponentManager::RegisterComponentLib(kCLiveconnectCID, "LiveConnect", "component://netscape/javascript/liveconnect", LIVECONNECT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCJVMManagerCID, "JVMManager", "component://netscape/oji/jvmmanager", OJI_DLL, PR_FALSE, PR_FALSE);
#endif
}
