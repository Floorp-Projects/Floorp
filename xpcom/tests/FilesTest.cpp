/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

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
    int Copy(const char*  sourceFile, const char* targDir);
    int Move(const char*  sourceFile, const char*  targDir);
    int Rename(const char*  sourceFile, const char* newName);

    int Execute(const char* appName, const char* args);

    int SpecialSystemDirectories();

    void Banner(const char* bannerString);
    int Passed();
    int Failed(const char* explanation = nsnull);
    void Inspect();
        
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
void FilesTest::Inspect()
//----------------------------------------------------------------------------------------
{
    mConsole << nsEndl << "^^^^^^^^^^ PLEASE INSPECT OUTPUT FOR ERRORS" << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::WriteStuff(nsOutputStream& s)
//----------------------------------------------------------------------------------------
{
    // Initialize a URL from a string without suffix.  Change the path to suit your machine.
    nsFileURL fileURL("file:///Development/MPW/MPW%20Shell", PR_FALSE);
    s << "File URL initialized to:     \"" << fileURL << "\""<< nsEndl;
    
    // Initialize a Unix path from a URL
    nsFilePath filePath(fileURL);
    s << "As a unix path:              \"" << (const char*)filePath << "\""<< nsEndl;
    
    // Initialize a native file spec from a URL
    nsFileSpec fileSpec(fileURL);
    s << "As a file spec:               " << fileSpec << nsEndl;
    
    // Make the spec unique (this one has no suffix).
    fileSpec.MakeUnique();
    s << "Unique file spec:             " << fileSpec << nsEndl;
    
    // Assign the spec to a URL
    fileURL = fileSpec;
    s << "File URL assigned from spec: \"" << fileURL << "\""<< nsEndl;
    
    // Assign a unix path using a string with a suffix.
    filePath = "/Development/MPW/SysErrs.err";
    s << "File path reassigned to:     \"" << (const char*)filePath << "\""<< nsEndl;    
    
    // Assign to a file spec using a unix path.
    fileSpec = filePath;
    s << "File spec reassigned to:      " << fileSpec << nsEndl;
    
    // Make this unique (this one has a suffix).
    fileSpec.MakeUnique();
    s << "File spec made unique:        " << fileSpec << nsEndl;
} // WriteStuff

//----------------------------------------------------------------------------------------
int FilesTest::OutputStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE); // relative path.
    const char* pathAsString = (const char*)myTextFilePath;
    nsFileSpec mySpec(myTextFilePath);
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
	FileSizeAndDate(relativePath);
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
    Inspect();
    return 0;
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
    mConsole
        << "GetParent() on "
        << "\n\t" << pathAsString
        << "\n yields "
        << "\n\t" << (const char*)parentPath
        << nsEndl;
    Inspect();
    return 0;
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
    nsFileSpec dirSpec(aPath, PR_TRUE);
	mConsole
		<< "Testing nsFilePath(X, PR_TRUE) using"
		<< "\n\t" << (const char*)aPath
		<< nsEndl;
		
    return Passed();
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
        char* itemName = ((nsFileSpec&)i).GetLeafName();
        mConsole << '\t' << itemName << nsEndl;
        delete [] itemName;
    }

    mConsole << "Backwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator j(grandparent, -1); j.Exists(); j--)
    {
        char* itemName = ((nsFileSpec&)j).GetLeafName();
        mConsole << '\t' << itemName << nsEndl;
        delete [] itemName;
    }
    Inspect();
    return 0;
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
        mConsole
            << "ERROR: after initializing the path object with a relative path,"
            << "\n the path consisted of the string "
            << "\n\t" << pathAsString
            << "\n which is not a canonical full path!"
            << nsEndl;
        return -1;
    }
    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::FileSpecEquality(const char *aFile, const char *bFile)
//----------------------------------------------------------------------------------------
{
    nsFileSpec aFileSpec(aFile, PR_FALSE);
    nsFileSpec bFileSpec(bFile, PR_FALSE);
    nsFileSpec cFileSpec(bFile, PR_FALSE);  // this should == bFile
    
    if (aFileSpec != bFileSpec &&
        bFileSpec == cFileSpec )
    {
        return Passed();
    }

    return Failed();
}

