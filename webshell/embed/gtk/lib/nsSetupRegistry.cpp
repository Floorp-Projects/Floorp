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
#include "nsIPref.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsPluginsCID.h"
#include "nsRDFCID.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIThrobber.h"

#include "nsParserCIID.h"
#include "nsDOMCID.h"
#include "nsINetService.h"
#ifdef OJI
#include "nsICapsManager.h"
#include "nsILiveconnect.h"
#include "nsIJVMManager.h"
#endif
#include "nsIPluginManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#include "nsUCvLatinCID.h"
#include "nsUCVJACID.h"
// #include "nsUCVJA2CID.h"

#include "nsUnicharUtilCIID.h"
#include "nsIProperties.h"

#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIAllocator.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIGenericFactory.h"
#include "nsGfxCIID.h"

#include "prprf.h"
#include "prmem.h"

#ifdef XP_PC
    #define XPCOM_DLL  "xpcom.dll"
    #define WIDGET_DLL "raptorwidget.dll"
    #define GFXWIN_DLL "raptorgfxwin.dll"
    #define VIEW_DLL   "raptorview.dll"
    #define WEB_DLL    "raptorweb.dll"
    #define PLUGIN_DLL "raptorplugin.dll"
    #define PREF_DLL   "xppref32.dll"
    #define PARSER_DLL "raptorhtmlpars.dll"
    #define DOM_DLL    "jsdom.dll"
    #define NETLIB_DLL "netlib.dll"
    #define EDITOR_DLL "ender.dll"
    #define RDF_DLL    "rdf.dll"
    #define CAPS_DLL   "caps.dll"
    #define LIVECONNECT_DLL    "jsj3250.dll"
    #define OJI_DLL    "oji.dll"
    #define UCONV_DLL    "uconv.dll"
    #define UCVLATIN_DLL "ucvlatin.dll"
    #define UCVJA_DLL    "ucvja.dll"
    #define UCVJA2_DLL   "ucvja2.dll"
    #define STRRES_DLL   "strres.dll"
    #define UNICHARUTIL_DLL   "unicharutil.dll"
#elif defined(XP_MAC)
    #define XPCOM_DLL   "XPCOM_DLL"
    #define WIDGET_DLL    "WIDGET_DLL"
    #define GFXWIN_DLL    "GFXWIN_DLL"
    #define VIEW_DLL        "VIEW_DLL"
    #define WEB_DLL            "WEB_DLL"
    #define PLUGIN_DLL    "PLUGIN_DLL"
    #define CAPS_DLL    "CAPS_DLL"
    #define LIVECONNECT_DLL "LIVECONNECT_DLL"
    #define OJI_DLL        "OJI_DLL"
    #define PREF_DLL        "PREF_DLL"
    #define PARSER_DLL    "PARSER_DLL"
    #define DOM_DLL        "DOM_DLL"
    #define NETLIB_DLL    "NETLIB_DLL"
    #define EDITOR_DLL    "ENDER_DLL"
    #define RDF_DLL            "RDF_DLL"
    #define UCONV_DLL    "UCONV_DLL"
    #define UCVLATIN_DLL "UCVLATIN_DLL"
    #define UCVJA_DLL    "UCVJA_DLL"
    #define UCVJA2_DLL   "UCVJA2_DLL"
    #define STRRES_DLL   "STRRES_DLL"
    #define UNICHARUTIL_DLL   "UNICHARUTIL_DLL"
#else
    #define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
    /** Currently CFLAGS  defines WIDGET_DLL and GFXWIN_DLL. If, for some 
      * reason, the cflags value doesn't get defined, use gtk, 
      * since that is the default.
     **/
    #ifndef WIDGET_DLL
    #define WIDGET_DLL "libwidgetgtk"MOZ_DLL_SUFFIX
    #endif
    #ifndef GFXWIN_DLL
    #define GFXWIN_DLL "libgfxgtk"MOZ_DLL_SUFFIX
    #endif
    #define VIEW_DLL   "libraptorview"MOZ_DLL_SUFFIX
    #define WEB_DLL    "libraptorwebwidget"MOZ_DLL_SUFFIX
    #define PLUGIN_DLL "libraptorplugin"MOZ_DLL_SUFFIX
    #define CAPS_DLL   "libcaps"MOZ_DLL_SUFFIX
    #define LIVECONNECT_DLL "libliveconnect"MOZ_DLL_SUFFIX
    #define OJI_DLL    "liboji"MOZ_DLL_SUFFIX
    #define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
    #define PARSER_DLL "libraptorhtmlpars"MOZ_DLL_SUFFIX
    #define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
    #define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
    #define EDITOR_DLL "libender"MOZ_DLL_SUFFIX
    #define RDF_DLL    "librdf"MOZ_DLL_SUFFIX
    #define UCONV_DLL    "libuconv"MOZ_DLL_SUFFIX
    #define UCVLATIN_DLL "libucvlatin"MOZ_DLL_SUFFIX
    #define UCVJA_DLL    "libucvja"MOZ_DLL_SUFFIX
    #define UCVJA2_DLL   "libucvja2"MOZ_DLL_SUFFIX
    #define STRRES_DLL   "libstrres"MOZ_DLL_SUFFIX
    #define UNICHARUTIL_DLL   "libunicharutil"MOZ_DLL_SUFFIX
