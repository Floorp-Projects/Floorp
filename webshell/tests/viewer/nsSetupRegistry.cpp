/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#define NS_IMPL_IDS
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsPluginsCID.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIThrobber.h"

#include "nsParserCIID.h"
#include "nsDOMCID.h"
#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#endif // NECKO
#ifdef OJI
#include "nsILiveconnect.h"
#endif
#include "nsIPluginManager.h"
#include "nsIProperties.h"

#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIAllocator.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIGenericFactory.h"
#include "nsGfxCIID.h"
#include "nsSpecialSystemDirectory.h"

#include "nsISound.h"
#include "nsIFileSpecWithUI.h"

#if defined(XP_UNIX) && !defined(MOZ_MONOLITHIC_TOOLKIT)
#include "nsIUnixToolkitService.h"
#endif

#include "prprf.h"
#include "prmem.h"
#include "prlog.h"	// PR_ASSERT

#ifdef XP_PC
    #define WIDGET_DLL "raptorwidget.dll"
    #define GFXWIN_DLL "raptorgfxwin.dll"
    #define VIEW_DLL   "raptorview.dll"
    #define WEB_DLL    "raptorweb.dll"
    #define PREF_DLL   "xppref32.dll"
    #define PARSER_DLL "raptorhtmlpars.dll"
    #define DOM_DLL    "jsdom.dll"
#ifdef NECKO
    #define NECKO_DLL "necko.dll"
#else
    #define NETLIB_DLL "netlib.dll"
#endif // NECKO
    #define PLUGIN_DLL "raptorplugin.dll"
    #define CAPS_DLL   "caps.dll"
    #define LIVECONNECT_DLL    "jsj3250.dll"
    #define OJI_DLL    "oji.dll"
#elif defined(XP_MAC)
    #define WIDGET_DLL    "WIDGET_DLL"
    #define GFXWIN_DLL    "GFXWIN_DLL"
    #define VIEW_DLL        "VIEW_DLL"
    #define WEB_DLL            "WEB_DLL"
    #define PREF_DLL        "PREF_DLL"
    #define PARSER_DLL    "PARSER_DLL"
    #define DOM_DLL        "DOM_DLL"
#ifdef NECKO
    #define NECKO_DLL "NECKO_DLL"
#else
    #define NETLIB_DLL    "NETLIB_DLL"
#endif // NECKO
    #define PLUGIN_DLL    "PLUGIN_DLL"
    #define CAPS_DLL    "CAPS_DLL"
    #define LIVECONNECT_DLL "LIVECONNECT_DLL"
    #define OJI_DLL        "OJI_DLL"
#else
    /** Currently CFLAGS  defines WIDGET_DLL and GFXWIN_DLL. If, for some 
      * reason, the cflags value doesn't get defined, use gtk, 
      * since that is the default.
     **/
    #ifndef WIDGET_DLL
    #define WIDGET_DLL "libwidget_gtk"MOZ_DLL_SUFFIX
    #endif
    #ifndef GFXWIN_DLL
    #define GFXWIN_DLL "libgfx_gtk"MOZ_DLL_SUFFIX
    #endif
    #define VIEW_DLL   "libraptorview"MOZ_DLL_SUFFIX
    #define WEB_DLL    "libraptorwebwidget"MOZ_DLL_SUFFIX
    #define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
    #define PARSER_DLL "libraptorhtmlpars"MOZ_DLL_SUFFIX
    #define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
#ifdef NECKO
    #define NECKO_DLL "libnecko"MOZ_DLL_SUFFIX
#else
    #define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#endif // NECKO
    #define PLUGIN_DLL "libraptorplugin"MOZ_DLL_SUFFIX
    #define CAPS_DLL   "libcaps"MOZ_DLL_SUFFIX
    #define LIVECONNECT_DLL "libliveconnect"MOZ_DLL_SUFFIX
    #define OJI_DLL    "liboji"MOZ_DLL_SUFFIX
#endif

// Class ID's

