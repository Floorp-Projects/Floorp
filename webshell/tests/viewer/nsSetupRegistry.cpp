/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#define NS_IMPL_IDS
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"

#include "nsIDocumentLoader.h"

#include "nsDOMCID.h"
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

#include "prprf.h"
#include "prmem.h"
#include "prlog.h"	// PR_ASSERT

#if defined(XP_OS2)
    #define WIDGET_DLL      "WDGTOS2"
    #define GFXWIN_DLL      "GFX_OS2"
#elif defined(XP_PC)
    #define WIDGET_DLL "gkwidget.dll"
    #define GFXWIN_DLL "gkgfxwin.dll"
#elif defined(XP_MAC)
    #define WIDGET_DLL    "WIDGET_DLL"
    #define GFXWIN_DLL    "GFXWIN_DLL"
#endif

// Class ID's

// WIDGET
#if !defined(XP_UNIX) && !defined(XP_OS2) && !defined(XP_WIN)
static NS_DEFINE_IID(kCLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCVScrollbarCID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCHScrollbarCID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCDialogCID, NS_DIALOG_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkitCID, NS_TOOLKIT_CID);
static NS_DEFINE_IID(kClipboardCID,            NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kClipboardHelperCID, NS_CLIPBOARDHELPER_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kDataFlavorCID,           NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCHTMLFormatConverterCID, NS_HTMLFORMATCONVERTER_CID);
static NS_DEFINE_IID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_IID(kCTimerCID,            NS_TIMER_CID);
static NS_DEFINE_IID(kCTimerManagerCID,            NS_TIMERMANAGER_CID);
static NS_DEFINE_IID(kSoundCID,            NS_SOUND_CID);
static NS_DEFINE_IID(kCFilePickerCID, NS_FILEPICKER_CID);
static NS_DEFINE_IID(kCPopUpCID,NS_POPUP_CID);

#ifdef IBMBIDI
static NS_DEFINE_IID(kCBidiKeyboardCID, NS_BIDIKEYBOARD_CID);
#endif

// widgets
static NS_DEFINE_IID(kCLabelCID, NS_LABEL_CID);
static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
#endif

// GFXWIN
#if !defined(XP_UNIX) && !defined(XP_OS2) && !defined(XP_WIN)
static NS_DEFINE_CID(kCRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_CID(kCDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kCFontMetricsCID, NS_FONT_METRICS_CID);
static NS_DEFINE_CID(kCFontEnumeratorCID, NS_FONT_ENUMERATOR_CID);
static NS_DEFINE_CID(kCFontListCID, NS_FONTLIST_CID);
static NS_DEFINE_CID(kCImageCID, NS_IMAGE_CID);
static NS_DEFINE_CID(kCRegionCID, NS_REGION_CID);
static NS_DEFINE_CID(kCScriptableRegionCID, NS_SCRIPTABLE_REGION_CID);
static NS_DEFINE_CID(kCBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_CID(kCDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_CID(kCDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_CID(kImageManagerCID, NS_IMAGEMANAGER_CID);
static NS_DEFINE_CID(kScreenManagerCID, NS_SCREENMANAGER_CID);
static NS_DEFINE_CID(kPrintOptionsCID,NS_PRINTOPTIONS_CID);
#endif

extern "C" void
NS_SetupRegistry()
{

#ifdef XP_UNIX  
#undef WIDGET_DLL
#undef GFXWIN_DLL
#endif /* defined(XP_UNIX) */

  // WIDGET
#if !defined(XP_UNIX) && !defined(XP_OS2) && !defined(XP_WIN)
  nsComponentManager::RegisterComponentLib(kCLookAndFeelCID, NULL, "@mozilla.org/widget/lookandfeel;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCWindowCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCVScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCHScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDialogCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCChildCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCPopUpCID,NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCAppShellCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCToolkitCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kClipboardCID,            "Clipboard", "@mozilla.org/widget/clipboard;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kClipboardHelperCID, "Clipboard Helper", "@mozilla.org/widget/clipboardhelper;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTransferableCID,        "Transferable", "@mozilla.org/widget/transferable;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kDataFlavorCID,           NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCHTMLFormatConverterCID,  NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDragServiceCID,          "Drag Service", "@mozilla.org/widget/dragservice;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kSoundCID,   "Sound Services", "@mozilla.org/sound;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFilePickerCID, "FilePicker", "@mozilla.org/filepicker;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
#ifdef XP_PC
  nsComponentManager::RegisterComponentLib(kCTimerCID, "Timer", "@mozilla.org/timer;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTimerManagerCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

#ifdef IBMBIDI
  nsComponentManager::RegisterComponentLib(kCBidiKeyboardCID, "Bidi Keyboard", "@mozilla.org/widget/bidikeyboard;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

#ifdef XP_MAC
  nsComponentManager::RegisterComponentLib(kCTimerCID, "Timer", "@mozilla.org/timer;1", WIDGET_DLL, PR_FALSE, PR_FALSE);
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

  nsComponentManager::RegisterComponentLib(kCMenuBarCID,       NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuCID,          NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuItemCID,      NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

  // GFXWIN
#if !defined(XP_UNIX) && !defined(XP_OS2) && !defined(XP_WIN)
  nsComponentManager::RegisterComponentLib(kCRenderingContextCID, "Rendering Context", "@mozilla.org/gfx/renderingcontext;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextCID, "Device Context", "@mozilla.org/gfx/devicecontext;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontMetricsCID, "Font Metrics", "@mozilla.org/gfx/fontmetrics;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontEnumeratorCID, "Font Enumerator", "@mozilla.org/gfx/fontenumerator;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontListCID, "Font List", "@mozilla.org/gfx/fontlist;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCImageCID, "Image", "@mozilla.org/gfx/image;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCRegionCID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScriptableRegionCID, "Region", "@mozilla.org/gfx/region;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCBlenderCID, "Blender", "@mozilla.org/gfx/blender;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextSpecCID, "Device Context Spec", "@mozilla.org/gfx/devicecontextspec;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextSpecFactoryCID, "Device Context Spec Factory", "@mozilla.org/gfx/devicecontextspecfactory;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kImageManagerCID, "Image Manager", "@mozilla.org/gfx/imagemanager;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kScreenManagerCID, "Screen Manager", "@mozilla.org/gfx/screenmanager;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kPrintOptionsCID, "PrintOptions", "@mozilla.org/gfx/printoptions;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);
#endif

}
