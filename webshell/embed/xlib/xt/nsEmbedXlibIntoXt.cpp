/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// XXX Milind:
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <X11/StringDefs.h>

#include "EmbedMozilla.h"

#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsIEventQueueService.h"
#include "nsIXlibWindowService.h"
#include "nsIUnixToolkitService.h"
#include "nsIWebShell.h"
#include "nsIComponentManager.h"
#include "nsIPref.h"
#include "xlibrgb.h"




//-----------------------------------------------------------------------------
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kWindowServiceCID,       NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,       NS_XLIB_WINDOW_SERVICE_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID,            NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIPrefIID,               NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID,                NS_PREF_CID);
static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);




//-----------------------------------------------------------------------------
extern "C" void NS_SetupRegistry();

static void resize_callback( Widget    w, 
                         		 XtPointer data, 
												 		 XEvent    *xevent, 
												 		 Boolean   *toContinue );
												 
static void window_event_handler( Widget    w, 
                                  XtPointer data, 
																	XEvent    *xevent,
																	Boolean   *toContinue );

static void event_processor_callback( XtPointer client_data,
                                      int       *fd,
																			XtInputId *id );


//-----------------------------------------------------------------------------
static nsXlibEventDispatcher gsEventDispatcher = nsnull;

static nsIXlibWindowService *  gsWindowService = nsnull;
static nsIWebShell *           sgWebShell = nsnull;
static nsIPref *               sgPrefs = nsnull;

// XXX Milind: the Qt socket notifier which processes pending events
static Widget                  gTopLevelWidget = 0;
static Window                  gWebShellWindow = 0;
static Widget                  gWebShellWidget = 0;

static const long lEVENT_MASK =
                     ButtonPressMask  | ButtonReleaseMask |  
                     ButtonMotionMask | PointerMotionMask |  
                     EnterWindowMask  | LeaveWindowMask   |  
                     KeyPressMask     | KeyReleaseMask    |
                     ExposureMask;//     | ResizeRedirectMask;

// XXX Milind:
Display		*gDisplay       = NULL;
int				gScreenNumber   = 0;
Window		gTopLevelWindow = 0;




//-----------------------------------------------------------------------------
static void WindowCreateCallback( PRUint32 aID )
{
	// XXX Milind:
  printf( "window created: %u\n", aID );
	Widget	parentwidget = gTopLevelWidget;

	Widget	widget = XtCreateEmbedMozilla( parentwidget,
																				 ( Window )aID,
																				 "XtEmbedMozilla",
																					NULL,
																					0
																				);
	XtRealizeWidget( widget );

	printf( "widget(%p) of window(%ld)\n\n", widget, (Window)aID );
	XtAddEventHandler( widget,
										 ( ExposureMask |
										   ButtonPressMask | ButtonReleaseMask |
											 PointerMotionMask | ButtonMotionMask |
											 EnterWindowMask | LeaveWindowMask |
											 KeyPressMask | KeyReleaseMask
										 ),
										 False,
										 window_event_handler,
										 gsEventDispatcher
										);
}

static void WindowDestroyCallback(PRUint32 aID)
{
  printf("window destroyed\n");
}


