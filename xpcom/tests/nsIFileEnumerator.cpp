#include "nsILocalFile.h"
#include "nsDependentString.h"
#include "nsString.h"

#include <stdio.h>
#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"


PRBool LoopInDir(nsILocalFile* file)
{
    nsresult rv;
    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = file->GetDirectoryEntries(getter_AddRefs(entries));
    if(NS_FAILED(rv) || !entries)
        return PR_FALSE;
    
    PRBool hasMore;
    while(NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> sup;
        entries->GetNext(getter_AddRefs(sup));
        if(!sup)
            return PR_FALSE;
        
        nsCOMPtr<nsILocalFile> file = do_QueryInterface(sup);
        if(!file)
            return PR_FALSE;
    
        nsCAutoString name;
        if(NS_FAILED(file->GetNativeLeafName(name)))
            return PR_FALSE;
        
        PRBool isDir;
        printf("%s\n", name.get());
        rv = file->IsDirectory(&isDir);
        if (NS_FAILED(rv))
		{
			printf("IsDirectory Failed!!!\n");
				return PR_FALSE;
		}

		if (isDir == PR_TRUE)
        {
           nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(file);
           LoopInDir(lfile);   
        }        
    }
    return PR_TRUE;
}


int
main(int argc, char* argv[])
{
    nsresult rv;
    {
        nsCOMPtr<nsILocalFile> topDir;

        nsCOMPtr<nsIServiceManager> servMan;
        rv = NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        if (NS_FAILED(rv)) return -1;
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

        if (argc > 1 && argv[1] != nsnull)
        {
            char* pathStr = argv[1];
            NS_NewNativeLocalFile(nsDependentCString(pathStr), PR_FALSE, getter_AddRefs(topDir));
        }
    
        if (!topDir)
        {
           printf("No Top Dir\n");
           return -1;
        }
        PRInt32 startTime = PR_IntervalNow();
    
        LoopInDir(topDir);
    
        PRInt32 endTime = PR_IntervalNow();
    
        printf("\nTime: %d\n", PR_IntervalToMilliseconds(endTime - startTime));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
