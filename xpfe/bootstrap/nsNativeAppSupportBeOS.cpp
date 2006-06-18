/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Takashi Toyoshima <toyoshim@be-in.org>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//This define requires DebugConsole (see BeBits.com) to be installed
//#define DC_PROGRAMNAME "seamonkey-bin"

#include "nsNativeAppSupportBase.h"
#include "nsIObserver.h"

#include "nsICmdLineService.h"
#include "nsIBrowserDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIWindowMediator.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIProxyObjectManager.h"
#include "nsXPFEComponentsCID.h"
#include "nsIAppStartup.h"
#include "nsIComponentManager.h"
#include "nsIAppShellService.h"
#include "nsVoidArray.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIDocShell.h"

#include <Application.h>
#include <AppFileInfo.h>
#include <Window.h>
#include <View.h>
#include <StringView.h>
#include <Bitmap.h>
#include <Screen.h>
#include <Font.h>
#include <Resources.h>
#include <Path.h>
#include <stdlib.h>

#ifdef DC_PROGRAMNAME
# include <DebugConsole.h>
#endif

#ifdef DEBUG
#define DEBUG_SPLASH 1
#endif

// Two static helpers 
static nsresult
GetMostRecentWindow(const PRUnichar* aType, nsIDOMWindowInternal** aWindow)
{
	nsresult rv;
	nsCOMPtr<nsIWindowMediator> med(do_GetService( NS_WINDOWMEDIATOR_CONTRACTID, &rv));
	if (NS_FAILED(rv))
	{
#ifdef DC_PROGRAMNAME
		TRACE("no med");
#endif
		return rv;
	}

	if (med)
		return med->GetMostRecentWindow(aType, aWindow);
	return NS_ERROR_FAILURE;
}

// When URI is opened from click or drop,
// user expects to see it in current wokspace and activated
static nsresult
ActivateWindow(nsIDOMWindowInternal* aWindow)
{
	nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
	NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
	nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(window->GetDocShell()));
	NS_ENSURE_TRUE(baseWindow, NS_ERROR_FAILURE);
	nsCOMPtr<nsIWidget> mainWidget;
	baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
	NS_ENSURE_TRUE(mainWidget, NS_ERROR_FAILURE);
	BWindow *bwindow = (BWindow *)(mainWidget->GetNativeData(NS_NATIVE_WINDOW));
	if (bwindow)
	{
		bwindow->SetWorkspaces(B_CURRENT_WORKSPACE);
		bwindow->Activate(true);
	}
	return NS_OK;
}
// End static helpers
class nsNativeAppSupportBeOS : public nsNativeAppSupportBase,  public nsIObserver
{
public:
	nsNativeAppSupportBeOS();
	~nsNativeAppSupportBeOS();
	NS_DECL_ISUPPORTS
	NS_DECL_NSINATIVEAPPSUPPORT
	NS_DECL_NSIOBSERVER
	static void HandleCommandLine( int32 argc, char **argv ); 
private:
	static nsresult OpenBrowserWindow(const char *args);
	nsresult LoadBitmap();
	BWindow *window;
	BBitmap *bitmap;
	BStringView *textView;
}; // nsNativeAppSupportBeOS

class nsBeOSApp : public BApplication
{
public:
	nsBeOSApp(sem_id sem) : BApplication(GetAppSig()), init(sem),
							appEnabled(false), mMessage(0)
	{}

	~nsBeOSApp()
	{
		mMessage.Clear();
	}

	void ReadyToRun()
	{
		release_sem(init);
	}

	static int32 Main(void *args)
	{
		nsBeOSApp *app = new nsBeOSApp((sem_id)args);
		if (app == NULL)
			return B_ERROR;
		return app->Run();
	}

	void ArgvReceived(int32 argc, char **argv)
	{
		if (!IsLaunching()) 
			nsNativeAppSupportBeOS::HandleCommandLine(argc, argv);
	}

	void RefsReceived(BMessage *msg)
	{
		// If UI is started, call ArgReceived, else store for future usage.
		// appEnabled is set to true after ::HideSplash() was called
		if (!appEnabled)
		{
#ifdef DC_PROGRAMNAME
			TRACE("Refs at Launch!");
#endif
			mMessage.InsertElementAt((void *)new BMessage(*msg), mMessage.Count());
			return;
		}

		entry_ref er;
		BPath path;	
		for (uint32 i = 0; msg->FindRef("refs", i, &er) == B_OK; i++)
		{
			int Argc = 2;
			char **Argv = new char*[ 3 ];
			BEntry entry(&er, true);
			BEntry fentry(GetAppFile(), false);
			entry.GetPath(&path);
			Argv[0] = strdup( GetAppFile() ? GetAppFile() : "" );
			Argv[1] = strdup( path.Path() ? path.Path() : "" );
			// Safety measure
			Argv[2] = 0;
			ArgvReceived(Argc, Argv);
			Argc = 0;
			delete [] Argv;
			Argv = NULL;
		} 
	}

