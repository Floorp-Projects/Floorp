#include "nsILocalFile.h"
#include "nsStringGlue.h"

#include <stdio.h>
#include "nsXPCOM.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"

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

void GetPaths(nsILocalFile* file)
{
    nsresult rv;
    nsCAutoString pathName;

    printf("Getting Path\n");

    rv = file->GetNativePath(pathName);
    VerifyResult(rv);
    
    printf("filepath: %s\n", pathName.get());
}

void InitTest(const char* creationPath, const char* appendPath)
{
    nsILocalFile* file = nsnull;
    nsresult rv = CallCreateInstance(NS_LOCAL_FILE_CONTRACTID, &file);
    
    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsILocalFile failed\n");
        return;
    }

    nsCAutoString leafName;

    Banner("InitWithPath");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithNativePath(nsDependentCString(creationPath));
    VerifyResult(rv);
    
    printf("Getting Filename\n");
    rv = file->GetNativeLeafName(leafName);
    printf(" %s\n", leafName.get());
    VerifyResult(rv);

    printf("Appending %s \n", appendPath);
    rv = file->AppendNative(nsDependentCString(appendPath));
    VerifyResult(rv);

    printf("Getting Filename\n");
    rv = file->GetNativeLeafName(leafName);
    printf(" %s\n", leafName.get());
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


void CreationTest(const char* creationPath, const char* appendPath,
		  PRInt32 whatToCreate, PRInt32 perm)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> file =
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);

    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsILocalFile failed\n");
        return;
    }

    Banner("Creation Test");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithNativePath(nsDependentCString(creationPath));
    VerifyResult(rv);
 
    printf("Appending %s\n", appendPath);
    rv = file->AppendRelativeNativePath(nsDependentCString(appendPath));
    VerifyResult(rv);
    
    printf("Check For Existence\n");

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

void CreateUniqueTest(const char* creationPath, const char* appendPath,
                 PRInt32 whatToCreate, PRInt32 perm)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> file =
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);

    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsILocalFile failed\n");
        return;
    }

    Banner("Creation Test");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithNativePath(nsDependentCString(creationPath));
    VerifyResult(rv);
 
    printf("Appending %s\n", appendPath);
    rv = file->AppendNative(nsDependentCString(appendPath));
    VerifyResult(rv);
    
    printf("Check For Existence\n");

    PRBool exists;
    file->Exists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");


    rv = file->CreateUnique(whatToCreate, perm);  
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
CopyTest(const char *testFile, const char *targetDir)
{
  printf("start copy test\n");

  nsresult rv;
  nsCOMPtr<nsILocalFile> file =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    
  if (NS_FAILED(rv) || (!file)) 
  {
    printf("create nsILocalFile failed\n");
    return;
  }

  rv = file->InitWithNativePath(nsDependentCString(testFile));
  VerifyResult(rv);
  
  nsCOMPtr<nsILocalFile> dir =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);

  if (NS_FAILED(rv) || (!dir)) 
  {
    printf("create nsILocalFile failed\n");
    return;
  }

  rv = dir->InitWithNativePath(nsDependentCString(targetDir));
  VerifyResult(rv);

  rv = file->CopyTo(dir, EmptyString());
  VerifyResult(rv);

  printf("end copy test\n");
}
    
void
DeletionTest(const char* creationPath, const char* appendPath, PRBool recursive)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> file =
            do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    
    if (NS_FAILED(rv) || (!file)) 
    {
        printf("create nsILocalFile failed\n");
        return;
    }

    Banner("Deletion Test");
    printf("creationPath == %s\nappendPath == %s\n", creationPath, appendPath);

    rv = file->InitWithNativePath(nsDependentCString(creationPath));
    VerifyResult(rv);
 
    printf("Appending %s\n", appendPath);
    rv = file->AppendNative(nsDependentCString(appendPath));
    VerifyResult(rv);
    
    printf("Check For Existance\n");

    PRBool exists;
    file->Exists(&exists);

    if (exists)
        printf("Yup!\n");
    else
        printf("no.\n");

    rv = file->Remove(recursive);  
    VerifyResult(rv);

    rv = file->Exists(&exists);
    VerifyResult(rv);
    
    if (exists)
    {
        Failed("Did not create delete system object!");
        return;
    }
    
}

void 
MoveTest(const char *testFile, const char *targetDir)
{
  Banner("Move Test");

  printf("start move test\n");

  nsresult rv;
  nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
     
  if (!file) 
  {
    printf("create nsILocalFile failed\n");
    return;
  }

  rv = file->InitWithNativePath(nsDependentCString(testFile));
  VerifyResult(rv);
  
  nsCOMPtr<nsILocalFile> dir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
 
  if (!dir) 
  {
    printf("create nsILocalFile failed\n");
    return;
  }

  rv = dir->InitWithNativePath(nsDependentCString(targetDir));
  VerifyResult(rv);

  rv = file->MoveToNative(dir, NS_LITERAL_CSTRING("newtemp"));
  VerifyResult(rv);
  if (NS_FAILED(rv))
  {
    printf("MoveToNative() test Failed.\n");
  }
  printf("end move test\n");
}

