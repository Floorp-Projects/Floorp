/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "prprf.h"

//#include "string.h"
//void* operator new(size_t n) { return malloc(n); }

struct FilesTest
{
    FilesTest() : mConsole() {}

    int RunAllTests();
    
    void WriteStuff(nsOutputStream& s);
    int InputStream(const char* relativePath);
    int FileSizeAndDate(const char* relativePath);
    int OutputStream(const char* relativePath);
    int IOStream(const char* relativePath);
    int StringStream();
    int Parent(const char* relativePath, nsFileSpec& outParent);
    int Delete(nsFileSpec& victim);
    int CreateDirectory(nsFileSpec& victim);
    int CreateDirectoryRecursive(const char* aPath);
    int IterateDirectoryChildren(nsFileSpec& startChild);
    int CanonicalPath(const char* relativePath);
    int Persistence(const char* relativePath);
    int FileSpecEquality(const char *aFile, const char *bFile);
    int FileSpecAppend(nsFileSpec& parent, const char* relativePath);
    int CopyToDir(const char*  sourceFile, const char* targDir);
    int MoveToDir(const char*  sourceFile, const char*  targDir);
    int Rename(const char*  sourceFile, const char* newName);
    
    int DiskSpace();

    int Execute(const char* appName, const char* args);

    int SpecialSystemDirectories();
    
    int NSPRCompatibility(const char* sourceFile);

    void Banner(const char* bannerString);
    int Passed();
    int Failed(const char* explanation = nsnull);
    int Inspect();
        
	nsOutputConsoleStream mConsole;
};

//----------------------------------------------------------------------------------------
void FilesTest::Banner(const char* bannerString)
//----------------------------------------------------------------------------------------
{
    mConsole
        << nsEndl
        << "---------------------------" << nsEndl
        << bannerString << " Test"       << nsEndl
        << "---------------------------" << nsEndl;
}

//----------------------------------------------------------------------------------------
int FilesTest::Passed()
//----------------------------------------------------------------------------------------
{
    ((nsOutputStream&)mConsole) << "Test passed.";
    mConsole << nsEndl;
    return 0; // for convenience
}

//----------------------------------------------------------------------------------------
int FilesTest::Failed(const char* explanation)
//----------------------------------------------------------------------------------------
{
    mConsole << "ERROR : Test failed." << nsEndl;
    if (explanation)
        mConsole << "REASON: " << explanation << nsEndl;
    return -1; // for convenience
}

//----------------------------------------------------------------------------------------
int FilesTest::Inspect()
//----------------------------------------------------------------------------------------
{
    mConsole << nsEndl << "^^^^^^^^^^ PLEASE INSPECT OUTPUT FOR ERRORS" << nsEndl;
    return 0; // for convenience
}

//----------------------------------------------------------------------------------------
void FilesTest::WriteStuff(nsOutputStream& s)
//----------------------------------------------------------------------------------------
{
    // Initialize a URL from a string without suffix.  Change the path to suit your machine.
    nsFileURL fileURL("file:///X/Developer/Headers", PR_FALSE);
    s << "File URL initialized to:     \"" << fileURL << "\""<< nsEndl;
    
    // Initialize a Unix path from a URL
    nsFilePath filePath(fileURL);
    s << "As a unix path:              \"" << (const char*)filePath << "\""<< nsEndl;
    
    // Initialize a native file spec from a URL
    nsFileSpec fileSpec(fileURL);
    s << "As a file spec:               " << fileSpec.GetNativePathCString() << nsEndl;
    
    // Make the spec unique (this one has no suffix).
    fileSpec.MakeUnique();
    s << "Unique file spec:             " << fileSpec.GetNativePathCString() << nsEndl;
    
    // Assign the spec to a URL
    fileURL = fileSpec;
    s << "File URL assigned from spec: \"" << fileURL.GetURLString() << "\""<< nsEndl;
    
    // Assign a unix path using a string with a suffix.
    filePath = "/X/Developer/Headers/FlatCarbon/vfp.h";
    s << "File path reassigned to:     \"" << (const char*)filePath << "\""<< nsEndl;    
    
    // Assign to a file spec using a unix path.
    fileSpec = filePath;
    s << "File spec reassigned to:      " << fileSpec.GetNativePathCString() << nsEndl;
    
    // Make this unique (this one has a suffix).
    fileSpec.MakeUnique();
    s << "File spec made unique:        " << fileSpec.GetNativePathCString() << nsEndl;
} // WriteStuff

