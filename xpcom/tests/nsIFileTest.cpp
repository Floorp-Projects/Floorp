#include "nsIFile.h"
#include "nsFileUtils.h"

#include "stdio.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsXPIDLString.h"

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
//----------------------------------------------------------------------------
void Banner(const char* bannerString)
//----------------------------------------------------------------------------
{
    printf("---------------------------\n");
    printf("%s\n", bannerString);
    printf("---------------------------\n");
}

//----------------------------------------------------------------------------
void Passed()
//----------------------------------------------------------------------------
{
    printf("Test passed.");
}

//----------------------------------------------------------------------------
void Failed(const char* explanation)
//----------------------------------------------------------------------------
{
    printf("ERROR : Test failed.\n");
    printf("REASON: %s.\n", explanation);
}

//----------------------------------------------------------------------------
void Inspect()
//----------------------------------------------------------------------------
{
    printf("^^^^^^^^^^ PLEASE INSPECT OUTPUT FOR ERRORS\n");
}

void GetPaths(nsIFile* file)
{
    nsresult rv;
    nsXPIDLCString pathName;

    printf("Getting Paths\n");

    rv = file->GetPath(nsIFile::UNIX_PATH, getter_Copies(pathName));
    VerifyResult(rv);
    
    printf("Unix filepath: %s\n", (const char *)pathName);

    rv = file->GetPath(nsIFile::NATIVE_PATH, getter_Copies(pathName));
    VerifyResult(rv);
    
    printf("Native filepath: %s\n", (const char *)pathName);
    
    rv = file->GetPath(nsIFile::NSPR_PATH, getter_Copies(pathName));
    VerifyResult(rv);
    
    printf("NSPR filepath: %s\n", (const char *)pathName);
}

extern "C" void
NS_SetupRegistry()
{
  nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL);
}

void InitTest(PRInt32 creationType, char* creationPath, char* appendPath)
{
    nsIFile* file = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(NS_FILE_PROGID, NULL,
						     NS_GET_IID(nsIFile),
						     (void**) &file);
    
    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsIFile failed\n");
        return;
    }

    nsXPIDLCString leafName;

    Banner("InitWithPath");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithPath(creationType, creationPath);
    VerifyResult(rv);
    
    printf("Getting Filename\n");
    rv = file->GetLeafName(getter_Copies(leafName));
    printf(" %s\n", (const char *)leafName);
    VerifyResult(rv);

    printf("Appending %s \n", appendPath);
    rv = file->AppendPath(appendPath);
    VerifyResult(rv);

    printf("Getting Filename\n");
    rv = file->GetLeafName(getter_Copies(leafName));
    printf(" %s\n", (const char *)leafName);
    VerifyResult(rv);

    GetPaths(file);

    
    printf("Check For Existence\n");

    PRBool exists;
    file->Exists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");
}


void CreationTest(PRInt32 creationType, char* creationPath, char* appendPath,
		  PRInt32 whatToCreate, PRInt32 perm)
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = 
      nsComponentManager::CreateInstance(NS_FILE_PROGID, NULL,
					 NS_GET_IID(nsIFile),
					 (void **)getter_AddRefs(file));
    
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
    file->Exists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");


    rv = file->Create(whatToCreate, perm);  
    VerifyResult(rv);

    rv = file->Exists(&exists);
    VerifyResult(rv);

    
    if (!exists)
    {
        Failed("Did not create file system object!");
        return;
    }
    
}    
    
void
DeletionTest(PRInt32 creationType, char* creationPath, char* appendPath, PRBool recursive)
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = 
      nsComponentManager::CreateInstance(NS_FILE_PROGID, NULL,
					 NS_GET_IID(nsIFile),
					 (void**)getter_AddRefs(file));
    
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
    file->Exists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");

    rv = file->Delete(recursive);  
    VerifyResult(rv);

    rv = file->Exists(&exists);
    VerifyResult(rv);
    
    if (exists)
    {
        Failed("Did not create delete system object!");
        return;
    }
    
}



int main(void)
{
    NS_SetupRegistry();


#ifdef XP_PC
    InitTest(nsIFile::UNIX_PATH, "/c|/temp/", "sub1/sub2/");
    InitTest(nsIFile::NATIVE_PATH, "c:\\temp\\", "sub1/sub2/");
    
    InitTest(nsIFile::UNIX_PATH, "/d|/temp/", "sub1/sub2/");
    InitTest(nsIFile::NATIVE_PATH, "d:\\temp\\", "sub1/sub2/");

    CreationTest(nsIFile::UNIX_PATH, "/c|/temp/", "file.txt", nsIFile::NORMAL_FILE_TYPE, 0644);
    DeletionTest(nsIFile::UNIX_PATH, "/c|/temp/", "file.txt", PR_FALSE);

    
    CreationTest(nsIFile::UNIX_PATH, "/c|/temp/", "mumble/a/b/c/d/e/f/g/h/i/j/k/", nsIFile::DIRECTORY_TYPE, 0644);
    DeletionTest(nsIFile::UNIX_PATH, "/c|/temp/", "mumble", PR_TRUE);

#else
#ifdef XP_UNIX
    InitTest(nsIFile::UNIX_PATH, "/tmp/", "sub1/sub2/");
    InitTest(nsIFile::NATIVE_PATH, "/tmp/", "sub1/sub2/");
    
    InitTest(nsIFile::UNIX_PATH, "/tmp", "sub1/sub2/");
    InitTest(nsIFile::NATIVE_PATH, "/tmp", "sub1/sub2/");

    CreationTest(nsIFile::UNIX_PATH, "/tmp", "file.txt",
		 nsIFile::NORMAL_FILE_TYPE, 0644);
    DeletionTest(nsIFile::UNIX_PATH, "/tmp/", "file.txt", PR_FALSE);

    
    CreationTest(nsIFile::UNIX_PATH, "/tmp", "mumble/a/b/c/d/e/f/g/h/i/j/k/",
		 nsIFile::DIRECTORY_TYPE, 0644);
    DeletionTest(nsIFile::UNIX_PATH, "/tmp", "mumble", PR_TRUE);
#endif /* XP_UNIX */
#endif /* XP_PC */
}