// move up the number of directories in moveUpCount, then append "foo/bar" 
void
NormalizeTest(const char *testPath, int moveUpCount,
	      const char *expected)
{
  Banner("Normalize Test");

  nsresult rv;
  nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
     
  if (!file)
  {
    printf("create nsILocalFile failed\n");
    return;
  }
  
  rv = file->InitWithNativePath(nsDependentCString(testPath));
  VerifyResult(rv);

  nsCOMPtr<nsIFile> parent;
  nsAutoString path;
  for (int i=0; i < moveUpCount; i++)
  {
    rv = file->GetParent(getter_AddRefs(parent));
    VerifyResult(rv);
    rv = parent->GetPath(path);
    VerifyResult(rv);
    rv = file->InitWithPath(path);
    VerifyResult(rv);
  }

  if (!parent) {
    printf("Getting parent failed!\n");
    return;
  }

  rv = parent->Append(NS_LITERAL_STRING("foo"));
  VerifyResult(rv);
  rv = parent->Append(NS_LITERAL_STRING("bar"));
  VerifyResult(rv);

  rv = parent->Normalize();
  VerifyResult(rv);

  nsCAutoString newPath;
  rv = parent->GetNativePath(newPath);
  VerifyResult(rv);

  nsCOMPtr<nsILocalFile> 
    expectedFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
     
  if (!expectedFile)
  {
    printf("create nsILocalFile failed\n");
    return;
  }
  rv = expectedFile->InitWithNativePath(nsDependentCString(expected));
  VerifyResult(rv);

  rv = expectedFile->Normalize();
  VerifyResult(rv);

  nsCAutoString expectedPath;
  rv = expectedFile->GetNativePath(expectedPath);
  VerifyResult(rv);

  if (!newPath.Equals(expectedPath)) {
    printf("ERROR: Normalize() test Failed!\n");
    printf("     Got: %s\n", newPath.get());
    printf("Expected: %s\n", expectedPath.get());
  }

  printf("end normalize test.\n");
}

int main(void)
{
    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    registrar->AutoRegister(nsnull);
  
#if defined(XP_WIN) || defined(XP_OS2)
    InitTest("c:\\temp\\", "sub1/sub2/"); // expect failure
    InitTest("d:\\temp\\", "sub1\\sub2\\"); // expect failure

    CreationTest("c:\\temp\\", "file.txt", nsIFile::NORMAL_FILE_TYPE, 0644);
    DeletionTest("c:\\temp\\", "file.txt", PR_FALSE);

    MoveTest("c:\\newtemp\\", "d:");

    CreationTest("c:\\temp\\", "mumble\\a\\b\\c\\d\\e\\f\\g\\h\\i\\j\\k\\", nsIFile::DIRECTORY_TYPE, 0644);
    DeletionTest("c:\\temp\\", "mumble", PR_TRUE);

    CreateUniqueTest("c:\\temp\\", "foo", nsIFile::NORMAL_FILE_TYPE, 0644);
    CreateUniqueTest("c:\\temp\\", "foo", nsIFile::NORMAL_FILE_TYPE, 0644);
    CreateUniqueTest("c:\\temp\\", "bar.xx", nsIFile::DIRECTORY_TYPE, 0644);
    CreateUniqueTest("c:\\temp\\", "bar.xx", nsIFile::DIRECTORY_TYPE, 0644);
    DeletionTest("c:\\temp\\", "foo", PR_TRUE);
    DeletionTest("c:\\temp\\", "foo-1", PR_TRUE);
    DeletionTest("c:\\temp\\", "bar.xx", PR_TRUE);
    DeletionTest("c:\\temp\\", "bar-1.xx", PR_TRUE);

#else
#ifdef XP_UNIX
    InitTest("/tmp/", "sub1/sub2/"); // expect failure
    
    CreationTest("/tmp", "file.txt", nsIFile::NORMAL_FILE_TYPE, 0644);
    DeletionTest("/tmp/", "file.txt", PR_FALSE);
    
    CreationTest("/tmp", "mumble/a/b/c/d/e/f/g/h/i/j/k/", nsIFile::DIRECTORY_TYPE, 0644);
    DeletionTest("/tmp", "mumble", PR_TRUE);

    CreationTest("/tmp", "file", nsIFile::NORMAL_FILE_TYPE, 0644);
    CopyTest("/tmp/file", "/tmp/newDir");
    MoveTest("/tmp/file", "/tmp/newDir/anotherNewDir");
    DeletionTest("/tmp", "newDir", PR_TRUE);

    CreationTest("/tmp", "qux/quux", nsIFile::NORMAL_FILE_TYPE, 0644);
    CreationTest("/tmp", "foo/bar", nsIFile::NORMAL_FILE_TYPE, 0644);
    NormalizeTest("/tmp/qux/quux/..", 1, "/tmp/foo/bar");
    DeletionTest("/tmp", "qux", PR_TRUE);
    DeletionTest("/tmp", "foo", PR_TRUE);

#endif /* XP_UNIX */
#endif /* XP_WIN || XP_OS2 */
    return 0;
}