//----------------------------------------------------------------------------------------
int FilesTest::OutputStream(const char* relativeUnixPath)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativeUnixPath, PR_TRUE); // convert to full path.
    nsFileSpec mySpec(myTextFilePath);
    const char* pathAsString = (const char*)mySpec;
    {
        mConsole << "WRITING IDENTICAL OUTPUT TO " << pathAsString << nsEndl << nsEndl;
        nsOutputFileStream testStream(mySpec);
        if (!testStream.is_open())
        {
            mConsole
                << "ERROR: File "
                << pathAsString
                << " could not be opened for output"
                << nsEndl;
            return -1;
        }
        FilesTest::WriteStuff(testStream);
    }    // <-- Scope closes the stream (and the file).

    if (!mySpec.Exists() || mySpec.IsDirectory() || !mySpec.IsFile())
    {
            mConsole
                << "ERROR: File "
                << pathAsString
                << " is not a file (cela n'est pas un pipe)"
                << nsEndl;
            return -1;
    }
	FileSizeAndDate(relativeUnixPath);
    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::StringStream()
//----------------------------------------------------------------------------------------
{
    char* string1 = nsnull, *string2 = nsnull;
    {
        mConsole << "WRITING USUAL STUFF TO string1" << nsEndl << nsEndl;
        nsOutputStringStream streamout(string1);
        FilesTest::WriteStuff(streamout);
    }
    {
        nsInputStringStream streamin(string1);
        nsOutputStringStream streamout2(string2);
        mConsole << "READING LINES FROM string1, writing to string2"
            << nsEndl << nsEndl;
        while (!streamin.eof())
        {
            char line[5000]; // Use a buffer longer than the file!
            streamin.readline(line, sizeof(line));
            streamout2 << line << nsEndl;
        }
        if (strcmp(string1, string2) != 0)
        {
            mConsole << "Results disagree!" << nsEndl;
            mConsole << "First string is:" << nsEndl;
            mConsole << string1 << nsEndl << nsEndl;
            mConsole << "Second string is:" << nsEndl;
            mConsole << string2 << nsEndl << nsEndl;
            return Failed();
        }
    }
    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::IOStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE); // relative path.
    const char* pathAsString = (const char*)myTextFilePath;
    nsFileSpec mySpec(myTextFilePath);
    mConsole
        << "Replacing \"path\" by \"ZUUL\" in " << pathAsString << nsEndl << nsEndl;
    nsIOFileStream testStream(mySpec);
    if (!testStream.is_open())
    {
        mConsole
            << "ERROR: File "
            << pathAsString
            << " could not be opened for input+output"
            << nsEndl;
        return -1;
    }
    char line[5000]; // Use a buffer longer than the file!
    testStream.seek(0); // check that the seek compiles
    while (!testStream.eof())
    {
        PRInt32 pos = testStream.tell();
        testStream.readline(line, sizeof(line));
        char* replacementSubstring = strstr(line, "path");
        if (replacementSubstring)
        {
            testStream.seek(pos + (replacementSubstring - line));
            testStream << "ZUUL";
            testStream.seek(pos); // back to the start of the line
        }
    }
    return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Persistence(
    const char* relativePathToWrite)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePathToWrite, PR_TRUE);
    const char* pathAsString = (const char*)myTextFilePath;
    nsFileSpec mySpec(myTextFilePath);

    nsIOFileStream testStream(mySpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
    if (!testStream.is_open())
    {
        mConsole
            << "ERROR: File "
            << pathAsString
            << " could not be opened for input+output"
            << nsEndl;
        return -1;
    }
    
    nsPersistentFileDescriptor myPersistent(mySpec);
    mConsole
        << "Writing persistent file data " << pathAsString << nsEndl << nsEndl;
    
    testStream.seek(0); // check that the seek compiles
    testStream << myPersistent;
    
    testStream.seek(0);
    
    nsPersistentFileDescriptor mySecondPersistent;
    testStream >> mySecondPersistent;
    
    mySpec = mySecondPersistent;
#ifdef XP_MAC
    if (mySpec.Error())
        return Failed();
#endif

    if (!mySpec.Exists())
        return Failed();
    
    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::InputStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE);
    const char* pathAsString = (const char*)myTextFilePath;
    mConsole << "READING BACK DATA FROM " << pathAsString << nsEndl << nsEndl;
    nsFileSpec mySpec(myTextFilePath);
    nsInputFileStream testStream2(mySpec);
    if (!testStream2.is_open())
    {
        mConsole
            << "ERROR: File "
            << pathAsString
            << " could not be opened for input"
            << nsEndl;
        return -1;
    }
    char line[1000];
    
    testStream2.seek(0); // check that the seek compiles
    while (!testStream2.eof())
    {
        testStream2.readline(line, sizeof(line));
        mConsole << line << nsEndl;
    }
    return Inspect();
}