	void MessageReceived(BMessage *msg)
	{
		if (msg->what == 'enbl' && !appEnabled)
		{
#ifdef DC_PROGRAMNAME
			TRACE("mMessage!");
#endif
			appEnabled = true;
		}
		
		if (appEnabled && mMessage.Count())
		{
			for (int i = 0; i < mMessage.Count(); i++)
			{
				be_app_messenger.SendMessage((BMessage *)mMessage.SafeElementAt(i));
			}
			mMessage.Clear();
		}
		
		if (msg->what == B_SIMPLE_DATA && appEnabled)
		{
			RefsReceived(msg);
		}
		BApplication::MessageReceived(msg);
	}

	char *GetAppSig()
	{
		image_info info;
		int32 cookie = 0;
		BFile file;
		BAppFileInfo appFileInfo;
		static char sig[B_MIME_TYPE_LENGTH];

		sig[0] = 0;
		if (get_next_image_info(0, &cookie, &info) == B_OK &&
			file.SetTo(info.name, B_READ_ONLY) == B_OK &&
			appFileInfo.SetTo(&file) == B_OK &&
			appFileInfo.GetSignature(sig) == B_OK)
		{
			return sig;
		}
		return "application/x-vnd.Mozilla-SeaMonkey";
	}
    
	char *GetAppFile()
	{
		image_info info;
		int32 cookie = 0;
		if (get_next_image_info(0, &cookie, &info) == B_OK && strlen(info.name) > 0)
			return info.name;
		return "./seamonkey-bin";
	}

private:
	sem_id init;
	bool appEnabled;
	nsVoidArray mMessage;
}; //class nsBeOSApp

// Create and return an instance of class nsNativeAppSupportBeOS.
nsresult
NS_CreateNativeAppSupport(nsINativeAppSupport **aResult) 
{
	if (!aResult) 
		return NS_ERROR_NULL_POINTER;

	nsNativeAppSupportBeOS *pNative = new nsNativeAppSupportBeOS();
	if (!pNative)
		return NS_ERROR_OUT_OF_MEMORY;

	*aResult = pNative;
	NS_ADDREF(*aResult);
	return NS_OK;
}

nsNativeAppSupportBeOS::nsNativeAppSupportBeOS()
	: window(NULL) , bitmap(NULL), textView(NULL)
{}

nsNativeAppSupportBeOS::~nsNativeAppSupportBeOS()
{
	if (window != NULL) 
		HideSplashScreen();
}

NS_IMPL_ISUPPORTS2(nsNativeAppSupportBeOS, nsINativeAppSupport, nsIObserver)

void nsNativeAppSupportBeOS::HandleCommandLine(int32 argc, char **argv)
{
	nsresult rv;
	nsCOMPtr<nsICmdLineService> cmdLineArgs;// = (do_GetService(NS_COMMANDLINESERVICE_CONTRACTID, &rv));
	nsCOMPtr<nsIComponentManager> compMgr;
	NS_GetComponentManager(getter_AddRefs(compMgr));
	rv = compMgr->CreateInstanceByContractID(
					NS_COMMANDLINESERVICE_CONTRACTID,
					nsnull, NS_GET_IID(nsICmdLineService),
					(void **)getter_AddRefs(cmdLineArgs));
	
	rv = cmdLineArgs->Initialize(argc, argv);
	if (NS_FAILED(rv)) return;

	nsCOMPtr<nsIAppStartup> appStartup (do_GetService(NS_APPSTARTUP_CONTRACTID, &rv));
	if (NS_FAILED(rv)) return;
	nsCOMPtr<nsIAppStartup> appStartupProxy;
	rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD, NS_GET_IID(nsIAppStartup), 
								appStartup, NS_PROXY_ASYNC | NS_PROXY_ALWAYS,
								getter_AddRefs(appStartupProxy));
	nsCOMPtr<nsINativeAppSupport> nativeApp;
	rv = appStartup->GetNativeAppSupport(getter_AddRefs(nativeApp));
	if (NS_FAILED(rv))
	{
#ifdef DC_PROGRAMNAME
		TRACE("No GetNativeAppSupport");
#endif
		return;
	}

	rv = appStartup->Ensure1Window(cmdLineArgs);
	if (NS_FAILED(rv))
	{
#ifdef DC_PROGRAMNAME
		TRACE("No Ensure1Window");
#endif
		return;
	}
	
	// first see if there is a url
	nsXPIDLCString urlToLoad;
	rv = cmdLineArgs->GetURLToLoad(getter_Copies(urlToLoad));
	if (NS_SUCCEEDED(rv) && !urlToLoad.IsEmpty())
	{
		nsCOMPtr<nsICmdLineHandler> handler(
			do_GetService("@mozilla.org/commandlinehandler/general-startup;1?type=browser"));

		nsXPIDLCString chromeUrlForTask;
		rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));

		if (NS_SUCCEEDED(rv))
			rv = OpenBrowserWindow(urlToLoad.get());
	}
}


