#include "nsILocalFile.h"

#include "stdio.h"
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
    
    PRUint32 count = 0;
    PRBool hasMore;
    while(NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> sup;
        entries->GetNext(getter_AddRefs(sup));
        if(!sup)
            return PR_FALSE;
        
        nsCOMPtr<nsIFile> file = do_QueryInterface(sup);
        if(!file)
            return PR_FALSE;
    
        char* name;
        if(NS_FAILED(file->GetLeafName(&name)))
            return PR_FALSE;
        
        PRBool isDir;
        printf("%s\n", name);
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
       
         nsMemory::Free(name);
    }
    return PR_TRUE;
}


int
main(int argc, char* argv[])
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> topDir;

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;
    
    nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL);

    if (argc > 1 && argv[1] != nsnull) 
    {
            char* pathStr = argv[1];
        NS_NewLocalFile(pathStr, PR_FALSE, getter_AddRefs(topDir));
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

    return 0;
}