//----------------------------------------------------------------------------------------
int FilesTest::Copy(const char* file, const char* dir)
//----------------------------------------------------------------------------------------
{
    nsFileSpec dirPath(dir, PR_TRUE);
        
    dirPath.CreateDirectory();
    if (! dirPath.Exists())
        return Failed();
    

    nsFileSpec mySpec(file, PR_TRUE); // relative path.
    {
        nsIOFileStream testStream(mySpec); // creates the file
        // Scope ends here, file gets closed
    }
    
    nsFileSpec filePath(file);
    if (! filePath.Exists())
        return Failed();
   
    nsresult error = filePath.Copy(dirPath);

    dirPath += filePath.GetLeafName();
    if (! dirPath.Exists() || ! filePath.Exists() || NS_FAILED(error))
        return Failed();

   return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::Move(const char* file, const char* dir)
//----------------------------------------------------------------------------------------
{
    nsFileSpec dirPath(dir, PR_TRUE);
        
    dirPath.CreateDirectory();
    if (! dirPath.Exists())
        return Failed();
    

    nsFileSpec srcSpec(file, PR_TRUE); // relative path.
    {
       nsIOFileStream testStream(srcSpec); // creates the file
       // file gets closed here because scope ends here.
    };
    
    if (! srcSpec.Exists())
        return Failed();
   
    nsresult error = srcSpec.Move(dirPath);


    dirPath += srcSpec.GetLeafName();
    if (! dirPath.Exists() || srcSpec.Exists() || NS_FAILED(error))
        return Failed();

    return Passed();
}

//----------------------------------------------------------------------------------------
int FilesTest::Execute(const char* appName, const char* args)
//----------------------------------------------------------------------------------------
{
    nsFileSpec appPath(appName, PR_FALSE);
    if (!appPath.Exists())
        return Failed();
    
    nsresult error = appPath.Execute(args);    
    if (NS_FAILED(error))
        return Failed();

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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        mConsole << systemDir << nsEndl;
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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
        (nsOutputStream)mConsole << systemDir << nsEndl;
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
	// Test of mConsole output
	
	mConsole << "WRITING TEST OUTPUT TO CONSOLE" << nsEndl << nsEndl;

	// Test of nsFileSpec
	
	Banner("Interconversion");
	WriteStuff(mConsole);
	Inspect();
		
	Banner("Canonical Path");
	if (CanonicalPath("mumble/iotest.txt") != 0)
		return -1;

	Banner("OutputStream");
	if (OutputStream("mumble/iotest.txt") != 0)
		return -1;
	
	Banner("InputStream");
	if (InputStream("mumble/iotest.txt") != 0)
		return -1;
	
	Banner("IOStream");
	if (IOStream("mumble/iotest.txt") != 0)
		return -1;
	FileSizeAndDate("mumble/iotest.txt");

	if (InputStream("mumble/iotest.txt") != 0)
		return -1;
	
    Banner("StringStream");
    if (StringStream() != 0)
        return -1;
        
	Banner("Parent");
	nsFileSpec parent;
	if (Parent("mumble/iotest.txt", parent) != 0)
		return -1;
		
	Banner("Delete");
	if (Delete(parent) != 0)
		return -1;
		
	Banner("CreateDirectory");
	if (CreateDirectory(parent) != 0)
		return -1;

    Banner("CreateDirectoryRecursive Relative (using nsFileSpec)");
	if (CreateDirectoryRecursive("mumble/dir1/dir2/dir3/") != 0)
		return -1;
#ifdef XP_PC
    Banner("CreateDirectoryRecursive Absolute (using nsFileSpec)");
	if (CreateDirectoryRecursive("c:\\temp\\dir1\\dir2\\dir3\\") != 0)
		return -1;
#endif

	Banner("IterateDirectoryChildren");
	if (IterateDirectoryChildren(parent) != 0)
		return -1;
    
    Banner("nsFileSpec equality");
    if (FileSpecEquality("mumble/a", "mumble/b") != 0)
        return -1;

    Banner("Copy");
    if (Copy("mumble/copyfile.txt", "mumble/copy") != 0)
        return -1;
    
    Banner("Move");
    if (Move("mumble/moveFile.txt", "mumble/move") != 0)
        return -1;
    Banner("Execute");
#ifdef XP_MAC
    // This path is hard-coded to test on jrm's machine.  Finding an app
    // on an arbitrary Macintosh would cost more trouble than it's worth.
    // Change path to suit.
    if NS_FAILED(Execute("/Projects/Nav45_BRANCH/ns/cmd/macfe/"\
        "projects/client45/Client45PPC", ""))
#elif XP_PC
    if NS_FAILED(Execute("c:\\windows\\notepad.exe", ""))
#else
    if NS_FAILED(Execute("/bin/ls", "/"))
#endif
        return -1;

    Banner("Special System Directories");
    if (SpecialSystemDirectories() != 0)
        return -1;

	Banner("Persistence");
	if (Persistence("mumble/filedesc.dat") != 0)
		return -1;

	Banner("Delete again (to clean up our mess)");
	if (Delete(parent) != 0)
		return -1;
		
    return 0;
} // FilesTest::RunAllTests

//----------------------------------------------------------------------------------------
int main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{
    FilesTest tester;
    return tester.RunAllTests();
} // main