// Open the argument URL in the most recently used Navigator window.
nsresult
nsNativeAppSupportBeOS::OpenBrowserWindow(const char *args)
{
	nsresult rv;

	// Get most recently used Nav window.
	nsCOMPtr<nsIDOMWindowInternal> navWin;
	rv = GetMostRecentWindow(NS_LITERAL_STRING("navigator:browser").get(), getter_AddRefs(navWin ));

	// If caller requires a new window, then don't use an existing one.
	if (NS_FAILED(rv) || !navWin)
	{
#ifdef DC_PROGRAMNAME
		TRACE("!navWin");
#endif
		return NS_ERROR_FAILURE;
	}

	nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(navWin, &rv));
	if (NS_FAILED(rv) || !chromeWin)
	{
#ifdef DC_PROGRAMNAME
		TRACE("!chromeWin");
#endif
		return NS_ERROR_FAILURE;
	}

	nsCOMPtr<nsIBrowserDOMWindow> bwin;
	rv = chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));
	if (NS_FAILED(rv) || !bwin)
	{
#ifdef DC_PROGRAMNAME
		TRACE("!bwin");
#endif
		return NS_ERROR_FAILURE;
	}

	nsCOMPtr<nsIURIFixup> fixup(do_GetService(NS_URIFIXUP_CONTRACTID, &rv));
	if (NS_FAILED(rv) || !fixup)
	{
#ifdef DC_PROGRAMNAME
		TRACE("!fixup");
#endif
		return NS_ERROR_FAILURE;
	}

	nsCOMPtr<nsIURI> uri;
	rv = fixup->CreateFixupURI(nsDependentCString(args),
							nsIURIFixup::FIXUP_FLAG_NONE,
							getter_AddRefs(uri));
	if (NS_FAILED(rv) || !uri)
	{
#ifdef DC_PROGRAMNAME
		TRACE("!uri");
#endif
		return NS_ERROR_FAILURE;
	}

#ifdef DC_PROGRAMNAME
	TRACE("OpenURI! %s", uri.get());
#endif
	nsCOMPtr<nsIBrowserDOMWindow> bwinProxy;
	rv = NS_GetProxyForObject(
			NS_PROXY_TO_MAIN_THREAD,
			NS_GET_IID(nsIBrowserDOMWindow),
			bwin, 
			NS_PROXY_ASYNC | NS_PROXY_ALWAYS, 
			getter_AddRefs(bwinProxy));
	
	if (NS_FAILED(rv))
	{
		printf("!bwinProxy");
		return rv;
	}

	nsCOMPtr<nsIDOMWindow> resWin;
	rv =  bwinProxy->OpenURI( uri, nsnull, nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW, nsIBrowserDOMWindow::OPEN_EXTERNAL, getter_AddRefs(resWin) );
	// rv failure may be just 404, so still activating navWin exists
	ActivateWindow(navWin);
	return rv;
}