#endif

// Class ID's
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCDialogCID, NS_DIALOG_CID);
static NS_DEFINE_IID(kCLabelCID, NS_LABEL_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkitCID, NS_TOOLKIT_CID);
static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCCheckButtonIID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kCChildIID, NS_CHILD_CID);
static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
static NS_DEFINE_IID(kCRegionIID, NS_REGION_CID);
static NS_DEFINE_IID(kCBlenderIID, NS_BLENDER_CID);
static NS_DEFINE_IID(kCDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kCDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kCPluginHostCID, NS_PLUGIN_HOST_CID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);
static NS_DEFINE_CID(kWellFormedDTDCID, NS_WELLFORMEDDTD_CID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCDOMScriptObjectFactory, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_IID(kCScriptNameSetRegistry, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

static NS_DEFINE_IID(kClipboardCID,            NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kDataFlavorCID,           NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_IID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
//static NS_DEFINE_IID(kCFileListTransferableCID,  NS_FILELISTTRANSFERABLE_CID);
static NS_DEFINE_IID(kCFontRetrieverServiceCID,  NS_FONTRETRIEVERSERVICE_CID);

static NS_DEFINE_CID(kCPluginManagerCID,          NS_PLUGINMANAGER_CID);
#ifdef OJI
static NS_DEFINE_CID(kCapsManagerCID,             NS_CCAPSMANAGER_CID);
static NS_DEFINE_CID(kLiveconnectCID,             NS_CLIVECONNECT_CID);
static NS_DEFINE_CID(kJVMManagerCID,              NS_JVMMANAGER_CID);
#endif

static NS_DEFINE_IID(kCMenuBarCID,                NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenuCID,                   NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItemCID,               NS_MENUITEM_CID);
static NS_DEFINE_IID(kCContextMenuCID,            NS_CONTEXTMENU_CID);
//static NS_DEFINE_IID(kCXULCommandCID,             NS_XULCOMMAND_CID);


static NS_DEFINE_IID(kUnicharUtilCID,             NS_UNICHARUTIL_CID);

extern "C" void
NS_SetupRegistry()
{
  nsComponentManager::RegisterComponent(kLookAndFeelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCWindowIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCScrollbarIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCHScrollbarIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDialogCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCLabelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCComboBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCFileWidgetCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCListBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCRadioButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCTextAreaCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCTextFieldCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCCheckButtonIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCChildIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCAppShellCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCToolkitCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCRenderingContextIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDeviceContextIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCFontMetricsIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCImageIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCRegionIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCBlenderIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDeviceContextSpecCID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDeviceContextSpecFactoryCID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCViewManagerCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCScrollingViewCID, NULL, NULL, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kWebShellCID, NULL, NULL, WEB_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDocLoaderServiceCID, NULL, NULL, WEB_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kThrobberCID, NULL, NULL, WEB_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kPrefCID, NULL, NULL, PREF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCPluginHostCID, NULL, NULL, PLUGIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCParserCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kWellFormedDTDCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDOMScriptObjectFactory, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCScriptNameSetRegistry, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kClipboardCID,            NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCTransferableCID,        NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kDataFlavorCID,           NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCXIFFormatConverterCID,  NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCDragServiceCID,          NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  //nsComponentManager::RegisterComponent(kCFileListTransferableCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCFontRetrieverServiceCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);


  nsComponentManager::RegisterComponent(kUnicharUtilCID,          NULL, NULL, UNICHARUTIL_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCPluginManagerCID, NULL, NULL, PLUGIN_DLL,      PR_FALSE, PR_FALSE);
#ifdef OJI
  nsComponentManager::RegisterComponent(kCapsManagerCID, NULL, NULL, CAPS_DLL,          PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kLiveconnectCID, NULL, NULL, LIVECONNECT_DLL,   PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kJVMManagerCID,  NULL, NULL, OJI_DLL,           PR_FALSE, PR_FALSE);
#endif

  nsComponentManager::RegisterComponent(kCMenuBarCID,       NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCMenuCID,          NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCMenuItemCID,      NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCContextMenuCID,   NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
}
