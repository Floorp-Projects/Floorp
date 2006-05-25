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

#include "nsStringSupport.h"

#include "nsNativeAppSupportBase.h"
#include "nsIObserver.h"

#include <Application.h>
#include <AppFileInfo.h>
#include <Window.h>
#include <View.h>
#include <StringView.h>
#include <Bitmap.h>
#include <Screen.h>
#include <Font.h>
#include <Resources.h>

#ifdef DEBUG
#define DEBUG_SPLASH 1
#endif

class nsNativeAppSupportBeOS : public nsNativeAppSupportBase,  public nsIObserver
{
public:
    nsNativeAppSupportBeOS();
    ~nsNativeAppSupportBeOS();
    NS_DECL_ISUPPORTS
    NS_DECL_NSINATIVEAPPSUPPORT
    NS_DECL_NSIOBSERVER
    
private:
    nsresult LoadBitmap();

    BWindow *window;
    BBitmap *bitmap;
    BStringView *textView;
}; // nsNativeAppSupportBeOS


class nsBeOSApp : public BApplication
{
public:
    nsBeOSApp(sem_id sem) : BApplication(GetAppSig()), init(sem)
    {}
  
    void ReadyToRun()
    {
        release_sem(init);
    }

    static int32 Main(void *args)
    {
        nsBeOSApp *app = new nsBeOSApp( (sem_id)args );
        if(app == NULL)
            return B_ERROR;
        return app->Run();
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
        if(get_next_image_info(0, &cookie, &info) == B_OK &&
            file.SetTo(info.name, B_READ_ONLY) == B_OK &&
            appFileInfo.SetTo(&file) == B_OK &&
            appFileInfo.GetSignature(sig) == B_OK)
            return sig;
    
        return "application/x-vnd.Mozilla";
    }

    sem_id init;
}; //class nsBeOSApp

// Create and return an instance of class nsNativeAppSupportBeOS.
nsresult
NS_CreateNativeAppSupport(nsINativeAppSupport **aResult) 
{
    if(!aResult) 
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
    if(window != NULL) 
        HideSplashScreen();
}


NS_IMPL_ISUPPORTS2(nsNativeAppSupportBeOS, nsINativeAppSupport, nsIObserver)

NS_IMETHODIMP
nsNativeAppSupportBeOS::Start(PRBool *aResult) 
{
    NS_ENSURE_ARG(aResult);
    NS_ENSURE_TRUE(be_app == NULL, NS_ERROR_NOT_INITIALIZED);

    sem_id initsem = create_sem(0, "Mozilla BApplication init");
    if(initsem < B_OK)
        return NS_ERROR_FAILURE;
    thread_id tid = spawn_thread(nsBeOSApp::Main, "Mozilla BApplication", B_NORMAL_PRIORITY, (void *)initsem);
    *aResult = PR_TRUE;
    if(tid < B_OK || B_OK != resume_thread(tid))
        *aResult = PR_FALSE;

    if(B_OK != acquire_sem(initsem))
        *aResult = PR_FALSE;
    
    if(B_OK != delete_sem(initsem))
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
    be_app_messenger.SendMessage(B_QUIT_REQUESTED);
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::EnsureProfile(nsICmdLineService* args)
{
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
    *aIsServerMode = PR_FALSE;
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
    if(NULL == bitmap && NS_OK != LoadBitmap())
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
    if(NULL == window) 
    {
        window = new BWindow(winRect,
                             "mozilla splash",
                             B_NO_BORDER_WINDOW_LOOK,
                             B_MODAL_APP_WINDOW_FEEL,
                             0);
        if(NULL == window)
            return NS_ERROR_FAILURE;
        BView *view = new BView(bmpRect, "splash view", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
        if(NULL != view) {
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
    if(NULL != window)
    {
        if(window->Lock())
            window->Quit();
        window = NULL;
    }
    if(NULL != bitmap) {
        delete bitmap;
        bitmap = NULL;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Observe(nsISupports *aSubject,
                            const char *aTopic,
                            const PRUnichar *someData)
{
    if(!bitmap)
        return NS_OK;
    nsCAutoString statusString;
    statusString.AssignWithConversion(someData);
    if(textView != NULL)
    {
        if (textView->LockLooper())
        {
            textView->SetText(statusString.get());
            textView->UnlockLooper();
        }
        return NS_OK;
    }
    BRect textRect = bitmap->Bounds();
    textView = new BStringView(textRect,
                             "splash text",
                             statusString.get(), 
                             B_FOLLOW_LEFT | B_FOLLOW_V_CENTER);

    if(!textView)
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
    if(NULL == rsrc)
        return NS_ERROR_FAILURE;
    size_t length;
    const void *data = rsrc->LoadResource('BBMP', "MOZILLA:SPLASH", &length);
    if(NULL == data)
        return NS_ERROR_FAILURE;
    BMessage msg;
    if(B_OK != msg.Unflatten((const char *) data))
        return NS_ERROR_FAILURE;
    BBitmap *bmp = new BBitmap(&msg);
    if(NULL == bmp)
        return NS_ERROR_FAILURE;
    bitmap = new BBitmap(bmp, true);
    if(NULL == bitmap)
    {
        delete bmp;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}