NS_IMETHODIMP nsNativeAppSupportBeOS::Start(PRBool *aResult) 
{
	NS_ENSURE_ARG(aResult);
	NS_ENSURE_TRUE(be_app == NULL, NS_ERROR_NOT_INITIALIZED);
	sem_id initsem = create_sem(0, "Mozilla BApplication init");
	if (initsem < B_OK)
		return NS_ERROR_FAILURE;
	thread_id tid = spawn_thread(nsBeOSApp::Main, "Mozilla BApplication",
								B_NORMAL_PRIORITY,
								(void *)initsem);
	*aResult = PR_TRUE;
	if (tid < B_OK || B_OK != resume_thread(tid))
		*aResult = PR_FALSE;
	
	if (B_OK != acquire_sem(initsem))
		*aResult = PR_FALSE;

	if (B_OK != delete_sem(initsem))
		*aResult = PR_FALSE;
	return *aResult == PR_TRUE ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Stop(PRBool *aResult) 
{
	NS_ENSURE_ARG(aResult);
	NS_ENSURE_TRUE(be_app, NS_ERROR_NOT_INITIALIZED);
	// Do we need it here?
	//Quit();
	*aResult = PR_TRUE;
	 return NS_OK;
}

nsresult NS_CreateSplashScreen(nsISplashScreen **aResult)
{
	nsresult rv = NS_OK;
	if (aResult)
		*aResult = nsnull;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Quit() 
{
	be_app_messenger.SendMessage(B_QUIT_REQUESTED);
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::EnsureProfile(nsICmdLineService* args)
{
	// TODO: Needs some code for -turbo mode
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::ReOpen()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::OnLastWindowClosing()
{
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::GetIsServerMode(PRBool *aIsServerMode) 
{
	aIsServerMode = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::SetIsServerMode(PRBool aIsServerMode) 
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::StartServerMode() 
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::SetShouldShowUI(PRBool aShouldShowUI) 
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::GetShouldShowUI(PRBool *aShouldShowUI) 
{
	*aShouldShowUI = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::ShowSplashScreen() 
{
#ifdef DEBUG_SPLASH
	puts("nsNativeAppSupportBeOS::ShowSplashScreen()");
#endif
	if (NULL == bitmap && NS_OK != LoadBitmap())
		return NS_ERROR_FAILURE;

	// Get the center position.
	BScreen scr;
	BRect scrRect = scr.Frame();
	BRect bmpRect = bitmap->Bounds();
	float winX = (scrRect.right - bmpRect.right) / 2;
	float winY = (scrRect.bottom - bmpRect.bottom) / 2;
	BRect winRect(winX, winY, winX + bmpRect.right, winY + bmpRect.bottom);
#ifdef DEBUG_SPLASH
	printf("SplashRect (%f, %f) - (%f, %f)\n", winRect.left, winRect.top,
												winRect.right, winRect.bottom);
#endif
	if (NULL == window) 
	{
		window = new BWindow(winRect, "mozilla splash",
							B_NO_BORDER_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL, 0);
		if (NULL == window)
			return NS_ERROR_FAILURE;
		BView *view = new BView(bmpRect, "splash view", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
		if (NULL != view)
		{
			window->AddChild(view);
			view->SetViewBitmap(bitmap);
		}
		window->Show();
	}
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::HideSplashScreen()
{
#ifdef DEBUG_SPLASH
	puts("nsNativeAppSupportBeOS::HideSplashScreen()");
#endif
	if (NULL != window)
	{
		if (window->Lock())
			window->Quit();
		window = NULL;
	}
	if (NULL != bitmap)
	{
		delete bitmap;
		bitmap = NULL;
	}
	// Notification, now we are able to process scheduled RefsReceived().
	if (be_app)
		be_app_messenger.SendMessage('enbl');
	return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Observe(nsISupports *aSubject,
								const char *aTopic,
								const PRUnichar *someData)
{
	if (!bitmap)
		return NS_OK;
	nsCAutoString statusString;
	statusString.AssignWithConversion(someData);
	if (textView != NULL)
	{
		if (textView->LockLooper())
		{
			textView->SetText(statusString.get());
			textView->UnlockLooper();
		}
		return NS_OK;
	}
	BRect textRect = bitmap->Bounds();
	textView = new BStringView(textRect, "splash text",
								statusString.get(),
								B_FOLLOW_LEFT | B_FOLLOW_V_CENTER);

	if (!textView)
		return NS_OK;

	// Reduce the view size, and take into account the image frame
	textRect.bottom -= 10;
	textRect.left += 10;
	textRect.right -= 10; 
	textRect.top = textRect.bottom - 20;
	textView->SetViewColor(B_TRANSPARENT_COLOR);
	textView->SetHighColor(255,255,255,0);
	textView->SetLowColor(0,0,0,0);
	window->AddChild(textView);
	return NS_OK;
}

nsresult
nsNativeAppSupportBeOS::LoadBitmap()
{
	BResources *rsrc = be_app->AppResources();
	if (NULL == rsrc)
		return NS_ERROR_FAILURE;
	size_t length;
	const void *data = rsrc->LoadResource('BBMP', "MOZILLA:SPLASH", &length);
	if (NULL == data)
		return NS_ERROR_FAILURE;
	BMessage msg;
	if (B_OK != msg.Unflatten((const char *) data))
		return NS_ERROR_FAILURE;
	BBitmap *bmp = new BBitmap(&msg);
	if (NULL == bmp)
		return NS_ERROR_FAILURE;
	bitmap = new BBitmap(bmp, true);
	if (NULL == bitmap)
	{
		delete bmp;
		return NS_ERROR_FAILURE;
	}
	return NS_OK;
}