// WIDGET
static NS_DEFINE_IID(kCLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCVScrollbarCID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCHScrollbarCID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCDialogCID, NS_DIALOG_CID);
static NS_DEFINE_IID(kCLabelCID, NS_LABEL_CID);
static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kCPopUpCID,NS_POPUP_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkitCID, NS_TOOLKIT_CID);
static NS_DEFINE_IID(kClipboardCID,            NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kDataFlavorCID,           NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_IID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
//static NS_DEFINE_IID(kCFileListTransferableCID,  NS_FILELISTTRANSFERABLE_CID);
static NS_DEFINE_IID(kCFontRetrieverServiceCID,  NS_FONTRETRIEVERSERVICE_CID);
static NS_DEFINE_IID(kCMenuBarCID,                NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenuCID,                   NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItemCID,               NS_MENUITEM_CID);
static NS_DEFINE_IID(kCContextMenuCID,            NS_CONTEXTMENU_CID);
//static NS_DEFINE_IID(kCXULCommandCID,             NS_XULCOMMAND_CID);
static NS_DEFINE_IID(kSoundCID,            NS_SOUND_CID);
static NS_DEFINE_CID(kFileSpecWithUICID, NS_FILESPECWITHUI_CID);

// GFXWIN
static NS_DEFINE_CID(kCRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_CID(kCDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kCFontMetricsCID, NS_FONT_METRICS_CID);
static NS_DEFINE_CID(kCImageCID, NS_IMAGE_CID);
static NS_DEFINE_CID(kCRegionCID, NS_REGION_CID);
static NS_DEFINE_CID(kCBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_CID(kCDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_CID(kCDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);


// VIEW
static NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);

// WEB
static NS_DEFINE_IID(kCWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kCDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kCThrobberCID, NS_THROBBER_CID);

// PREF
static NS_DEFINE_CID(kCPrefCID, NS_PREF_CID);

// PARSER
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);
static NS_DEFINE_CID(kCWellFormedDTDCID, NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kCNavDTDCID, NS_CNAVDTD_CID);

// DOM
static NS_DEFINE_IID(kCDOMScriptObjectFactory, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_IID(kCScriptNameSetRegistry, NS_SCRIPT_NAMESET_REGISTRY_CID);

// NETLIB
#ifndef NECKO
static NS_DEFINE_CID(kCNetServiceCID, NS_NETSERVICE_CID);
#else
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

// PLUGIN
static NS_DEFINE_IID(kCPluginHostCID, NS_PLUGIN_HOST_CID);
static NS_DEFINE_CID(kCPluginManagerCID,          NS_PLUGINMANAGER_CID);
#ifdef OJI
static NS_DEFINE_CID(kCLiveconnectCID,             NS_CLIVECONNECT_CID);
#endif

extern "C" void
NS_SetupRegistry()
{
#if defined(XP_UNIX) && !defined(MOZ_MONOLITHIC_TOOLKIT)
  // On unix, the widget and gfx toolkit are not linked into the app.
  //
  // Instead, they are loaded at runtime courtesy of xpcom.
  //
  // Loading the toolkit specific dlls at runtime has many benefits:
  //
  // o Simplifies linking of the "app" since it no longer needs to 
  //   pull in the toolkit world.
  //
  // o Makes it possible to embed the xlib gfx/widget backends into
  //   other high level x toolkit such as motif and gtk.  This is
  //   highly desirable in the long run as a means to increase code
  //   reuse and eyeball focus.
  //
  // o Makes it possible to run X Mozilla using different toolkits
  //   without having to hack the application bits.  This in turn
  //   simplifies everyone's life, since only one binary is needed.
  //   The only platform specific bits are:
  //
  //   mozilla/gfx/src
  //   mozilla/widget/src/
  //   mozilla/widget/timer/
  //
  // o It bypasses (yeah right) the toolkit inquisitions and crusades.
  // 
  // o It makes you breakfast, lunch and dinner.
  //


  //
  // Note1:
  //
  // The following code assumes that:
  //
  // + Some kind of auto registration has occurred for components.
  //   For example, NS_AutoRegister() or nsComponentManager::AutoRegister().
  //
  //   -or-
  //
  // + The "app" was installed and a registry with information on the
  //   toolkit_service component has been populated.  The master plan
  //   is that this occurs at install time.  Otherwise bad things will
  //   happen.
  //
  // During the development of mozilla, this usually occurs in the
  // main() of the "app" (viewer/apprunner).  It is always the first
  // thing that happens on the NS_SetupRegistry() function.
  //
  // If for some reason that changes in the future, the following code
  // might have to be moved.
  //
  // Note2:
  //
  // The WIDGET_DLL and GFX_DLL macros will be redefined from the 
  // hard coded values set in this file above.  They will point
  // to strings valid only in the scope of this function.  
  //
  // If for some reason, the nsComponentManager::RegisterComponentLib()
  // calls below are moved to a different scope, this unix specific code
  // will have to deal.

  static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

  nsresult   rv;

  nsIUnixToolkitService * unixToolkitService = nsnull;
    
  rv = nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
                                          nsnull,
                                          nsIUnixToolkitService::GetIID(),
                                          (void **) &unixToolkitService);
  
  NS_ASSERTION(rv == NS_OK,"Cannot obtain unix toolkit service.");


  nsAutoString unixToolkitName("error");
  nsAutoString unixWidgetDllName("error");
  nsAutoString unixGfxDllName("error");
  
  if (NS_OK == rv && nsnull != unixToolkitService)
  {
    nsresult rv2;

    rv2 = unixToolkitService->GetToolkitName(unixToolkitName);

    NS_ASSERTION(rv2 == NS_OK,"Cannot get unix toolkit name context.");

    rv2 = unixToolkitService->GetWidgetDllName(unixWidgetDllName);

    NS_ASSERTION(rv2 == NS_OK,"Cannot get unix toolkit widget dll name.");

    rv2 = unixToolkitService->GetGfxDllName(unixGfxDllName);

    NS_ASSERTION(rv2 == NS_OK,"Cannot get unix toolkit gfx dll name.");

    NS_RELEASE(unixToolkitService);
  }

#ifdef NS_DEBUG
  printf("NS_SetupRegistry() MOZ_TOOLKIT=%s, WIDGET_DLL=%s, GFX_DLL=%s\n",
         (const char *) nsAutoCString(unixToolkitName),
         (const char *) nsAutoCString(unixWidgetDllName),
         (const char *) nsAutoCString(unixGfxDllName));
#endif
  
#undef WIDGET_DLL
#undef GFXWIN_DLL

#define WIDGET_DLL (const char *) nsAutoCString(unixWidgetDllName)
#define GFXWIN_DLL (const char *) nsAutoCString(unixGfxDllName)

#endif /* defined(XP_UNIX) && !defined(MOZ_MONOLITHIC_TOOLKIT) */


  


  // WIDGET
  nsComponentManager::RegisterComponentLib(kCLookAndFeelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCWindowCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCVScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCHScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDialogCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCLabelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCComboBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFileWidgetCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCListBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCRadioButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTextAreaCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTextFieldCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCCheckButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCChildCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCPopUpCID,NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCAppShellCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCToolkitCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kClipboardCID,            NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCTransferableCID,        "Transferable", "component://netscape/widget/transferable", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kDataFlavorCID,           NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCXIFFormatConverterCID,  NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDragServiceCID,          "Drag Service", "component://netscape/widget/dragservice", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontRetrieverServiceCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuBarCID,       NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuCID,          NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCMenuItemCID,      NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCContextMenuCID,   NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kSoundCID,   "Sound Services", "component://netscape/sound", WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kFileSpecWithUICID,   NS_FILESPECWITHUI_CLASSNAME, NS_FILESPECWITHUI_PROGID, WIDGET_DLL, PR_FALSE, PR_FALSE);

  // GFXWIN
  nsComponentManager::RegisterComponentLib(kCRenderingContextCID, "Rendering Context", "component://netscape/gfx/renderingcontext", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextCID, "Device Context", "component://netscape/gfx/devicecontext", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCFontMetricsCID, "Font Metrics", "component://netscape/gfx/fontmetrics", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCImageCID, "Image", "component://netscape/gfx/image", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCRegionCID, "Region", "component://netscape/gfx/region", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCBlenderCID, "Blender", "component://netscape/gfx/blender", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextSpecCID, "Device Context Spec", "component://netscape/gfx/devicecontextspec", GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDeviceContextSpecFactoryCID, "Device Context Spec Factory", "component://netscape/gfx/devicecontextspecfactory", GFXWIN_DLL, PR_FALSE, PR_FALSE);

  // VIEW
  nsComponentManager::RegisterComponentLib(kCViewManagerCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScrollingViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);

  // WEB
  nsComponentManager::RegisterComponentLib(kCWebShellCID, NULL, NULL, WEB_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCDocLoaderServiceCID, NULL, NULL, WEB_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCThrobberCID, NULL, NULL, WEB_DLL, PR_FALSE, PR_FALSE);

  // PREF
  nsComponentManager::RegisterComponentLib(kCPrefCID, "Preferences Services", "component://netscape/preferences", PREF_DLL, PR_FALSE, PR_FALSE);

  // PARSER
  nsComponentManager::RegisterComponentLib(kCParserCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCWellFormedDTDCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCNavDTDCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);

  // DOM
  nsComponentManager::RegisterComponentLib(kCDOMScriptObjectFactory, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScriptNameSetRegistry, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);

  // NETLIB
#ifndef NECKO
  nsComponentManager::RegisterComponentLib(kCNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#else
//  nsComponentManager::RegisterComponentLib(kIOServiceCID, NULL, NULL, NECKO_DLL, PR_FALSE, PR_FALSE);
#endif // NECKO

  // PLUGIN
  nsComponentManager::RegisterComponentLib(kCPluginHostCID, NULL, NULL, PLUGIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCPluginManagerCID, NULL, NULL, PLUGIN_DLL, PR_FALSE, PR_FALSE);

#ifdef OJI
  nsComponentManager::RegisterComponentLib(kCLiveconnectCID, NULL, NULL, LIVECONNECT_DLL, PR_FALSE, PR_FALSE);
#endif
}

