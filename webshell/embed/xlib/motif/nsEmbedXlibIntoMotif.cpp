
#include <Xm/Xm.h>

#include <assert.h>

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

static NS_DEFINE_IID(kIEventQueueServiceIID,
                     NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID,
                     NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kWindowServiceCID,
		     NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,
		     NS_XLIB_WINDOW_SERVICE_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

extern "C" void NS_SetupRegistry();

static void event_processor_callback(XtPointer       aClosure,
                                     int *           aFd,
                                     XtIntervalId *  aId)
{
//   printf("event_processor_callback()\n");
  nsIEventQueue *eventQueue = (nsIEventQueue*)aClosure;
  eventQueue->ProcessPendingEvents();
}

static nsXlibEventDispatcher gsEventDispatcher = nsnull;

static nsIXlibWindowService *  gsWindowService = nsnull;
static nsIWebShell *           sgWebShell = nsnull;
static nsIPref *               sgPrefs = nsnull;
static Widget                  sgTopLevel = NULL;

static void EmbedEventHandler(Widget     w, 
							  XtPointer  client_data, 
							  XEvent *   xevent, 
							  Boolean *  cont)
{
//   nsWindow * widgetWindow = (nsWindow *) p ;
//   nsMouseEvent mevent;
//   nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_DOWN);
//   widgetWindow->DispatchMouseEvent(mevent);

	printf("test_filter called\n");

  if (nsnull != gsEventDispatcher)
  {
	printf("dispatching native event\n");
	(*gsEventDispatcher)((nsXlibNativeEvent) xevent);
  }

//   gsWindowService->DispatchNativeXlibEvent((void *)xevent);

}

static void WindowCreateCallback(PRUint32 aID)
{
  Window xid = (Window) aID;

  Widget em = XmCreateEmbedMozilla(sgTopLevel,
								   xid,
								   "XmEmbedMozilla",
								   NULL,
								   0);

  XtRealizeWidget(em);

  assert( XtIsRealized(em) );

  XtAddEventHandler(em, 
					ButtonPressMask | ButtonReleaseMask |  
					ButtonMotionMask | PointerMotionMask |  
					EnterWindowMask | LeaveWindowMask |  
                    KeyPressMask | KeyReleaseMask |
					ExposureMask,
					False, 
					EmbedEventHandler,
					NULL);

//   printf("window created\n");
}

static void WindowDestroyCallback(PRUint32 aID)
{
  printf("window destroyed\n");
}


int main(int argc, char **argv) 
{
  XtAppContext  app_context = nsnull;

  XtSetLanguageProc(NULL, NULL, NULL);

  sgTopLevel = XtAppInitialize(&app_context,   // app_context_return
                              "Mozilla",      // application_class
                              NULL,           // options
                              0,              // num_options
                              &argc,          // argc_in_out
                              argv,           // argv_in_out
                              NULL,           // fallback_resources
                              NULL,           // args
                              0);             // num_args
  

  // XXX this is a hack, will replace with a service RSN
  xlib_rgb_init(XtDisplay(sgTopLevel), XtScreen(sgTopLevel));
  

  XtVaSetValues(sgTopLevel,
				XmNwidth,  500,
				XmNheight, 500,
				NULL);

  XtRealizeWidget(sgTopLevel);

  //////////////////////////////////////////////////////////////////////
  //
  // Toolkit Service setup
  // 
  // Note: This must happend before NS_SetupRegistry() is called so
  //       that the toolkit specific xpcom components can be registered
  //       as needed.
  //
  //////////////////////////////////////////////////////////////////////
  nsIUnixToolkitService * unixToolkitService = nsnull;
    
  nsresult rv =
	nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
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
  
  rv = eventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);
  
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


  XtAppAddInput(app_context,
                eventQueue->GetEventQueueSelectFD(),
                (XtPointer) XtInputReadMask, 
                event_processor_callback, 
                eventQueue);


  rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
				    kIWebShellIID,
				    (void**)&sgWebShell);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot create WebShell.\n");

  if (!NS_SUCCEEDED(rv))
	return 1;

  sgWebShell->Init((nsNativeWidget *) XtWindow(sgTopLevel),
				   0, 0,
				   500, 500);

  gsWindowService->GetEventDispatcher(&gsEventDispatcher);

  rv = nsComponentManager::CreateInstance(kPrefCID, NULL, kIPrefIID,
					  (void **) &sgPrefs);

  if (NS_OK != rv) {
    printf("failed to get prefs instance\n");
    return rv;
  }
  
  sgPrefs->StartUp();
  sgPrefs->ReadUserPrefs();

  sgWebShell->SetPrefs(sgPrefs);
  sgWebShell->Show();
  
  char *url = "http://www.slashdot.org/";

  nsString URL(url);
  PRUnichar *u_url = ToNewUnicode(URL);
  sgWebShell->LoadURL(u_url);
  
  XtPopup(sgTopLevel,XtGrabNone);

  XEvent event;

  for (;;) 
  {
    XtAppNextEvent(app_context, &event);
	
    XtDispatchEvent(&event);
  } 

  return 0;
}
