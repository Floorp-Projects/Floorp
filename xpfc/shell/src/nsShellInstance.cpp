/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#define NS_IMPL_IDS 1

#include <stdio.h>
#include "nscore.h"

#include "nspr.h"
#include "net.h"
#include "plstr.h"

#include "nsISupports.h"
#include "nsIShellInstance.h"
#include "nsShellInstance.h"
#include "nsITimer.h"

#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsxpfcCIID.h"
#include "nsParserCIID.h"
#include "nsIAppShell.h"
#include "nsIWebViewerContainer.h"

#include "nsIPref.h"
#include "nsStreamManager.h"
#include "nsXPFCToolbarManager.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIThrobber.h"
#include "nsViewsCID.h"
#include "nsPluginsCID.h"
#include "nsIDeviceContext.h"
#include "nsINetService.h"
#include "nsDOMCID.h"
#include "nsLayoutCID.h"
#include "nsINetService.h"

#ifdef NS_WIN32
#include "direct.h"
#include "windows.h"
#elif NS_UNIX
#include <Xm/Xm.h>
#endif


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCShellInstance, NS_XPFC_SHELL_INSTANCE_CID);
static NS_DEFINE_IID(kCStreamManager, NS_STREAM_MANAGER_CID);
static NS_DEFINE_IID(kIStreamManager, NS_ISTREAM_MANAGER_IID);
static NS_DEFINE_IID(kCXPFCToolbarManager, NS_XPFCTOOLBAR_MANAGER_CID);
static NS_DEFINE_IID(kIXPFCToolbarManager, NS_IXPFCTOOLBAR_MANAGER_IID);
static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsEventStatus PR_CALLBACK HandleEventApplication(nsGUIEvent *aEvent);
nsShellInstance * gShellInstance = nsnull;

nsShellInstance::nsShellInstance()
{
  NS_INIT_REFCNT();
  mApplicationWindow = nsnull;
  mPref = nsnull;
  gShellInstance = this;
  mStreamManager = nsnull;
  mToolbarManager = nsnull;
  mDeviceContext = nsnull;
  mOptState = nsnull;
  mArgc = 0;
  mArgv = nsnull;
}

nsShellInstance::~nsShellInstance()
{
  //NS_ShutdownINetService();

  if (nsnull != mOptState)
  	PL_DestroyOptState(mOptState);

  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mApplicationWindow);

  if (mPref != nsnull)
    mPref->Shutdown();

  NS_IF_RELEASE(mPref);
  NS_IF_RELEASE(mStreamManager);
  NS_IF_RELEASE(mToolbarManager);
}

NS_DEFINE_IID(kIShellInstanceIID, NS_IXPFC_SHELL_INSTANCE_IID);
NS_IMPL_ISUPPORTS(nsShellInstance,kIShellInstanceIID);

nsresult nsShellInstance::Init()
{
  nsresult res = NS_OK;

  RegisterFactories() ;

  // Load preferences
  res = nsRepository::CreateInstance(kPrefCID, NULL, kIPrefIID,
                                     (void **) &mPref);
  if (NS_OK != res) {
    return res;
  }

  mPref->Startup(nsnull);


  //res = NS_InitINetService();
  //if (NS_OK != res)
  //  return res;

  // Create a Stream Manager
  res = nsRepository::CreateInstance(kCStreamManager, 
                                     NULL, 
                                     kIStreamManager,
                                     (void **) &mStreamManager);
  if (NS_OK != res)
    return res;

  mStreamManager->Init();


  // Create a Toolbar Manager
  res = nsRepository::CreateInstance(kCXPFCToolbarManager, 
                                     NULL, 
                                     kIXPFCToolbarManager,
                                     (void **) &mToolbarManager);
  if (NS_OK != res)
    return res;

  mToolbarManager->Init();

  return res;
}

nsresult nsShellInstance::Run()
{

#ifdef NS_WIN32
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return ((nsresult)msg.wParam);
#elif NS_UNIX
#if 0
  extern   XtAppContext app_context ;

  XtAppMainLoop(app_context) ;
#endif
  extern   XtAppContext app_context ;
  extern Widget topLevel;

  XtRealizeWidget(topLevel);

  XEvent event;
  for (;;) {
    XtAppNextEvent(app_context, &event);
    XtDispatchEvent(&event);
  } 

  return NS_OK;
#else
  return NS_OK;
#endif
}

void * nsShellInstance::GetNativeInstance()
{
  return mNativeInstance ;
}

