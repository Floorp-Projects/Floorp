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
#include "nsRepository.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsViewsCID.h"
#include "nsPluginsCID.h"
#include "nsRDFCID.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIThrobber.h"

#include "nsParserCIID.h"
#include "nsDOMCID.h"
#include "nsLayoutCID.h"
#include "nsINetService.h"

#include "nsIEditor.h"

#include "nsIAppShellService.h"
/// #include "nsBrowserCIDs.h"

#ifdef XP_PC

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

#define APPSHELL_DLL "nsappshell.dll"
#define BROWSER_DLL  "nsbrowser.dll"

#else

#ifdef XP_MAC

#define WIDGET_DLL		"WIDGET_DLL"
#define GFXWIN_DLL		"GFXWIN_DLL"
#define VIEW_DLL			"VIEW_DLL"
#define WEB_DLL				"WEB_DLL"
#define PLUGIN_DLL		"PLUGIN_DLL"
#define PREF_DLL			"PREF_DLL"
#define PARSER_DLL		"PARSER_DLL"
#define DOM_DLL    		"DOM_DLL"
#define LAYOUT_DLL		"LAYOUT_DLL"
#define NETLIB_DLL		"NETLIB_DLL"
#define APPSHELL_DLL "APPSHELL_DLL"
//#define EDITOR_DLL	"EDITOR_DLL"	// temporary
#define RDF_DLL			"RDF_DLL"


#else

// XP_UNIX
#ifndef WIDGET_DLL
#define WIDGET_DLL "libwidgetgtk.so"
#endif
#ifndef GFXWIN_DLL
#define GFXWIN_DLL "libgfxgtk.so"
#endif
#define VIEW_DLL   "libraptorview.so"
#define WEB_DLL    "libraptorwebwidget.so"
#define PLUGIN_DLL "raptorplugin.so"
#define PREF_DLL   "libpref.so"
#define PARSER_DLL "libraptorhtmlpars.so"
#define DOM_DLL    "libjsdom.so"
#define LAYOUT_DLL "libraptorhtml.so"
#define NETLIB_DLL "libnetlib.so"
#define EDITOR_DLL "libender.so"
#define RDF_DLL    "librdf.so"

#define APPSHELL_DLL  "libnsappshell.so"

#endif // XP_MAC

#endif // XP_PC

// Class ID's
static NS_DEFINE_IID(kCTreeViewCID, NS_TREEVIEW_CID);
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
static NS_DEFINE_IID(kCDocumentLoaderCID, NS_DOCUMENTLOADER_CID);
static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);
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
static NS_DEFINE_IID(kIEditFactoryIID, NS_IEDITORFACTORY_IID);

static NS_DEFINE_CID(kRDFBookMarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFDataBaseCID,           NS_RDFDATABASE_CID);
static NS_DEFINE_CID(kRDFHTMLDocumentCID,       NS_RDFHTMLDOCUMENT_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFTreeDocumentCID,       NS_RDFTREEDOCUMENT_CID);

static NS_DEFINE_CID(kCSSParserCID,             NS_CSSPARSER_CID);
static NS_DEFINE_CID(kPresShellCID,             NS_PRESSHELL_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,        NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kTextNodeCID,              NS_TEXTNODE_CID);
static NS_DEFINE_CID(kSelectionCID,             NS_SELECTION_CID);
static NS_DEFINE_CID(kRangeListCID,				NS_RANGELIST_CID);
static NS_DEFINE_CID(kFrameUtilCID,             NS_FRAME_UTIL_CID);


static NS_DEFINE_IID(kCAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
/// static NS_DEFINE_IID(kCBrowserControllerCID, NS_BROWSERCONTROLLER_CID);

extern "C" void
NS_SetupRegistry()
{
  nsRepository::RegisterFactory(kCTreeViewCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kLookAndFeelCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCHScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDialogCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCLabelCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCheckButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCChildIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCAppShellCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCToolkitCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCRegionIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCBlenderIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDeviceContextSpecCID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDeviceContextSpecFactoryCID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCViewManagerCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCScrollingViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kWebShellCID, WEB_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDocumentLoaderCID, WEB_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kThrobberCID, WEB_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kPrefCID, PREF_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCPluginHostCID, PLUGIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCParserCID, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kWellFormedDTDCID, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDOMScriptObjectFactory, DOM_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCScriptNameSetRegistry, DOM_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCHTMLDocument, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXMLDocument, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCImageDocument, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCHTMLImageElement, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kNameSpaceManagerCID, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kNetServiceCID, NETLIB_DLL, PR_FALSE, PR_FALSE);
#ifndef XP_MAC		// temporary
  nsRepository::RegisterFactory(kIEditFactoryIID, EDITOR_DLL, PR_FALSE, PR_FALSE);
#endif	//XP_MAC

  nsRepository::RegisterFactory(kRDFHTMLDocumentCID,       RDF_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kRDFTreeDocumentCID,       RDF_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kRDFInMemoryDataSourceCID, RDF_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kRDFServiceCID,            RDF_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kRDFBookMarkDataSourceCID, RDF_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kRDFDataBaseCID,           RDF_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kCSSParserCID,      LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kPresShellCID,      LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kHTMLStyleSheetCID, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kTextNodeCID,       LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kSelectionCID,      LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kRangeListCID,		LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kFrameUtilCID,      LAYOUT_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kCAppShellServiceCID, APPSHELL_DLL, PR_FALSE, PR_FALSE);
///   nsRepository::RegisterFactory(kCBrowserControllerCID, BROWSER_DLL, PR_FALSE, PR_FALSE);
}