// XXX MAIN
int main(int argc, char **argv) 
{
	XtAppContext app;

	gTopLevelWidget = XtAppInitialize( &app,                 // app context
	                                   "embed_xlib_into_qt", // app class name
																		 0,                    // option list
																		 0,                    // number of options
																		 &argc,                // argc
																		 argv,                 // argv
																		 0,                    // fallback resrces
																		 0,                    // arg list
																		 0 );                  // number of args

	gDisplay = XtDisplay( gTopLevelWidget );
	gScreenNumber = DefaultScreen( gDisplay );

	printf("TOP LEVEL WIDGET: %p\n", gTopLevelWidget);

  // init xlibrgb
  xlib_rgb_init( gDisplay, DefaultScreenOfDisplay( gDisplay ) );

  XtVaSetValues( gTopLevelWidget,
								 XtNwidth,  500,
								 XtNheight, 500,
								 NULL);


	printf("realizing TOP LEVEL WIDGET...\n");
	XtRealizeWidget( gTopLevelWidget );
	printf("done.\n");

	gTopLevelWindow = XtWindow( gTopLevelWidget );

	printf("Top Level Window = %ld\n", gTopLevelWindow);
	

  //////////////////////////////////////////////////////////////////////
  //
  // Toolkit Service setup
  // 
  // Note: This must happend before NS_SetupRegistry() is called so
  //       that the toolkit specific xpcom components can be registered
  //       as needed.
  //
  //////////////////////////////////////////////////////////////////////
  nsresult   rv;

  nsIUnixToolkitService * unixToolkitService = nsnull;
    
  rv = nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
                                          nsnull,
                                          NS_GET_IID(nsIUnixToolkitService),
                                          (void **) &unixToolkitService);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot obtain unix toolkit service.");

	if (!NS_SUCCEEDED(rv))
		return 1;

  // Force the toolkit into "xlib" mode regardless of MOZ_TOOLKIT
  unixToolkitService->SetToolkitName("xlib");

  NS_RELEASE(unixToolkitService);

  //////////////////////////////////////////////////////////////////////
  // End toolkit service setup
  //////////////////////////////////////////////////////////////////////


  //////////////////////////////////////////////////////////////////////
  //
  // Setup the registry
  //
  //////////////////////////////////////////////////////////////////////
  NS_SetupRegistry();

  printf("Creating event queue.\n");
    
  nsIEventQueueService * eventQueueService = nsnull;
  nsIEventQueue * eventQueue = nsnull;
  
  // Create the Event Queue for the UI thread...
  
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
									kIEventQueueServiceIID,
									(nsISupports **)&eventQueueService);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not obtain the event queue service.");

	if (!NS_SUCCEEDED(rv))
		return 1;

  rv = eventQueueService->CreateThreadEventQueue();
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not create the event queue for the the thread.");
  
	if (!NS_SUCCEEDED(rv))
		return 1;
  
  rv = eventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, 
	                                            &eventQueue);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not get the newly created thread event queue.\n");
  
	if (!NS_SUCCEEDED(rv))
		return 1;
  
  NS_RELEASE(eventQueueService);
  
  rv = nsServiceManager::GetService(kWindowServiceCID,
				    kWindowServiceIID,
				    (nsISupports **)&gsWindowService);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't obtain window service\n");

  if (!NS_SUCCEEDED(rv))
		return 1;

  gsWindowService->SetWindowCreateCallback(WindowCreateCallback);
  gsWindowService->SetWindowDestroyCallback(WindowDestroyCallback);

	// XXX Milind:
	XtAppAddInput( app,
	               eventQueue->GetEventQueueSelectFD(),
								 ( XtInputId* )XtInputReadMask,
								 event_processor_callback,
								 eventQueue );

  rv = nsRepository::CreateInstance(kWebShellCID, 
	                                  nsnull, 
																		kIWebShellIID, 
																		(void**)&sgWebShell);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot create WebShell.\n");

	if (!NS_SUCCEEDED(rv))
		return 1;

  sgWebShell->Init( ( nsNativeWidget )gTopLevelWindow, 0, 0, 500, 500);


	gsWindowService->GetEventDispatcher(&gsEventDispatcher);

  rv = nsComponentManager::CreateInstance(kPrefCID, 
	                                        NULL, 
																					kIPrefIID,
					                                (void **) &sgPrefs);

  if (NS_OK != rv) {
    printf("failed to get prefs instance\n");
    return rv;
  }
  
  sgPrefs->StartUp();
  sgPrefs->ReadUserPrefs();

  sgWebShell->SetPrefs(sgPrefs);
	printf("showing webshell...\n");
  sgWebShell->Show();


  char *url = "http://www.mozilla.org/unix/xlib.html";

  nsString URL(url);
  PRUnichar *u_url = ToNewUnicode(URL);
  sgWebShell->LoadURL(u_url);

	XtPopup( gTopLevelWidget, XtGrabNone );

	XtAddEventHandler( gTopLevelWidget,
										 StructureNotifyMask,
										 False,
										 resize_callback,
										 sgWebShell );
  
	XtAppMainLoop( app );
}


static void resize_callback( Widget    w, 
														 XtPointer data, 
														 XEvent    *xevent, 
														 Boolean   *toContinue )
{
	static int x = 0, y = 0, width = 0, height = 0, changed = 0;

	if ( xevent->type == ConfigureNotify )
	{
		XConfigureEvent	    &xconfigure = xevent->xconfigure;
		XResizeRequestEvent	&xresize = xevent->xresizerequest;
  	nsIWebShell					*webshell = ( nsIWebShell* )data;
		
		if ( xevent->type == ConfigureNotify )
		{
			//x = x != xconfigure.x ? changed = 1, xconfigure.x : x;
			//y = y != xconfigure.y ? changed = 1, xconfigure.y : y;
			width  = width != xconfigure.width ? changed = 1, xconfigure.width : width;
			height = height != xconfigure.height ? changed = 1, xconfigure.height : height;
		}
		else
		{
			//x = x != xresize.x ? changed = 1, xresize.x : x;
			//y = y != xresize.y ? changed = 1, xresize.y : y;
			width  = width != xresize.width ? changed = 1, xresize.width : width;
			height = height != xresize.height ? changed = 1, xresize.height : height;
		}
		
		if ( changed )
		{
			printf("RESIZE...%p(%d, %d)\n", w, width, height);
			webshell->SetBounds( 0, 0, width, height );   changed = 0;
		}
  	
	}
	
	//*toContinue = False;
}

// this will pick up size changes in the main window and resize
// the web shell window accordingly
static void window_event_handler( Widget    w, 
																	XtPointer data, 
																	XEvent    *xevent,
																	Boolean   *toContinue )
{
	//printf("window_event_handler...\n");
  (*gsEventDispatcher)((nsXlibNativeEvent) xevent);
	//*toContinue = True;
}


static void event_processor_callback( XtPointer client_data,
                                      int       *fd,
																			XtInputId *id )
{
	printf("event_processor_callback...\n");
	
	nsIEventQueue *eq = ( nsIEventQueue *)client_data;
	eq->ProcessPendingEvents();
}