//----------------------------------------------------------------------------------------
int FilesTest::FileSizeAndDate(const char* relativePath)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE);
    const char* pathAsString = (const char*)myTextFilePath;
    mConsole << "Info for " << pathAsString << nsEndl;

    nsFileSpec mySpec(myTextFilePath);
    mConsole << "Size " << mySpec.GetFileSize() << nsEndl;

    static nsFileSpec::TimeStamp oldStamp = 0;
    nsFileSpec::TimeStamp newStamp = 0;
    mySpec.GetModDate(newStamp);
    mConsole << "Date " << newStamp << nsEndl;
    if (oldStamp != newStamp && oldStamp != 0)
        mConsole << "Date has changed by " << (newStamp - oldStamp) << " seconds" << nsEndl << nsEndl;
    oldStamp = newStamp;
    return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Parent(
    const char* relativePath,
    nsFileSpec& outParent)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE);
    const char* pathAsString = (const char*)myTextFilePath;
    nsFileSpec mySpec(myTextFilePath);

    mySpec.GetParent(outParent);
    nsFilePath parentPath(outParent);
    nsFileURL url(parentPath);
    mConsole
        << "GetParent() on "
        << "\n\t" << pathAsString
        << "\n yields "
        << "\n\t" << (const char*)parentPath
        << "\n or as a URL"
        << "\n\t" << (const char*)url
        << nsEndl;
    return Inspect();
}

