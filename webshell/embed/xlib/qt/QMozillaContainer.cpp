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
 *   Milind Changire <changire@yahoo.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "QMozillaContainer.h"
#include "nsQtEventProcessor.h"

#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIXlibWindowService.h"
#include "nsIUnixToolkitService.h"
#include "nsIWebShell.h"
#include "nsIComponentManager.h"
#include "nsIPref.h"
#include "xlibrgb.h"

#include <stdio.h>

//-----------------------------------------------------------------------------
static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kWindowServiceCID,       NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,       NS_XLIB_WINDOW_SERVICE_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID,            NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIPrefIID,               NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID,                NS_PREF_CID);





//-----------------------------------------------------------------------------
extern "C" void NS_SetupRegistry();


// XXX ----------------------------------------------------------------
// XXX
// XXX QMozillaWidget is used to bind the windows created by Mozilla
// XXX with the Qt library.
// XXX Please refer to the Qt sources QXtWidget and QXtApplication
// XXX for further details
// XXX
class QMozillaWidget : public QWidget
{
public:
	QMozillaWidget( WId wid, QWidget* parent = NULL ) : QWidget( parent, "QMozillaContainer" )
	{
		create( wid, FALSE, FALSE );
	}

	virtual ~QMozillaWidget()
	{
	}

	virtual bool x11Event( XEvent* e )
	{
  	(*gsEventDispatcher)((nsXlibNativeEvent) e );
		return TRUE;
	}

	static nsXlibEventDispatcher gsEventDispatcher;
};

nsXlibEventDispatcher QMozillaWidget::gsEventDispatcher = nsnull;




//----------------------------------------------------------------
// XXX
// XXX QMozillaContainer methods
// XXX

QMozillaContainer::QMozillaContainer( QWidget* parent ) : QWidget( parent, "Container" )
{
	NS_INIT_ISUPPORTS();
	
	printf("calling   init()\n");
	if ( init() != 0 )
		printf("MOZILLA CONTAINER WIDGET: !!! ERROR !!!    in init()\n");
	printf("done\n");
}
	
QMozillaContainer::~QMozillaContainer()
{
	delete m_MozillaEventProcessor;
	m_WebShell->SetContainer(nsnull);
	NS_RELEASE(m_WebShell);
}


/* virtual */
void QMozillaContainer::focusInEvent ( QFocusEvent * )
{
	m_WebShell->SetFocus();
}

/* virtual */
void QMozillaContainer::focusOutEvent ( QFocusEvent * )
{
	m_WebShell->RemoveFocus();
}


/* virtual */
bool QMozillaContainer::x11Event( XEvent* xevent )
{
	if ( xevent->type == ConfigureNotify )
	//if ( xevent->type == ResizeRequest )
	{
		int changed = 0;

		XConfigureEvent	    &xconfigure = xevent->xconfigure;
		XResizeRequestEvent	&xresize = xevent->xresizerequest;
 		//nsIWebShell					*webshell = ( nsIWebShell* )data;
	
		if ( xevent->type == ConfigureNotify )
		{
			//x = x != xconfigure.x ? changed = 1, xconfigure.x : x;
			//y = y != xconfigure.y ? changed = 1, xconfigure.y : y;
			m_width  = m_width != xconfigure.width ? changed = 1, xconfigure.width : m_width;
			m_height = m_height != xconfigure.height ? changed = 1, xconfigure.height : m_height;
		}
		else
		{
			//x = x != xresize.x ? changed = 1, xresize.x : x;
			//y = y != xresize.y ? changed = 1, xresize.y : y;
			m_width  = m_width != xresize.width ? changed = 1, xresize.width : m_width;
			m_height = m_height != xresize.height ? changed = 1, xresize.height : m_height;
		}
	
		if ( changed )
		{
			//printf("RESIZE...%p(%d, %d)\n", w, width, height);
			m_WebShell->SetBounds( 0, 0, width(), height() );   changed = 0;
		}
	}

	return TRUE;
}


