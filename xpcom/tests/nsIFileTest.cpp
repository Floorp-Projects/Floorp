#include "nsIFile.h"
#include "nsFileUtils.h"

#include "stdio.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"

void Passed();
void Failed(const char* explanation = nsnull);
void Inspect();
void Banner(const char* bannerString);

void VerifyResult(nsresult rv)
{
    if (NS_FAILED(rv))
    {
        Failed("rv failed");
        printf("rv = %d\n", rv);
    }
}
//----------------------------------------------------------------------------------------
void Banner(const char* bannerString)
//----------------------------------------------------------------------------------------
{
    printf("---------------------------\n");
    printf("%s\n", bannerString);
    printf("---------------------------\n");
}

//----------------------------------------------------------------------------------------
void Passed()
//----------------------------------------------------------------------------------------
{
    printf("Test passed.");
}

//----------------------------------------------------------------------------------------
void Failed(const char* explanation)
//----------------------------------------------------------------------------------------
{
    printf("ERROR : Test failed.\n");
    printf("REASON: %s.\n", explanation);
}

//----------------------------------------------------------------------------------------
void Inspect()
//----------------------------------------------------------------------------------------
{
    printf("^^^^^^^^^^ PLEASE INSPECT OUTPUT FOR ERRORS\n");
}

void GetPaths(nsIFile* file)
{
    nsresult rv;
    char* fileName;

    printf("Getting Paths\n");

    rv = file->GetPath(nsIFile::UNIX_PATH, &fileName);
    VerifyResult(rv);
    
    printf("Unix filepath: %s\n", fileName);
    nsAllocator::Free( fileName );

    rv = file->GetPath(nsIFile::NATIVE_PATH, &fileName);
    VerifyResult(rv);
    
    printf("Native filepath: %s\n", fileName);
    nsAllocator::Free( fileName );
    
    rv = file->GetPath(nsIFile::NSPR_PATH, &fileName);
    VerifyResult(rv);
    
    printf("NSPR filepath: %s\n", fileName);
    nsAllocator::Free( fileName );
}

extern "C" void
NS_SetupRegistry()
{
  nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL);
}

void InitTest(PRInt32 creationType, char* creationPath, char* appendPath)
{
    nsIFile* file = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(NS_FILE_PROGID, NULL, nsCOMTypeInfo<nsIFile>::GetIID(), (void**) &file);
    
    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsIFile failed\n");
        return;
    }

    char* fileName;

    Banner("InitWithPath");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithPath(creationType, creationPath);
    VerifyResult(rv);
    
    printf("Getting Filename\n");
    rv = file->GetFileName(&fileName);
    printf(" %s\n", fileName);
    VerifyResult(rv);
    nsAllocator::Free( fileName );

    printf("Appending %s \n", appendPath);
    rv = file->AppendPath(appendPath);
    VerifyResult(rv);

    printf("Getting Filename\n");
    rv = file->GetFileName(&fileName);
    printf(" %s\n", fileName);
    VerifyResult(rv);
    nsAllocator::Free( fileName );

    GetPaths(file);

    
    printf("Check For Existance\n");

    PRBool exists;
    file->IsExists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");
}


void CreationTest(PRInt32 creationType, char* creationPath, char* appendPath, PRInt32 whatToCreate, PRInt32 perm)
{
    nsIFile* file = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(NS_FILE_PROGID, NULL, nsCOMTypeInfo<nsIFile>::GetIID(), (void**) &file);
    
    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsIFile failed\n");
        return;
    }

    Banner("Creation Test");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithPath(creationType, creationPath);
    VerifyResult(rv);
 
    printf("Appending %s\n", appendPath);
    rv = file->AppendPath(appendPath);
    VerifyResult(rv);
    
    printf("Check For Existance\n");

    PRBool exists;
    file->IsExists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");


    rv = file->Create(whatToCreate, perm);  
    VerifyResult(rv);

    rv = file->IsExists(&exists);
    VerifyResult(rv);

    
    if (!exists)
    {
        Failed("Did not create file system object!");
        return;
    }
    
    NS_RELEASE(file);
}    
    
void
DeletetionTest(PRInt32 creationType, char* creationPath, char* appendPath, PRBool recursive)
{
    nsIFile* file = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(NS_FILE_PROGID, NULL, nsCOMTypeInfo<nsIFile>::GetIID(), (void**) &file);
    
    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsIFile failed\n");
        return;
    }

    Banner("Deletion Test");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithPath(creationType, creationPath);
    VerifyResult(rv);
 
    printf("Appending %s\n", appendPath);
    rv = file->AppendPath(appendPath);
    VerifyResult(rv);
    
    printf("Check For Existance\n");

    PRBool exists;
    file->IsExists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");

    rv = file->Delete(recursive);  
    VerifyResult(rv);

    rv = file->IsExists(&exists);
    VerifyResult(rv);
    
    if (exists)
    {
        Failed("Did not create delete system object!");
        return;
    }
    
    NS_RELEASE(file);
}



void main(void)
{
    NS_SetupRegistry();


#ifdef XP_PC
    InitTest(nsIFile::UNIX_PATH, "/c|/temp/", "sub1/sub2/");
    InitTest(nsIFile::NATIVE_PATH, "c:\\temp\\", "sub1/sub2/");
    
    InitTest(nsIFile::UNIX_PATH, "/d|/temp/", "sub1/sub2/");
    InitTest(nsIFile::NATIVE_PATH, "d:\\temp\\", "sub1/sub2/");

    CreationTest(nsIFile::UNIX_PATH, "/c|/temp/", "file.txt", nsIFile::NORMAL_FILE_TYPE, 0644);
    DeletetionTest(nsIFile::UNIX_PATH, "/c|/temp/", "file.txt", PR_FALSE);

    
    CreationTest(nsIFile::UNIX_PATH, "/c|/temp/", "mumble/a/b/c/d/e/f/g/h/i/j/k/", nsIFile::DIRECTORY_TYPE, 0644);
    DeletetionTest(nsIFile::UNIX_PATH, "/c|/temp/", "mumble", PR_TRUE);

#endif

}
