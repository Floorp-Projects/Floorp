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
#include "nsGfxCIID.h"
#include "nsViewsCID.h"
#include "nsPluginsCID.h"
#include "nsRDFCID.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIThrobber.h"

#include "nsXPComCIID.h"
#include "nsParserCIID.h"
#include "nsDOMCID.h"
#include "nsLayoutCID.h"
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

#include "nsIStringBundle.h"
#include "nsUnicharUtilCIID.h"
#include "nsIProperties.h"
#include "nsCollationCID.h"
#include "nsDateTimeFormatCID.h"
#include "nsLocaleCID.h"
#include "nsLWBrkCIID.h"

#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIProfile.h"
#include "nsIAllocator.h"
#include "nsIGenericFactory.h"

#include "nsSpecialSystemDirectory.h"    // For exe dir
#include "prprf.h"
#include "prmem.h"

#ifdef XP_PC
    #define XPCOM_DLL  "xpcom32.dll"
    #define WIDGET_DLL "raptorwidget.dll"
    #define GFXWIN_DLL "raptorgfxwin.dll"
    #define VIEW_DLL   "raptorview.dll"
    #define WEB_DLL    "raptorweb.dll"
    #define PLUGIN_DLL "raptorplugin.dll"
    #define PREF_DLL   "xppref32.dll"
    #define PARSER_DLL "raptorhtmlpars.dll"
    #define DOM_DLL    "jsdom.dll"
    #define LAYOUT_DLL "raptorhtml.dll"
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
    #define BASE_DLL   "raptorbase.dll"
    #define NSLOCALE_DLL "nslocale.dll"
    #define LWBRK_DLL "lwbrk.dll"
    #define PROFILE_DLL "xpprofile32.dll"
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
    #define LAYOUT_DLL    "LAYOUT_DLL"
    #define NETLIB_DLL    "NETLIB_DLL"
    #define EDITOR_DLL    "ENDER_DLL"
    #define RDF_DLL            "RDF_DLL"
    #define UCONV_DLL    "UCONV_DLL"
    #define UCVLATIN_DLL "UCVLATIN_DLL"
    #define UCVJA_DLL    "UCVJA_DLL"
    #define UCVJA2_DLL   "UCVJA2_DLL"
    #define STRRES_DLL   "STRRES_DLL"
    #define UNICHARUTIL_DLL   "UNICHARUTIL_DLL"
    #define BASE_DLL   "base.shlb"
    #define NSLOCALE_DLL "NSLOCALE_DLL"
    #define LWBRK_DLL "LWBRK_DLL"
    #define PROFILE_DLL "PROFILE_DLL"
#else
    #define XPCOM_DLL  "libxpcom.so"
    /** Currently CFLAGS  defines WIDGET_DLL and GFXWIN_DLL. If, for some 
      * reason, the cflags value doesn't get defined, use gtk, 
      * since that is the default.
     **/
    #ifndef WIDGET_DLL
    #define WIDGET_DLL "libwidgetgtk.so"
    #endif
    #ifndef GFXWIN_DLL
    #define GFXWIN_DLL "libgfxgtk.so"
    #endif
    #define VIEW_DLL   "libraptorview.so"
    #define WEB_DLL    "libraptorwebwidget.so"
    #define PLUGIN_DLL "libraptorplugin.so"
    #define CAPS_DLL   "libcaps.so"
    #define LIVECONNECT_DLL "libliveconnect.so"
    #define OJI_DLL    "liboji.so"
    #define PREF_DLL   "libpref.so"
    #define PARSER_DLL "libraptorhtmlpars.so"
    #define DOM_DLL    "libjsdom.so"
    #define LAYOUT_DLL "libraptorhtml.so"
    #define NETLIB_DLL "libnetlib.so"
    #define EDITOR_DLL "libender.so"
    #define RDF_DLL    "librdf.so"
    #define UCONV_DLL    "libuconv.so"
    #define UCVLATIN_DLL "libucvlatin.so"
    #define UCVJA_DLL    "libucvja.so"
    #define UCVJA2_DLL   "libucvja2.so"
    #define STRRES_DLL   "libstrres.so"
    #define UNICHARUTIL_DLL   "libunicharutil.so"
    #define BASE_DLL     "libraptorbase.so"
    #define NSLOCALE_DLL "libnslocale.so"
    #define LWBRK_DLL "liblwbrk.so"
    #define PROFILE_DLL "libprofile.so"
#endif