/* public slot */
void QMozillaContainer::loadURL( const char *url )
{
	if ( m_WebShell )
	{
		nsString URL(url);
		PRUnichar *u_url = ToNewUnicode(URL);
		m_WebShell->LoadURL(u_url);
	}
}

/* public slot */
void QMozillaContainer::reload( QMozillaReloadType type = ReloadFromCache )
{
	if ( m_WebShell )
		m_WebShell->Reload( type );
}

/* public slot */
void QMozillaContainer::forward()
{
	if ( m_WebShell && m_WebShell->CanForward() )
		m_WebShell->Forward();
}

/* public slot */
void QMozillaContainer::back()
{
	if ( m_WebShell && m_WebShell->CanBack() )
		m_WebShell->Back();
}

/* public slot */
void QMozillaContainer::stop()
{
	if ( m_WebShell )
		m_WebShell->Stop();
}



//-----------------------------------------------------------------------------

static void WindowCreateCallback( PRUint32 aID )
{
	// XXX Milind:
  printf( "window created: %u\n", aID );

	QWidget*	qwidget = new QMozillaWidget( ( WId )aID );
	qwidget->setMouseTracking( TRUE );
	XSelectInput( qt_xdisplay(),
								( Window )aID, 
								( ExposureMask |
								  ButtonPressMask | ButtonReleaseMask |
								  PointerMotionMask | ButtonMotionMask |
								  EnterWindowMask | LeaveWindowMask |
								  KeyPressMask | KeyReleaseMask |
								  StructureNotifyMask
								)
							);
}

static void WindowDestroyCallback(PRUint32 aID)
{
  printf("window destroyed\n");
}


int QMozillaContainer::init()
{
  // init xlibrgb
  xlib_rgb_init( qt_xdisplay(), DefaultScreenOfDisplay( qt_xdisplay() ) );

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
  
  rv = eventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not get the newly created thread event queue.\n");
  
	if (!NS_SUCCEEDED(rv))
		return 1;
  
  NS_RELEASE(eventQueueService);
  
  rv = nsServiceManager::GetService(kWindowServiceCID,
				    kWindowServiceIID,
				    (nsISupports **)&m_WindowService);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't obtain window service\n");

  if (!NS_SUCCEEDED(rv))
		return 1;

  m_WindowService->SetWindowCreateCallback(WindowCreateCallback);
  m_WindowService->SetWindowDestroyCallback(WindowDestroyCallback);

	printf("adding xlib event queue callback...\n");
	m_MozillaEventProcessor = new nsQtEventProcessor( eventQueue, this );


	printf("creating webshell...\n");
  rv = nsRepository::CreateInstance(kWebShellCID, 
	                                  nsnull, 
																		kIWebShellIID, 
																		(void**)&m_WebShell);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot create WebShell.\n");

	if (!NS_SUCCEEDED(rv))
		return 1;

	printf("initializing webshell...\n");
  m_WebShell->Init( ( nsNativeWidget )winId(), 0, 0, 500, 500);
	m_WebShell->SetContainer( this );


	m_WindowService->GetEventDispatcher(&QMozillaWidget::gsEventDispatcher);

  rv = nsComponentManager::CreateInstance(kPrefCID, 
	                                        NULL, 
																					kIPrefIID,
					                                (void **) &m_Prefs);

  if (NS_OK != rv) {
    printf("failed to get prefs instance\n");
    return rv;
  }
  
  m_Prefs->StartUp();
  m_Prefs->ReadUserPrefs();

  m_WebShell->SetPrefs(m_Prefs);
	printf("showing webshell...\n");
  m_WebShell->Show();


	return 0;
}


