#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"

#include <sys/stat.h>
#include "nspr.h"

#include "nsRepository.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"

#include "nsInstall.h"
#include "zipfile.h"


extern PRInt32 InitXPInstallObjects(nsIScriptContext *aContext, char* jarfile, char* args);


static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);

static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);


/* ReadFileIntoBuffer
 * given a file name, reads it into buffer
 * returns an error code
 */

static short ReadFileIntoBuffer(char * fileName, char** buffer, unsigned long *bufferSize)
{
    PRFileDesc* file;
    struct stat st;
    short result = 0;

    if ( stat( fileName, &st) != 0 )
    {
        result = -1;
        goto fail;
    }

    *bufferSize = st.st_size + 1;

    *buffer = (char*) PR_MALLOC( *bufferSize);
    if (*buffer == NULL)
    {
        result = -1;
        goto fail;
    }

    memset(*buffer, '\0', *bufferSize);
    file = PR_Open(fileName,  PR_RDONLY, 0644);

    if ( file == NULL)
    {
        result = -1;
        goto fail;
    }

    if ( PR_Read(file, *buffer, *bufferSize ) != st.st_size )
    {
        result = -1;
        PR_Close( file );
        goto fail;
    }


    PR_Close( file );
    
    return result;

fail:
    if (*buffer != NULL)
        delete( *buffer);
    *buffer = NULL;
    return result;
}


extern "C" NS_EXPORT PRInt32 Install(char* jarFile, char* args)
{
    // Open the jarfile.
    void* hZip;

    PRInt32 result = ZIPR_OpenArchive(jarFile ,  &hZip);
    
    if (result != ZIP_OK)
    {
        return result;
    }


    // Read manifest file for Install Script filename.
    //FIX:  need to do.
    
    char* installJSFile = "c:\\temp\\install.js";


    remove(installJSFile);

    // Extract the install.js file.
    result  = ZIPR_ExtractFile( hZip, "install.js", installJSFile );
    if (result != ZIP_OK)
    {
        return result;
    }

    
    nsIBrowserWindow    *aWindow;
    nsIWebShell         *aWebShell;

    // Create a new window so that we can both run a script in it and display UI.

    nsresult rv = nsRepository::CreateInstance( kBrowserWindowCID, 
                                                nsnull,
                                                kIBrowserWindowIID,
                                                (void**) &aWindow);
    if (rv == NS_OK) 
    {
        nsRect rect(0, 0, 275, 300);
        
        nsAutoString            retval;
        PRBool                  isUndefined;
        
        nsIScriptContextOwner*  scriptContextOwner;
        nsIScriptContext*       scriptContext;

        rv = aWindow->Init(nsnull, nsnull, rect, PRUint32(0), PR_FALSE);

        if (rv == NS_OK)
        {
            rv = aWindow->GetWebShell(aWebShell);
            
            /* 
             * FIX: Display a window here...(ie.OpenURL)
             */

            if (NS_OK == aWebShell->QueryInterface( kIScriptContextOwnerIID, (void**)&scriptContextOwner)) 
            {
                rv = scriptContextOwner->GetScriptContext(&scriptContext);

                if (NS_OK == rv) 
                {
                    
                    InitXPInstallObjects(scriptContext, jarFile, args );

                    char* buffer;
                    unsigned long bufferLength;
                    
                    ReadFileIntoBuffer(installJSFile, &buffer, &bufferLength);

                    // We expected this to block.
                    scriptContext->EvaluateString(nsString(buffer), nsnull, 0, retval, &isUndefined);

                    PR_FREEIF(buffer);
                    NS_RELEASE(scriptContext);
                }
                
                NS_RELEASE(scriptContextOwner);
            }
        }

        aWindow->Close();
	    NS_RELEASE(aWindow);
    }
    else
    {
        return -1;
    }

    ZIPR_CloseArchive(&hZip);

    return 0;
}
