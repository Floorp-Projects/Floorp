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
 * The Original Code is the Mozilla Browser.
 *
 * The Initial Developer of the Original Code is
 * Fredrik Holmqvist <thesuckiestemail@yahoo.se>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Sergei Dolgov <sergei_d@fi.tartu.ee>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
//#define DC_PROGRAMNAME "firefox-bin"
# ifdef DC_PROGRAMNAME
#include <DebugConsole.h>
#endif

#include "nsIServiceManager.h"
#include "nsNativeAppSupportBase.h"
#include "nsICommandLineRunner.h"
#include "nsCOMPtr.h"
#include "nsIProxyObjectManager.h"
//#include "nsIBrowserDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIWindowMediator.h"
#include "nsXPIDLString.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIDocShell.h"

#include <Application.h>
#include <AppFileInfo.h>
#include <Resources.h>
#include <Path.h>
#include <Window.h>
#include <unistd.h>

// Two static helpers for future - if we decide to use OpenBrowserWindow, like we do in SeaMonkey
static nsresult
GetMostRecentWindow(const PRUnichar* aType, nsIDOMWindowInternal** aWindow)
{
    nsresult rv;
    nsCOMPtr<nsIWindowMediator> med(do_GetService( NS_WINDOWMEDIATOR_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;

    if (med)
    {
        nsCOMPtr<nsIWindowMediator> medProxy;
        rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD, NS_GET_IID(nsIWindowMediator), 
                                  med, NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                                  getter_AddRefs(medProxy));
        if (NS_FAILED(rv))
            return rv;
        return medProxy->GetMostRecentWindow( aType, aWindow );
    }
    return NS_ERROR_FAILURE;
}

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
        bwindow->Activate(true);
    return NS_OK;
}
//End static helpers

class nsNativeAppSupportBeOS : public nsNativeAppSupportBase
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSINATIVEAPPSUPPORT
    static void HandleCommandLine( int32 argc, char **argv, PRUint32 aState);
}; // nsNativeAppSupportBeOS


class nsBeOSApp : public BApplication
{
public:
    nsBeOSApp(sem_id sem) : BApplication( GetAppSig() ), init(sem), mMessage(NULL)
    {}

  	~nsBeOSApp()
  	{
        delete mMessage;
  	}

    void ReadyToRun() 
    {
        release_sem(init);
    }

    static int32 Main( void *args ) 
    {
        nsBeOSApp *app = new nsBeOSApp((sem_id)args);
        if (app == NULL)
            return B_ERROR;
        return app->Run();
    }
    
    void ArgvReceived(int32 argc, char **argv)
    {
        if (IsLaunching())
        {
#ifdef DC_PROGRAMNAME
       		TRACE("ArgvReceived Launching\n");
#endif
       		return;
        }
        PRInt32 aState = /*IsLaunching() ?
                         nsICommandLine::STATE_INITIAL_LAUNCH :*/
                         nsICommandLine::STATE_REMOTE_AUTO;
        nsNativeAppSupportBeOS::HandleCommandLine(argc, argv, aState);
    }

    void RefsReceived(BMessage* msg)
    {
#ifdef DC_PROGRAMNAME
        TRACE("RefsReceived\n");
#endif
        if (IsLaunching())
        {
           mMessage = new BMessage(*msg);
           return;
        }
        BPath path;
        entry_ref er;
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
            // Is started, call ArgReceived, delete mArgv, else store for future usage
            // after ::Enable() was called
            ArgvReceived(2, Argv);
            Argc = 0;
            delete [] Argv;
            Argv = NULL;
        } 
    }
    
    void MessageReceived(BMessage* msg)
    {
        // BMessage from nsNativeAppBeOS::Enable() received.
        // Services are ready, so we can supply stored refs
        if (msg->what == 'enbl' && mMessage)
        {
#ifdef DC_PROGRAMNAME
            TRACE("enbl received");
#endif
            be_app_messenger.SendMessage(mMessage);
        }
        // Processing here file drop events from BWindow
        // - until we implement native DnD in widget.
        else if (msg->what == B_SIMPLE_DATA)
        {
            RefsReceived(msg);
        }
        else
            BApplication::MessageReceived(msg);
    }