// helper fuction for BeginLoadURL, ProgressLoadURL, EndLoadURL
// XXX Dont forget to delete this 'C' String since we create it here
static char* makeCString( const PRUnichar* aString )
{
	int	len = 0;
	const PRUnichar*	ptr = aString;
	
	while ( *ptr ) len++, ptr++;

	char	*cstring = new char[ ++len ];

	// just cast down to a character
	while ( len >= 0 )
	{
		cstring[len] = ( char )aString[len];
		len--;
	}
}



// nsIWebShellContainer methods
NS_IMPL_ISUPPORTS1(QMozillaContainer, nsIWebShellContainer)



// XXX DO NOTHING: STUB METHOD FOR COMPLIANCE WITH  nsIWebShellContainer
NS_METHOD QMozillaContainer::WillLoadURL(nsIWebShell* aShell, 
																							const PRUnichar* aURL, 
																							nsLoadType aReason)
{
	char	*url = makeCString( aURL );
	printf("MOZILLA CONTAINER WIDGET: will load %s...\n", url);
	delete url;

	return NS_OK;
}


NS_METHOD QMozillaContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
	char	*url = makeCString( aURL );
	printf("MOXILLA CONTAINER WIDGET: loading %s...\n", url);
	delete url;

	emit urlLoadStarted();
	return NS_OK;
}

NS_METHOD QMozillaContainer::ProgressLoadURL( nsIWebShell* aShell, 
																									 const PRUnichar* aURL,
																									 PRInt32 aProgress, 
																									 PRInt32 aProgressMax)
{
	char	*url = makeCString( aURL );
		
	emit urlLoadProgressed( (const char *)url, (int)aProgress, (int)aProgressMax );
	
	delete url;
	
	return NS_OK;
}

NS_METHOD QMozillaContainer::EndLoadURL(nsIWebShell* aShell, 
																						 const PRUnichar* aURL, 
																						 nsresult aStatus)
{
	char	*url = makeCString( aURL );

	if ( aStatus != NS_OK )
		printf("MOZILLA CONTAINER WIDGET: error loading %s...\n", url);
	else
		printf("MOXILLA CONTAINER WIDGET: done loading %s...\n", url);

	delete url;

	emit urlLoadEnded();
	return NS_OK;
}


// XXX DO NOTHING: STUB METHOD FOR COMPLIANCE WITH  nsIWebShellContainer
NS_METHOD QMozillaContainer::NewWebShell( PRUint32 aChromeMask, 
																					PRBool aVisible, 
																					nsIWebShell *&aNewWebShell)
{
	return NS_OK;
}

// XXX DO NOTHING: STUB METHOD FOR COMPLIANCE WITH  nsIWebShellContainer
NS_METHOD QMozillaContainer::ContentShellAdded(nsIWebShell* aChildShell, 
																										nsIContent* frameNode)
{
	return NS_OK;
}

// XXX DO NOTHING: STUB METHOD FOR COMPLIANCE WITH  nsIWebShellContainer
NS_METHOD QMozillaContainer::CreatePopup( nsIDOMElement* aElement, 
																					nsIDOMElement* aPopupContent,
																					PRInt32 aXPos, PRInt32 aYPos,
																					const nsString& aPopupType, 
																					const nsString& anAnchorAlignment,
																					const nsString& aPopupAlignment,
																					nsIDOMWindowInternal* aWindow, 
																					nsIDOMWindowInternal** outPopup)
{
	return NS_OK;
}


// XXX DO NOTHING: STUB METHOD FOR COMPLIANCE WITH  nsIWebShellContainer
NS_METHOD QMozillaContainer::FindWebShellWithName(const PRUnichar* aName, 
																									nsIWebShell*& aResult)
{
	return NS_OK;
}


// XXX DO NOTHING: STUB METHOD FOR COMPLIANCE WITH  nsIWebShellContainer
NS_METHOD QMozillaContainer::FocusAvailable(nsIWebShell* aFocusedWebShell, 
																						PRBool& aFocusTaken)
{
	return NS_OK;
}