nsIPref * nsShellInstance::GetPreferences()
{
  return (mPref) ;
}

nsresult nsShellInstance::GetCommandLineOptions(PLOptState** aOptState, const char * aOptions)
{
  mOptState = PL_CreateOptState(mArgc, mArgv, aOptions);

  *aOptState = mOptState;

  return (NS_OK) ;
}

nsIStreamManager * nsShellInstance::GetStreamManager()
{
  return (mStreamManager) ;
}

nsIXPFCToolbarManager * nsShellInstance::GetToolbarManager()
{
  return (mToolbarManager) ;
}

void nsShellInstance::SetNativeInstance(void * aNativeInstance)
{
  mNativeInstance = aNativeInstance;
  return ;
}

nsIApplicationShell * nsShellInstance::GetApplicationShell()
{
  return mApplicationShell ;
}

void nsShellInstance::SetApplicationShell(nsIApplicationShell * aApplicationShell)
{
  mApplicationShell = aApplicationShell;
  return ;
}

// XXX We really need a standard way to enumerate 
//     a set of libraries and call their self
//     registration routines... when that code is 
//     XP of course.
nsresult nsShellInstance::RegisterFactories()
{
  // hardcode names of dll's
#ifdef NS_WIN32
  #define GFXWIN_DLL "raptorgfxwin.dll"
  #define WIDGET_DLL "raptorwidget.dll"
  #define VIEW_DLL   "raptorview.dll"
  #define PARSER_DLL "raptorhtmlpars.dll"
  #define PREF_DLL   "xppref32.dll"
  #define WEB_DLL    "raptorweb.dll"
  #define PLUGIN_DLL "raptorplugin.dll"
  #define NETLIB_DLL "netlib.dll"  
  #define DOM_DLL    "jsdom.dll"
  #define LAYOUT_DLL "raptorhtml.dll"
#else
  #define GFXWIN_DLL "libgfxunix.so"
  #define WIDGET_DLL "libwidgetunix.so"
  #define VIEW_DLL   "libraptorview.so"
  #define PARSER_DLL "libraptorhtmlpars.so"
  #define PREF_DLL   "libpref.so"
  #define WEB_DLL    "libraptorweb.so"
  #define PLUGIN_DLL "raptorplugin.so"
  #define NETLIB_DLL "libnetlib.so"
  #define DOM_DLL    "libjsdom.so"
  #define LAYOUT_DLL "libraptorhtml.so"
#endif


  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  nsRepository::RegisterFactory(kIWidgetIID, WIDGET_DLL, PR_FALSE, PR_FALSE);

  // register graphics classes
  static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
  static NS_DEFINE_IID(kCRegionIID, NS_REGION_CID);
  static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

  nsRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCRegionIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

  // register widget classes
  static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
  static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
  static NS_DEFINE_IID(kCDialogCID, NS_DIALOG_CID);
  static NS_DEFINE_IID(kCLabelCID, NS_LABEL_CID);
  static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
  static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
  static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
  static NS_DEFINE_IID(kCToolkitCID, NS_TOOLKIT_CID);
  static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
  static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
  static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
  static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
  static NS_DEFINE_IID(kCRadioGroupCID, NS_RADIOGROUP_CID);
  static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
  static NS_DEFINE_IID(kCHorzScrollbarCID, NS_HORZSCROLLBAR_CID);
  static NS_DEFINE_IID(kCVertScrollbarCID, NS_VERTSCROLLBAR_CID);
  static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
  static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);
  static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID);
  static NS_DEFINE_IID(kCTabWidgetCID, NS_TABWIDGET_CID);
  static NS_DEFINE_IID(kDocumentLoaderCID, NS_DOCUMENTLOADER_CID);
  static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);
  static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
  static NS_DEFINE_IID(kCPluginHostCID, NS_PLUGIN_HOST_CID);
  static NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
  static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
  static NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);
  static NS_DEFINE_IID(kCDOMScriptObjectFactory, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
  static NS_DEFINE_IID(kCDOMNativeObjectRegistry, NS_DOM_NATIVE_OBJECT_REGISTRY_CID);
  static NS_DEFINE_IID(kCHTMLDocument, NS_HTMLDOCUMENT_CID);
  static NS_DEFINE_IID(kCImageDocument, NS_IMAGEDOCUMENT_CID);
  static NS_DEFINE_IID(kCHTMLImageElementFactory, NS_HTMLIMAGEELEMENTFACTORY_CID);

  nsRepository::RegisterFactory(kCWindowCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCChildCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCheckButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCRadioGroupCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCHorzScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVertScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCTabWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCParserCID, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCParserNodeCID, PARSER_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kPrefCID, PREF_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kDocumentLoaderCID, WEB_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kThrobberCID, WEB_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kWebShellCID, WEB_DLL, PR_FALSE, PR_FALSE);
  
  nsRepository::RegisterFactory(kCPluginHostCID, PLUGIN_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kCViewManagerCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCScrollingViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kCDOMScriptObjectFactory, DOM_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDOMNativeObjectRegistry, DOM_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCHTMLDocument, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCImageDocument, LAYOUT_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCHTMLImageElementFactory, LAYOUT_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kNetServiceCID, NETLIB_DLL, PR_FALSE, PR_FALSE);

  nsRepository::RegisterFactory(kLookAndFeelCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDialogCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCLabelCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCAppShellCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCToolkitCID, WIDGET_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsIWidget * nsShellInstance::CreateApplicationWindow(nsIAppShell * aAppShell,
                                                     const nsRect &aRect)
{

  nsRect windowRect ;

  if (aRect.IsEmpty()) {
    windowRect.SetRect(100,100,320,480);
  } else {
    windowRect.SetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);

  nsRepository::CreateInstance(kCWindowCID, 
                               nsnull, 
                               kIWidgetIID, 
                               (void **)&(mApplicationWindow));

  nsWidgetInitData initData ;

  initData.clipChildren = PR_TRUE;

  nsresult res = nsRepository::CreateInstance(kDeviceContextCID, 
                                              nsnull, 
                                              kDeviceContextIID, 
                                              (void **)&mDeviceContext);

  if (NS_OK == res)
    mDeviceContext->Init(nsnull);

  mApplicationWindow->Create((nsIWidget*)nsnull, 
                             aRect, 
                             HandleEventApplication, 
                             mDeviceContext, 
                             aAppShell, 
                             nsnull, 
                             &initData);

  return (mApplicationWindow);
}


nsresult nsShellInstance::ShowApplicationWindow(PRBool show)
{
  mApplicationWindow->Show(show);

#ifdef NS_UNIX
  XtRealizeWidget((Widget)GetNativeInstance());
#endif

  return NS_OK;
}

nsresult nsShellInstance::ExitApplication()
{

#ifdef NS_WIN32
  PostQuitMessage(0);
#endif
  return NS_OK;
}

void * nsShellInstance::GetApplicationWindowNativeInstance()
{
  return (mApplicationWindow->GetNativeData(NS_NATIVE_WINDOW));
}

nsIWidget * nsShellInstance::GetApplicationWidget()
{
  return (mApplicationWindow);
}

EVENT_CALLBACK nsShellInstance::GetShellEventCallback()
{
  return ((EVENT_CALLBACK)HandleEventApplication);
}

nsresult nsShellInstance::LaunchApplication(nsString& aApplication)
{
  char * app = aApplication.ToNewCString();
  char *argv[2];

  PRStatus status ;

  char path[1024];
  (void)getcwd(path, sizeof(path));
  (void)PL_strcat(path, "\\");
  (void)PL_strcat(path, app);
  argv[0] = path;
  argv[1] = nsnull;

  status = PR_CreateProcessDetached(argv[0], argv, nsnull, nsnull);

  if (status == PR_FAILURE)
    return NS_OK;

  delete app;

  return NS_OK;
}

nsEventStatus PR_CALLBACK HandleEventApplication(nsGUIEvent *aEvent)
{
  /*
   * If this is a menu selection, generate a command object and
   * dispatch it to the target object
   */

  if (aEvent->message == NS_MENU_SELECTED)
  {

    nsIWebViewerContainer * viewer;

    nsresult res = gShellInstance->GetApplicationShell()->GetWebViewerContainer(&viewer);

    if (res == NS_OK)
    {
      nsMenuEvent * event = (nsMenuEvent *) aEvent;

      nsIMenuManager * menumgr = viewer->GetMenuManager();

      nsIXPFCMenuItem * item = menumgr->MenuItemFromID(event->mCommand);

      item->SendCommand();

      NS_RELEASE(viewer);

      return nsEventStatus_eIgnore; 

    }
    
  }

  return (gShellInstance->GetApplicationShell()->HandleEvent(aEvent));
}