private:
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
            return sig;

        return "application/x-vnd.Mozilla";
    }
    
    char *GetAppFile()
    {
        image_info info;
        int32 cookie = 0;
        if (get_next_image_info(0, &cookie, &info) == B_OK && strlen(info.name) > 0)
            return info.name;
        
        return "";
    }

    sem_id init;
    BMessage *mMessage;
}; //class nsBeOSApp

// Create and return an instance of class nsNativeAppSupportBeOS.
nsresult
NS_CreateNativeAppSupport(nsINativeAppSupport **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    nsNativeAppSupportBeOS *pNative = new nsNativeAppSupportBeOS;
    if (!pNative)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = pNative;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsNativeAppSupportBeOS, nsINativeAppSupport)


void
nsNativeAppSupportBeOS::HandleCommandLine(int32 argc, char **argv, PRUint32 aState)
{
    nsresult rv;
    // Here we get stuck when starting from file-click or "OpenWith".
    // No cmdLine or any other service can be created
    // To workaround the problem, we store arguments if IsLaunching()
    // and using this after ::Enable() was called.
    nsCOMPtr<nsICommandLineRunner> cmdLine(do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
    if (!cmdLine)
    {
#ifdef DC_PROGRAMNAME
        TRACE("Couldn't create command line!");
#endif
        return;
    }
   
    // nsICommandLineRunner::Init() should be called from main mozilla thread
    // but we are at be_app thread. Using proxy to switch thread
    nsCOMPtr<nsICommandLineRunner> cmdLineProxy;
    rv = NS_GetProxyForObject(  NS_PROXY_TO_MAIN_THREAD, NS_GET_IID(nsICommandLineRunner), 
        cmdLine, NS_PROXY_ASYNC | NS_PROXY_ALWAYS, getter_AddRefs(cmdLineProxy));
    if (rv != NS_OK)
    {
#ifdef DC_PROGRAMNAME
        TRACE("Couldn't get command line Proxy!");
#endif
        return;
    }
    
    // nsICommandLineRunner::Init(,,workingdir,) requires some folder to be provided
    // but that's unclear if we need it, so using 0 instead atm
    rv = cmdLine->Init(argc, argv, 0 , aState);
    if (rv != NS_OK)
    {
#ifdef DC_PROGRAMNAME
        TRACE("Couldn't init command line!");
#endif
        return;
    }

    nsCOMPtr<nsIDOMWindowInternal> navWin;
    GetMostRecentWindow( NS_LITERAL_STRING( "navigator:browser" ).get(),
                         getter_AddRefs(navWin ));
    if (navWin)
    {
# ifdef DC_PROGRAMNAME
        TRACE("GotNavWin!");
# endif
        cmdLine->SetWindowContext(navWin);
    }

// TODO: try to use OpenURI here if there is navWin, maybe using special function
// OpenBrowserWindow which calls OpenURI like we do  for SeaMonkey, 
// else let CommandLineRunner to do its work.
// Problem with current implementation is unsufficient tabbed browsing support
    cmdLineProxy->Run();
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Start(PRBool *aResult) 
{
    NS_ENSURE_ARG(aResult);
    NS_ENSURE_TRUE(be_app == NULL, NS_ERROR_NOT_INITIALIZED);
    sem_id initsem = create_sem(0, "Mozilla BApplication init");
    if (initsem < B_OK)
        return NS_ERROR_FAILURE;
    thread_id tid = spawn_thread(nsBeOSApp::Main, "Mozilla XUL BApplication", B_NORMAL_PRIORITY, (void *)initsem);
#ifdef DC_PROGRAMNAME
    TRACE("BeApp created");
#endif
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

    *aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Quit() 
{
    if (be_app->Lock())
    {
        be_app->Quit();
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::ReOpen()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Enable()
{
    // Informing be_app that UI and services are ready to use.
    if (be_app)
    {
        be_app_messenger.SendMessage('enbl');
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::OnLastWindowClosing()
{
    return NS_OK;
}
