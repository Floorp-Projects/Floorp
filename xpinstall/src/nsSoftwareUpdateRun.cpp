#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"

#include <sys/stat.h>
#include "nspr.h"

#include "nsRepository.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"


static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);

static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);


/* su_ReadFileIntoBuffer
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
        result = su_ErrInternalError;
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
        result = su_ErrInternalError;
        goto fail;
    }

    if ( PR_Read(file, *buffer, *bufferSize ) != st.st_size )
    {
        result = su_ErrInternalError;
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



int RunInstallJS(char* installJSFile)
{
    nsIBrowserWindow    *aWindow;
    nsIWebShell         *aWebShell;

    nsresult rv = nsRepository::CreateInstance( kBrowserWindowCID, 
                                                nsnull,
                                                kIBrowserWindowIID,
                                                (void**) &aWindow);
    if (rv == NS_OK) 
    {
        nsRect rect(0, 0, 275, 300);

        rv = aWindow->Init(nsnull, nsnull, rect, PRUint32(0), PR_FALSE);

        if (rv == NS_OK)
        {
            rv = aWindow->GetWebShell(aWebShell);
            
            /* FIX: Display a window here...(ie.OpenURL)
            
               What about silent/forced installs?
            */


            nsAutoString            retval;
            PRBool                  isUndefined;
            nsIScriptContextOwner*  scriptContextOwner;
            
            if (NS_OK == aWebShell->QueryInterface( kIScriptContextOwnerIID,
                                                    (void**)&scriptContextOwner)) 
            {
                const char* url = "";
                nsIScriptContext* scriptContext;
                nsresult res = scriptContextOwner->GetScriptContext(&scriptContext);

                if (NS_OK == res) 
                {
                    char* buffer;
                    unsigned long bufferLength;
                    ReadFileIntoBuffer(installJSFile, &buffer, &bufferLength);
                    
                    scriptContext->EvaluateString(nsString(buffer), nsnull, 0, retval, &isUndefined);
                    
                    PR_FREEIF(buffer);
                    NS_RELEASE(scriptContext);
                }
                
                NS_RELEASE(scriptContextOwner);
            }
        }
    }

    aWindow->Release();
    
    return 0;
}

