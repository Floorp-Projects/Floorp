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

#include "nsStringSupport.h"

#include "nsNativeAppSupportBase.h"

#include <Application.h>
#include <AppFileInfo.h>
#include <Resources.h>


class nsNativeAppSupportBeOS : public nsNativeAppSupportBase
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSINATIVEAPPSUPPORT
    
private:
}; // nsNativeAppSupportBeOS


class nsBeOSApp : public BApplication
{
public:
    nsBeOSApp(sem_id sem) : BApplication( GetAppSig() ), init(sem)
    {}
  
    void ReadyToRun() 
    {
        release_sem(init);
    }

    static int32 Main( void *args ) 
    {
        nsBeOSApp *app = new nsBeOSApp((sem_id)args);
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

    nsNativeAppSupportBeOS *pNative = new nsNativeAppSupportBeOS;
    if(!pNative)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = pNative;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsNativeAppSupportBeOS, nsINativeAppSupport)

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
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::OnLastWindowClosing()
{
    return NS_OK;
}