// Class ID's
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
static NS_DEFINE_IID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
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
static NS_DEFINE_IID(kCHTMLDocument, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kCXMLDocument, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kCImageDocument, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kCHTMLImageElement, NS_HTMLIMAGEELEMENT_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID, NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);
static NS_DEFINE_IID(kObserverCID, NS_OBSERVER_CID);

#if defined(NS_USING_PROFILES)
static NS_DEFINE_IID(kProfileCID, NS_PROFILE_CID);
#endif


#ifdef NEW_CLIPBOARD_SUPPORT
static NS_DEFINE_IID(kClipboardCID,            NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCGenericTransferableCID, NS_GENERICTRANSFERABLE_CID);
static NS_DEFINE_IID(kDataFlavorCID,           NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_IID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
#endif

#if 0    // autoregistration now works on all platforms, and RDF self-registers, so commenting out
#if defined(XP_MAC) || defined (XP_UNIX)
static NS_DEFINE_CID(kRDFBookMarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFHTMLBuilderCID,        NS_RDFHTMLBUILDER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,        NS_RDFTREEBUILDER_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,        NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,      NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,         NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kXULDataSourceCID,         NS_XULDATASOURCE_CID);
static NS_DEFINE_CID(kXULDocumentCID,           NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULContentSinkCID,        NS_XULCONTENTSINK_CID);
#endif
#endif

static NS_DEFINE_CID(kCSSParserCID,             NS_CSSPARSER_CID);
static NS_DEFINE_CID(kPresShellCID,             NS_PRESSHELL_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,        NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID,     NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kTextNodeCID,              NS_TEXTNODE_CID);
static NS_DEFINE_CID(kSelectionCID,             NS_SELECTION_CID);
static NS_DEFINE_CID(kRangeCID,                 NS_RANGE_CID);
static NS_DEFINE_CID(kRangeListCID,                NS_RANGELIST_CID);
static NS_DEFINE_IID(kContentIteratorCID,       NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID,       NS_SUBTREEITERATOR_CID);
static NS_DEFINE_CID(kFrameUtilCID,             NS_FRAME_UTIL_CID);
static NS_DEFINE_CID(kCEventListenerManagerCID, NS_EVENTLISTENERMANAGER_CID);

static NS_DEFINE_CID(kCPluginManagerCID,          NS_PLUGINMANAGER_CID);
#ifdef OJI
static NS_DEFINE_CID(kCapsManagerCID,             NS_CCAPSMANAGER_CID);
static NS_DEFINE_CID(kLiveconnectCID,             NS_CLIVECONNECT_CID);
static NS_DEFINE_CID(kJVMManagerCID,              NS_JVMMANAGER_CID);
#endif

static NS_DEFINE_IID(kCMenuBarCID,                NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenuCID,                   NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItemCID,               NS_MENUITEM_CID);
//static NS_DEFINE_IID(kCXULCommandCID,             NS_XULCOMMAND_CID);


static NS_DEFINE_IID(kStringBundleServiceCID,     NS_STRINGBUNDLESERVICE_CID);

static NS_DEFINE_IID(kUnicharUtilCID,             NS_UNICHARUTIL_CID);

static NS_DEFINE_IID(kCollationCID,               NS_COLLATION_CID);
static NS_DEFINE_IID(kCollationFactoryCID,        NS_COLLATIONFACTORY_CID); // do we need this ???
static NS_DEFINE_IID(kDateTimeFormatCID,          NS_DATETIMEFORMAT_CID);
static NS_DEFINE_IID(kLocaleCID,                  NS_LOCALE_CID);
static NS_DEFINE_IID(kLocaleFactoryCID,           NS_LOCALEFACTORY_CID); // do we need this ???

static NS_DEFINE_IID(kLWBrkCID,                   NS_LWBRK_CID); 

extern "C" void
NS_SetupRegistry()
{
  // Autoregistration happens here. The rest of RegisterComponent() calls should happen
  // only for dlls not in the components directory.

  // Create exeDir/"components"
  nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
  sysdir += "components";
  const char *componentsDir = sysdir.GetCString(); // native path
  if (componentsDir != NULL)
  {
#ifdef XP_PC
      /* The PC version of the directory from filePath is of the form
       *    /y|/moz/mozilla/dist/bin/components
       * We need to remove the initial / and change the | to :
       * for all this to work with NSPR.      
       */
#endif /* XP_PC */
      printf("nsComponentManager: Using components dir: %s\n", componentsDir);

#ifdef XP_MAC
      nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);
#else
      nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, componentsDir);
#endif    /* XP_MAC */
      // XXX Look for user specific components
      // XXX UNIX: ~/.mozilla/components
  }

  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kAllocatorCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGenericFactoryCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
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
  nsComponentManager::RegisterComponent(kCHTMLDocument, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCXMLDocument, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCImageDocument, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCHTMLImageElement, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kNameSpaceManagerCID, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCEventListenerManagerCID, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kObserverServiceCID, NULL, NULL, BASE_DLL,PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kObserverCID, NULL, NULL, BASE_DLL,PR_FALSE, PR_FALSE);

