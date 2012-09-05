#include "nsIFile.h"
#include "nsStringGlue.h"

#include <stdio.h>
#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMPtr.h"

bool LoopInDir(nsIFile* file)
{
    nsresult rv;
    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = file->GetDirectoryEntries(getter_AddRefs(entries));
    if(NS_FAILED(rv) || !entries)
        return false;
    
    bool hasMore;
    while(NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> sup;
        entries->GetNext(getter_AddRefs(sup));
        if(!sup)
            return false;
        
        nsCOMPtr<nsIFile> file = do_QueryInterface(sup);
        if(!file)
            return false;
    
        nsAutoCString name;
        if(NS_FAILED(file->GetNativeLeafName(name)))
            return false;
        
        bool isDir;
        printf("%s\n", name.get());
        rv = file->IsDirectory(&isDir);
        if (NS_FAILED(rv))
		{
			printf("IsDirectory Failed!!!\n");
				return false;
		}

		if (isDir)
        {
           LoopInDir(file);   
        }        
    }
    return true;
}


int
main(int argc, char* argv[])
{
    nsresult rv;
    {
        nsCOMPtr<nsIFile> topDir;

        nsCOMPtr<nsIServiceManager> servMan;
        rv = NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        if (NS_FAILED(rv)) return -1;

        if (argc > 1 && argv[1] != nullptr)
        {
            char* pathStr = argv[1];
            NS_NewNativeLocalFile(nsDependentCString(pathStr), false, getter_AddRefs(topDir));
        }
    
        if (!topDir)
        {
           printf("No Top Dir\n");
           return -1;
        }
        int32_t startTime = PR_IntervalNow();
    
        LoopInDir(topDir);
    
        int32_t endTime = PR_IntervalNow();
    
        printf("\nTime: %d\n", PR_IntervalToMilliseconds(endTime - startTime));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
