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

//#include "string.h"

struct FilesTest
{
	FilesTest() : mConsole() {}

	int RunAllTests();
	
	void WriteStuff(nsOutputFileStream& s);
	int InputStream(const char* relativePath);
	int OutputStream(const char* relativePath);
	int IOStream(const char* relativePath);
	int Parent(const char* relativePath, nsFileSpec& outParent);
	int Delete(nsFileSpec& victim);
	int CreateDirectory(nsFileSpec& victim);
	int IterateDirectoryChildren(nsFileSpec& startChild);
	int CanonicalPath(const char* relativePath);
	int Persistence(const char* relativePath);

    int Copy(const char*  sourceFile, const char* targDir);
    int Move(const char*  sourceFile, const char*  targDir);
    int Rename(const char*  sourceFile, const char* newName);

    int Execute(const char* appName, const char* args);

	void Banner(const char* bannerString);
	void Passed();
	void Failed();
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
void FilesTest::Passed()
//----------------------------------------------------------------------------------------
{
	((nsOutputStream&)mConsole) << "Test passed.";
	mConsole << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::Failed()
//----------------------------------------------------------------------------------------
{
	mConsole << "ERROR: Test failed." << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::Inspect()
//----------------------------------------------------------------------------------------
{
	mConsole << nsEndl << "^^^^^^^^^^ PLEASE INSPECT OUTPUT FOR ERRORS" << nsEndl;
}

//----------------------------------------------------------------------------------------
void FilesTest::WriteStuff(nsOutputFileStream& s)
//----------------------------------------------------------------------------------------
{
	// Initialize a URL from a string without suffix.  Change the path to suit your machine.
	nsFileURL fileURL("file:///Development/MPW/MPW%20Shell", false);
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
	nsFilePath myTextFilePath(relativePath, true); // relative path.
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
	}	// <-- Scope closes the stream (and the file).

	if (!mySpec.Exists() || mySpec.IsDirectory() || !mySpec.IsFile())
	{
			mConsole
			    << "ERROR: File "
			    << pathAsString
			    << " is not a file (cela n'est pas un pipe)"
			    << nsEndl;
			return -1;
	}
	Passed();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::IOStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true); // relative path.
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
	char line[1000];
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
	nsFilePath myTextFilePath(relativePathToWrite, true);
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
	{
		Failed();
		return -1;
	}
#endif

    if (!mySpec.Exists())
	{
		Failed();
		return -1;
	}
    
	Passed();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::InputStream(const char* relativePath)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true);
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
int FilesTest::Parent(
	const char* relativePath,
	nsFileSpec& outParent)
//----------------------------------------------------------------------------------------
{
	nsFilePath myTextFilePath(relativePath, true);
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
	victim.Delete(false);	
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
	victim.Delete(true);
	if (victim.Exists())
	{
		mConsole
		    << "ERROR: Directory "
		    << "\n\t" << (const char*)victimPath
		    << "\n has NOT been deleted despite the recursion option!"
		    << nsEndl;
		return -1;
	}
	
	Passed();
	return 0;
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
	if (dirSpec.Exists())
		Passed();
	else
	{
		Failed();
		return -1;
	}
	dirSpec.Delete(true);
	return 0;
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
	nsFilePath myTextFilePath(relativePath, true);
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
	Passed();
	return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Copy(const char* file, const char* dir)
//----------------------------------------------------------------------------------------
{
    nsFileSpec dirPath(dir, true);
		
	dirPath.CreateDirectory();
    if (! dirPath.Exists())
	{
		Failed();
		return -1;
	}
    

    nsFileSpec mySpec(file, true); // relative path.
    {
	    nsIOFileStream testStream(mySpec); // creates the file
	    // Scope ends here, file gets closed
    }
    
    nsFileSpec filePath(file);
    if (! filePath.Exists())
	{
		Failed();
		return -1;
	}
   
    nsresult error = filePath.Copy(dirPath);

    dirPath += filePath.GetLeafName();
    if (! dirPath.Exists() || ! filePath.Exists() || NS_FAILED(error))
	{
		Failed();
		return -1;
	}

   Passed();
   
   return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Move(const char* file, const char* dir)
//----------------------------------------------------------------------------------------
{
    nsFileSpec dirPath(dir, true);
		
	dirPath.CreateDirectory();
    if (! dirPath.Exists())
	{
		Failed();
		return -1;
	}
    

    nsFileSpec srcSpec(file, true); // relative path.
    {
       nsIOFileStream testStream(srcSpec); // creates the file
       // file gets closed here because scope ends here.
    };
    
    if (! srcSpec.Exists())
	{
		Failed();
		return -1;
	}
   
    nsresult error = srcSpec.Move(dirPath);


    dirPath += srcSpec.GetLeafName();
    if (! dirPath.Exists() || srcSpec.Exists() || NS_FAILED(error))
	{
		Failed();
		return -1;
	}

    Passed();
    return 0;
}

//----------------------------------------------------------------------------------------
int FilesTest::Execute(const char* appName, const char* args)
//----------------------------------------------------------------------------------------
{
    nsFileSpec appPath(appName, false);
    if (!appPath.Exists())
	{
		Failed();
		return -1;
	}
	
    nsresult error = appPath.Execute(args);    
    if (NS_FAILED(error))
	{
		Failed();
		return -1;
	}

   Passed();

    return 0;
}

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
	if (InputStream("mumble/iotest.txt") != 0)
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

	Banner("IterateDirectoryChildren");
	if (IterateDirectoryChildren(parent) != 0)
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

	Banner("Persistence");
	if (Persistence("mumble/filedesc.dat") != 0)
		return -1;

	Banner("Delete again (to clean up our mess)");
	if (Delete(parent) != 0)
		return -1;
		
    return 0;
}

//----------------------------------------------------------------------------------------
int main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{
	FilesTest tester;
	return tester.RunAllTests();
} // main