//----------------------------------------------------------------------------------------
int FilesTest::Delete(nsFileSpec& victim)
//----------------------------------------------------------------------------------------
{
    // - Test of non-recursive delete

    nsFilePath victimPath(victim);
    mConsole
        << "Attempting to delete "
        << "\n\t" << (const char*)victimPath
        << "\n without recursive option (should fail)"    
        << nsEndl;
    victim.Delete(PR_FALSE);    
    if (victim.Exists())
        Passed();
    else
    {
        mConsole
            << "ERROR: File "
            << "\n\t" << (const char*)victimPath
            << "\n has been deleted without the recursion option,"
            << "\n and is a nonempty directory!"
            << nsEndl;
        return -1;
    }

    // - Test of recursive delete

    mConsole
        << nsEndl
        << "Deleting "
        << "\n\t" << (const char*)victimPath
        << "\n with recursive option"
        << nsEndl;
    victim.Delete(PR_TRUE);
    if (victim.Exists())
    {
        mConsole
            << "ERROR: Directory "
            << "\n\t" << (const char*)victimPath
            << "\n has NOT been deleted despite the recursion option!"
            << nsEndl;
        return -1;
    }
    
    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::CreateDirectory(nsFileSpec& dirSpec)
//----------------------------------------------------------------------------------------
{
    nsFilePath dirPath(dirSpec);
	mConsole
		<< "Testing CreateDirectory() using"
		<< "\n\t" << (const char*)dirPath
		<< nsEndl;
		
	dirSpec.CreateDirectory();
	if (!dirSpec.Exists())
		return Failed();
	dirSpec.Delete(PR_TRUE);
	return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::CreateDirectoryRecursive(const char* aPath)
//----------------------------------------------------------------------------------------
{
    nsFilePath dirPath(aPath, PR_TRUE);
	mConsole
		<< "Testing nsFilePath(X, PR_TRUE) using"
		<< "\n\t" << (const char*)aPath
		<< nsEndl;
	
	nsFileSpec spec(dirPath);
    if (spec.Valid())
        return Passed();
    return Failed();
}

//----------------------------------------------------------------------------------------
int FilesTest::IterateDirectoryChildren(nsFileSpec& startChild)
//----------------------------------------------------------------------------------------
{
    // - Test of directory iterator

    nsFileSpec grandparent;
    startChild.GetParent(grandparent); // should be the original default directory.
    nsFilePath grandparentPath(grandparent);
    
    mConsole << "Forwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator i(grandparent, +1); i.Exists(); i++)
    {
        char* itemName = (i.Spec()).GetLeafName();
        mConsole << '\t' << itemName << nsEndl;
        nsCRT::free(itemName);
    }

    mConsole << "Backwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator j(grandparent, -1); j.Exists(); j--)
    {
        char* itemName = (j.Spec()).GetLeafName();
        mConsole << '\t' << itemName << nsEndl;
        nsCRT::free(itemName);
    }
    return Inspect();
}

//----------------------------------------------------------------------------------------
int FilesTest::CanonicalPath(
    const char* relativePath)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE);
    const char* pathAsString = (const char*)myTextFilePath;
    if (*pathAsString != '/')
    {
#ifndef XP_PC
        mConsole
            << "ERROR: after initializing the path object with a relative path,"
            << "\n the path consisted of the string "
            << "\n\t" << pathAsString
            << "\n which is not a canonical full path!"
            << nsEndl;
        return -1;
#endif
    }
    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::FileSpecAppend(nsFileSpec& parent, const char* relativePath)
//----------------------------------------------------------------------------------------
{
    nsFilePath initialPath(parent);
    const char* initialPathString = (const char*)initialPath;
    mConsole << "Initial nsFileSpec:\n\t\"" << initialPathString << "\"" << nsEndl;

    nsFileSpec fileSpec(initialPath);

    mConsole << "Appending:\t\"" << relativePath << "\"" << nsEndl;

    fileSpec += relativePath;

    nsFilePath resultPath(fileSpec);
    const char* resultPathString = (const char*)resultPath;
    mConsole << "Result:\n\t\"" << resultPathString << "\"" << nsEndl;

    return Inspect();
} // FilesTest::FileSpecAppend

//----------------------------------------------------------------------------------------
int FilesTest::FileSpecEquality(const char *aFile, const char *bFile)
//----------------------------------------------------------------------------------------
{
    nsFilePath aFilePath(aFile, PR_TRUE);
    nsFilePath bFilePath(bFile, PR_TRUE);


    nsFileSpec aFileSpec(aFilePath);
    nsFileSpec bFileSpec(bFilePath);
    nsFileSpec cFileSpec(bFilePath);  // this should == bFile
    
    if (aFileSpec != bFileSpec &&
        bFileSpec == cFileSpec )
    {
        return Passed();
    }

    return Failed();
} // FilesTest::FileSpecEquality

//----------------------------------------------------------------------------------------
int FilesTest::CopyToDir(const char* file, const char* dir)
//----------------------------------------------------------------------------------------
{
    nsFileSpec dirPath(nsFilePath(dir, PR_TRUE));
        
    dirPath.CreateDirectory();
    if (! dirPath.Exists())
        return Failed();
    

    nsFileSpec mySpec(nsFilePath(file, PR_TRUE)); // relative path.
    {
        nsIOFileStream testStream(mySpec); // creates the file
        // Scope ends here, file gets closed
    }
    
    nsFileSpec filePath(nsFilePath(file, PR_TRUE));
    if (! filePath.Exists())
        return Failed();
   
    nsresult error = filePath.CopyToDir(dirPath);

    char* leafName = filePath.GetLeafName();
    dirPath += leafName;
    nsCRT::free(leafName);
    
    if (! dirPath.Exists() || ! filePath.Exists() || NS_FAILED(error))
        return Failed();

   return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::MoveToDir(const char* file, const char* dir)
//----------------------------------------------------------------------------------------
{
    nsFileSpec dirPath(nsFilePath(dir, PR_TRUE));
        
    dirPath.CreateDirectory();
    if (! dirPath.Exists())
        return Failed();
    

    nsFileSpec srcSpec(nsFilePath(file, PR_TRUE)); // relative path.
    {
       nsIOFileStream testStream(srcSpec); // creates the file
       // file gets closed here because scope ends here.
    };
    
    if (! srcSpec.Exists())
        return Failed();
   
    nsresult error = srcSpec.MoveToDir(dirPath);

    char* leafName = srcSpec.GetLeafName();
    dirPath += leafName;
    nsCRT::free(leafName);
    if (! dirPath.Exists() || ! srcSpec.Exists() || NS_FAILED(error))
        return Failed();

    return Passed();
}


//----------------------------------------------------------------------------------------
int FilesTest::DiskSpace()
//----------------------------------------------------------------------------------------
{
    
    nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::OS_DriveDirectory);
    if (!systemDir.Valid())
		return Failed();

    PRInt64 bytes = systemDir.GetDiskSpaceAvailable();
    
    char buf[100];
    PR_snprintf(buf, sizeof(buf), "OS Drive has %lld bytes free", bytes);
    printf("%s\n", buf);

    return Inspect();
}
//----------------------------------------------------------------------------------------
int FilesTest::Execute(const char* appName, const char* args)
//----------------------------------------------------------------------------------------
{
    mConsole << "Attempting to execute " << appName << nsEndl;
    nsFileSpec appSpec(appName, PR_FALSE);
    if (!appSpec.Exists())
        return Failed();
    
    nsresult error = appSpec.Execute(args);    
    if (NS_FAILED(error))
        return Failed();

    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::NSPRCompatibility(const char* relativeUnixFilePath)
//----------------------------------------------------------------------------------------
{

    nsFilePath filePath(relativeUnixFilePath, PR_TRUE); // relative path

    nsFileSpec createTheFileSpec(filePath);
    {
       nsIOFileStream testStream(createTheFileSpec); // creates the file
       // file gets closed here because scope ends here.
    };
    


    PRFileDesc* fOut = NULL;

    // From an nsFilePath
    fOut = PR_Open( nsNSPRPath(filePath), PR_RDONLY, 0 );
    if ( fOut == NULL )
    {
        return Failed();
    }
    else
    {
        PR_Close( fOut );
        fOut = NULL;
    }

    // From an nsFileSpec
    nsFileSpec fileSpec(filePath);
    
    fOut = PR_Open( nsNSPRPath(fileSpec), PR_RDONLY, 0 );
    if ( fOut == NULL )
    {
        return Failed();
    }
    else
    {
        PR_Close( fOut );
        fOut = NULL;
    }
  
    // From an nsFileURL
    nsFileURL fileURL(fileSpec);
    
    fOut = PR_Open( nsNSPRPath(fileURL), PR_RDONLY, 0 );
    if ( fOut == NULL )
    {
        return Failed();
    }
    else
    {
        PR_Close( fOut );
        fOut = NULL;
    }


    return Passed();
}


//----------------------------------------------------------------------------------------
int FilesTest::SpecialSystemDirectories()
//----------------------------------------------------------------------------------------
{
     mConsole << "Please verify that these are the paths to various system directories:" << nsEndl;


    nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::OS_DriveDirectory);
    if (!systemDir.Valid())
		return Failed();
    
    mConsole << "OS_DriveDirectory yields \t";
    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::OS_TemporaryDirectory;
    mConsole << "OS_TemporaryDirectory yields \t";
    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::OS_CurrentProcessDirectory;
    mConsole << "OS_CurrentProcessDirectory yields \t";
    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }
    
#ifdef XP_MAC
    systemDir = nsSpecialSystemDirectory::Mac_SystemDirectory;
    mConsole << "Mac_SystemDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_DesktopDirectory;
    mConsole << "Mac_DesktopDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_TrashDirectory;
    mConsole << "Mac_TrashDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_StartupDirectory;
    mConsole << "Mac_StartupDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_ShutdownDirectory;
    mConsole << "Mac_ShutdownDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_AppleMenuDirectory;
    mConsole << "Mac_AppleMenuDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_ControlPanelDirectory;
    mConsole << "Mac_ControlPanelDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_ExtensionDirectory;
    mConsole << "Mac_ExtensionDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_FontsDirectory;
    mConsole << "Mac_FontsDirectory yields \t";
    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_PreferencesDirectory;
    mConsole << "Mac_PreferencesDirectory yields \t";

    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Mac_DocumentsDirectory;
    mConsole << "Mac_DocumentsDirectory yields \t";

    if (systemDir.Valid())
    {
        mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

#elif XP_PC
    systemDir = nsSpecialSystemDirectory::Win_SystemDirectory;
    mConsole << "Win_SystemDirectory yields \t";
    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Win_WindowsDirectory;
    mConsole << "Win_WindowsDirectory yields \t";
    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

#else
    systemDir = nsSpecialSystemDirectory::Unix_LocalDirectory;
    mConsole << "Unix_LocalDirectory yields \t";
    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }

    systemDir = nsSpecialSystemDirectory::Unix_LibDirectory;
    mConsole << "Unix_LibDirectory yields \t";

    if (systemDir.Valid())
    {
        (nsOutputStream&)mConsole << systemDir.GetNativePathCString() << nsEndl;
    }
    else
    {
        mConsole <<   "nsnull"  << nsEndl;
        return Failed();
    }
#endif    
    
    return Passed();
} // FilesTest::SpecialSystemDirectories


