#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"

#ifndef XP_MAC
#include <sys/stat.h>
#else
#include <stat.h>
#endif 

#include "nsInstall.h"
#include "zipfile.h"

#include "nsSpecialSystemDirectory.h"
#include "nspr.h"

#include "nsRepository.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"


#include "nsIURL.h"

extern PRInt32 InitXPInstallObjects(nsIScriptContext *aContext, const char* jarfile, const char* args);

static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);

/* ReadFileIntoBuffer
 * given a file name, reads it into buffer
 * returns an error code
 */

static short ReadFileIntoBuffer(const char* fileName, char** buffer, unsigned long *bufferSize)
{
    PRFileDesc* file;
    struct stat st;
    short result = 0;
    
    *buffer = nsnull;

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

PRInt32 Install(nsInstallInfo *installInfo)
{   
    return Install( (const char*) nsAutoCString( installInfo->GetLocalFile() ), 
                    (const char*) nsAutoCString( installInfo->GetFlags()     ), 
                    (const char*) nsAutoCString( installInfo->GetArguments() ),
                    (const char*) nsAutoCString( installInfo->GetFromURL()   ));
}

extern "C" NS_EXPORT PRInt32 Install(const char* jarFile, const char* flags, const char* args, const char* fromURL)
{
    // Open the jarfile.
    void* hZip;

    PRInt32 rv = ZIP_OpenArchive(jarFile ,  &hZip);
    
    if (rv != ZIP_OK)
        return rv;


    // Read manifest file for Install Script filename.
    //FIX:  need to do.
    

    nsSpecialSystemDirectory installJSFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    installJSFileSpec += "install.js";
    installJSFileSpec.MakeUnique();

    // Extract the install.js file.
    rv  = ZIP_ExtractFile( hZip, "install.js", installJSFileSpec.GetCString() );
    if (rv != ZIP_OK)
    {
        return rv;
    }

    nsIWebShell*      aWebShell  = nsnull;
    nsIBrowserWindow* aWindow         = nsnull;
    
    rv = nsComponentManager::CreateInstance(kBrowserWindowCID, 
                                            nsnull,
                                            kIBrowserWindowIID,
                                            (void**) &aWindow);
  
    if (NS_FAILED(rv)) 
    {
        goto bail;
    }
  
    aWindow->Init(nsnull, nsnull, nsRect(0, 0, 300, 300), PRUint32(~0), PR_FALSE);
 
    rv = aWindow->GetWebShell(aWebShell);

    if (rv == NS_OK)
    {
        nsIScriptContextOwner*  scriptContextOwner;
        nsIScriptContext*       scriptContext;

        rv = aWebShell->QueryInterface( kIScriptContextOwnerIID, (void**)&scriptContextOwner); 
    
        if (rv == NS_OK)
        {
            rv = scriptContextOwner->GetScriptContext(&scriptContext);

            if (NS_OK == rv) 
            {
        
                InitXPInstallObjects(scriptContext, jarFile, args );

                char* buffer;
                unsigned long bufferLength;
        
                ReadFileIntoBuffer(installJSFileSpec, &buffer, &bufferLength);

                nsAutoString            retval;
                PRBool                  isUndefined;
                // We expected this to block.
                scriptContext->EvaluateString(nsString(buffer), nsnull, 0, retval, &isUndefined);

                PR_FREEIF(buffer);
                NS_RELEASE(scriptContext);
            }
    
            NS_RELEASE(scriptContextOwner);
        }
        // close and release window.
//        NS_RELEASE(aWebShell);
    }

bail:

    if (aWindow != nsnull)
    {
        aWindow->Close();
        NS_RELEASE(aWindow);
    }

    ZIP_CloseArchive(&hZip);
    installJSFileSpec.Delete(PR_FALSE);

    return rv;
}