#if defined(NS_USING_PROFILES)
  nsComponentManager::RegisterComponent(kProfileCID, NULL, NULL, PROFILE_DLL, PR_FALSE, PR_FALSE);
#endif

#if 0    // autoregistration now works on all platforms, and RDF self-registers, so commenting out
#if defined(XP_MAC) || defined (XP_UNIX)
  nsComponentManager::RegisterComponent(kRDFBookMarkDataSourceCID, NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFCompositeDataSourceCID, NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFHTMLBuilderCID,        NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFInMemoryDataSourceCID, NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFServiceCID,            NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFTreeBuilderCID,        NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFContentSinkCID,        NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFXMLDataSourceCID,      NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRDFXULBuilderCID,         NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kXULDataSourceCID,         NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kXULDocumentCID,           NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kXULContentSinkCID,        NULL, NULL, RDF_DLL, PR_FALSE, PR_FALSE);
#endif
#endif

#ifdef NEW_CLIPBOARD_SUPPORT
  nsComponentManager::RegisterComponent(kClipboardCID,            NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCGenericTransferableCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kDataFlavorCID,           NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCXIFFormatConverterCID,   NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCDragServiceCID,         NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
#endif

  nsComponentManager::RegisterComponent(kCSSParserCID,      NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kPresShellCID,      NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kHTMLStyleSheetCID, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kHTMLCSSStyleSheetCID, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kTextNodeCID,       NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kSelectionCID,      NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRangeCID,            NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRangeListCID,        NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kContentIteratorCID,NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kSubtreeIteratorCID,NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kFrameUtilCID,      NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCharsetConverterManagerCID,      NULL, NULL, UCONV_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCharsetAliasCID,      NULL, NULL, UCONV_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kLatin1ToUnicodeCID,      NULL, NULL, UCVLATIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kISO88597ToUnicodeCID,    NULL, NULL, UCVLATIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCP1253ToUnicodeCID,      NULL, NULL, UCVLATIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kSJIS2UnicodeCID,         NULL, NULL, UCVJA_DLL, PR_FALSE, PR_FALSE);
  // nsComponentManager::RegisterComponent(kEUCJPToUnicodeCID,       NULL, NULL, UCVJA2_DLL, PR_FALSE, PR_FALSE);
  // nsComponentManager::RegisterComponent(kISO2022JPToUnicodeCID,   NULL, NULL, UCVJA2_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kPlatformCharsetCID,      NULL, NULL, UCONV_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kStringBundleServiceCID,  NULL, NULL, STRRES_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kUnicharUtilCID,          NULL, NULL, UNICHARUTIL_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kPropertiesCID,           NULL, NULL, BASE_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCollationCID,            NULL, NULL, NSLOCALE_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCollationFactoryCID,     NULL, NULL, NSLOCALE_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kDateTimeFormatCID,       NULL, NULL, NSLOCALE_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kLocaleCID,               NULL, NULL, NSLOCALE_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kLocaleFactoryCID,        NULL, NULL, NSLOCALE_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kLWBrkCID,        NULL, NULL, LWBRK_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCPluginManagerCID, NULL, NULL, PLUGIN_DLL,      PR_FALSE, PR_FALSE);
#ifdef OJI
  nsComponentManager::RegisterComponent(kCapsManagerCID, NULL, NULL, CAPS_DLL,          PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kLiveconnectCID, NULL, NULL, LIVECONNECT_DLL,   PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kJVMManagerCID,  NULL, NULL, OJI_DLL,           PR_FALSE, PR_FALSE);
#endif

  nsComponentManager::RegisterComponent(kCMenuBarCID,       NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCMenuCID,          NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCMenuItemCID,      NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  //nsComponentManager::RegisterComponent(kCXULCommandCID,    NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
}