//----------------------------------------------------------------------------------------
int FilesTest::RunAllTests()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{
	int rv = 0;

	// Test of mConsole output	
	mConsole << "WRITING TEST OUTPUT TO CONSOLE" << nsEndl << nsEndl;

	// Test of nsFileSpec
	
	Banner("Interconversion");
	WriteStuff(mConsole);
	rv = Inspect();
	if (rv)
	    return rv;
	    
	Banner("Canonical Path");
	rv = CanonicalPath("mumble/iotest.txt");
	if (rv)
	    return rv;
    
    Banner("Getting DiskSpace");
    rv = DiskSpace();
    if (rv)
        return rv;

	Banner("OutputStream");
	rv = OutputStream("mumble/iotest.txt");
	if (rv)
	    return rv;
	
	Banner("InputStream");
	rv = InputStream("mumble/iotest.txt");
	if (rv)
	    return rv;
	
	Banner("IOStream");
	rv = IOStream("mumble/iotest.txt");
	if (rv)
	    return rv;
	FileSizeAndDate("mumble/iotest.txt");

	rv = InputStream("mumble/iotest.txt");
	if (rv)
	    return rv;
	
    Banner("StringStream");
//    rv = StringStream();
	if (rv)
	    return rv;
        
	Banner("Parent");
	nsFileSpec parent;
	rv = Parent("mumble/iotest.txt", parent);
	if (rv)
	    goto Clean;
		
	Banner("FileSpec Append using Unix relative path");
	rv = FileSpecAppend(parent, "nested/unix/file.txt");
	if (rv)
	    goto Clean;
	Banner("FileSpec Append using Native relative path");
#ifdef XP_PC
    rv = FileSpecAppend(parent, "nested\\windows\\file.txt");
#elif defined(XP_MAC)
    rv = FileSpecAppend(parent, ":nested:mac:file.txt");
#else
    rv = Passed();
#endif // XP_MAC
	if (rv)
	    goto Clean;

	Banner("Delete");
	rv = Delete(parent);
	if (rv)
	    goto Clean;
		
	Banner("CreateDirectory");
	rv = CreateDirectory(parent);
	if (rv)
	    goto Clean;

    Banner("CreateDirectoryRecursive Relative (using nsFileSpec)");
	rv = CreateDirectoryRecursive("mumble/dir1/dir2/dir3/");
	if (rv)
	    goto Clean;
#ifdef XP_PC
    Banner("CreateDirectoryRecursive Absolute (using nsFileSpec)");
	rv = CreateDirectoryRecursive("c:\\temp\\dir1\\dir2\\dir3\\");
	if (rv)
	    goto Clean;
#endif

	Banner("IterateDirectoryChildren");
	rv = IterateDirectoryChildren(parent);
	if (rv)
	    goto Clean;
    
    Banner("nsFileSpec equality");
    rv = FileSpecEquality("mumble/a", "mumble/b");
	if (rv)
	    goto Clean;

    Banner("Copy");
    rv = CopyToDir("mumble/copyfile.txt", "mumble/copy");
	if (rv)
	    goto Clean;
    
    Banner("Move");
    rv = MoveToDir("mumble/moveFile.txt", "mumble/move");
	if (rv)
	    goto Clean;

    Banner("Execute");
#ifdef XP_MAC
    // This path is hard-coded to test on jrm's machine.  Finding an app
    // on an arbitrary Macintosh would cost more trouble than it's worth.
    // Change path to suit. This is currently a native path, as you can see.
    if NS_FAILED(Execute("Projects:Nav45_BRANCH:ns:cmd:macfe:"\
        "projects:client45:Client45PPC", ""))
#elif XP_PC
    if NS_FAILED(Execute("c:\\windows\\notepad.exe", ""))
#else
    if NS_FAILED(Execute("/bin/ls", "/"))
#endif
        return -1;

    Banner("NSPR Compatibility");
    rv = NSPRCompatibility("mumble/aFile.txt");
	if (rv)
	    goto Clean;

    Banner("Special System Directories");
    rv = SpecialSystemDirectories();
	if (rv)
	    goto Clean;

	Banner("Persistence");
 	rv = Persistence("mumble/filedesc.dat");
	if (rv)
	    goto Clean;

Clean:
	Banner("Delete again (to clean up our mess)");
	Delete(parent);
		
    return rv;
} // FilesTest::RunAllTests

//----------------------------------------------------------------------------------------
int main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{
    FilesTest tester;
    return tester.RunAllTests();
} // main
