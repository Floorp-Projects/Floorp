#include "nsFileSpec.h"
#include "nsFileStream.h"

NS_NAMESPACE FilesTest
{
	NS_NAMESPACE_PROTOTYPE void WriteStuff(nsOutputFileStream& s);
} NS_NAMESPACE_END

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
	nsNativeFileSpec fileSpec(fileURL);
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
int main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{

	// Test of console output
	
	nsOutputFileStream nsOut;
	nsOut << "WRITING TEST OUTPUT TO cout" << nsEndl << nsEndl;

	// Test of nsFileSpec
	
	FilesTest::WriteStuff(nsOut);
	nsOut << nsEndl << nsEndl;
	
	// - Test of nsOutputFileStream

	nsFilePath myTextFilePath("mumble/iotest.txt", true); // relative path.
	const char* pathAsString = (const char*)myTextFilePath;
	if (*pathAsString != '/')
	{
		nsOut
		    << "ERROR: after initializing the path object with a relative path,"
		    << "\n the path consisted of the string "
		    << "\n\t" << pathAsString
		    << "\n which is not a canonical full path!"
		    << nsEndl;
		return -1;
	}
	
	nsNativeFileSpec mySpec(myTextFilePath);
	{
		nsOut << "WRITING IDENTICAL OUTPUT TO " << pathAsString << nsEndl << nsEndl;
		nsOutputFileStream testStream(myTextFilePath);
		if (!testStream.is_open())
		{
			nsOut
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
			nsOut
			    << "ERROR: File "
			    << pathAsString
			    << " is not a file (cela n'est pas un pipe)"
			    << nsEndl;
			return -1;
	}
	// - Test of nsInputFileStream

	{
		nsOut << "READING BACK DATA FROM " << pathAsString << nsEndl << nsEndl;
		nsInputFileStream testStream2(myTextFilePath);
		if (!testStream2.is_open())
		{
			nsOut
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
			nsOut << line << nsEndl;
		}
	}	// <-- Scope closes the stream (and the file).

	// - Test of GetParent()
	
    nsNativeFileSpec parent;
    mySpec.GetParent(parent);
    nsFilePath parentPath(parent);
	nsOut
		<< "GetParent() on "
		<< "\n\t" << pathAsString
		<< "\n yields "
		<< "\n\t" << (const char*)parentPath
		<< nsEndl;

	// - Test of non-recursive delete

	nsOut
		<< "Attempting to delete "
		<< "\n\t" << (const char*)parentPath
		<< "\n without recursive option (should fail)"	
		<< nsEndl;
	parent.Delete(false);	
	if (parent.Exists())
		nsOut << "Test passed." << nsEndl;
	else
	{
		nsOut
		    << "ERROR: File "
		    << "\n\t" << (const char*)parentPath
		    << "\n has been deleted without the recursion option,"
		    << "\n and is a nonempty directory!"
		    << nsEndl;
		return -1;
	}

	// - Test of recursive delete

	nsOut
		<< "Deleting "
		<< "\n\t" << (const char*)parentPath
		<< "\n with recursive option"
		<< nsEndl;
	parent.Delete(true);
	if (parent.Exists())
	{
		nsOut
		    << "ERROR: Directory "
		    << "\n\t" << (const char*)parentPath
		    << "\n has NOT been deleted despite the recursion option!"
		    << nsEndl;
		return -1;
	}
	else
		nsOut << "Test passed." << nsEndl;
	
	// - Test of CreateDirectory

	nsOut
		<< "Testing CreateDirectory() using"
		<< "\n\t" << (const char*)parentPath
		<< nsEndl;
		
	parent.CreateDirectory();
	if (parent.Exists())
		nsOut << "Test passed." << nsEndl;
	else
	{
		nsOut << "ERROR: Test failed." << nsEndl;
		return -1;
	}
	parent.Delete(true);

#ifndef XP_PC // uncomment when tested and ready.

	// - Test of directory iterator

    nsNativeFileSpec grandparent;
    parent.GetParent(grandparent); // should be the original default directory.
    nsFilePath grandparentPath(grandparent);
    
    nsOut << "Forwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator i(grandparent, +1); i; i++)
    {
    	char* itemName = ((nsNativeFileSpec&)i).GetLeafName();
    	nsOut << '\t' << itemName << nsEndl;
    	delete [] itemName;
    }

    nsOut << "Backwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
    for (nsDirectoryIterator i(grandparent, -1); i; i--)
    {
    	char* itemName = ((nsNativeFileSpec&)i).GetLeafName();
    	nsOut << '\t' << itemName << nsEndl;
    	delete [] itemName;
    }
#endif

    return 0;
} // main
