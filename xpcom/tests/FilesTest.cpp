#include "nsFileSpec.h"
#include "nsFileStream.h"

#ifdef NS_USING_STL
using std::endl;
using std::cout;
#endif

NS_NAMESPACE FileTest
{
	NS_NAMESPACE_PROTOTYPE void WriteStuff(nsOutputFileStream& s);
} NS_NAMESPACE_END

//----------------------------------------------------------------------------------------
void FileTest::WriteStuff(nsOutputFileStream& s)
//----------------------------------------------------------------------------------------
{
	// Initialize a URL from a string without suffix.  Change the path to suit your machine.
	nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
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
void main()
// For use with DEBUG defined.
//----------------------------------------------------------------------------------------
{

	// Test of nsFileSpec
	
	nsOutputFileStream nsOut(cout);
	nsOut << "WRITING TEST OUTPUT TO cout" << nsEndl << nsEndl;

	FileTest::WriteStuff(nsOut);
	nsOut << nsEndl << nsEndl;
	
	// Test of nsOutputFileStream

	nsFilePath myTextFilePath("iotest.txt");

	{
		nsOut << "WRITING IDENTICAL OUTPUT TO " << (const char*)myTextFilePath << nsEndl << nsEndl;
		nsOutputFileStream testStream(myTextFilePath);
		NS_ASSERTION(testStream.is_open(), "File could not be opened");
		FileTest::WriteStuff(testStream);
	}	// <-- Scope closes the stream (and the file).

	// Test of nsInputFileStream

	{
		nsOut << "READING BACK DATA FROM " << (const char*)myTextFilePath << nsEndl << nsEndl;
		nsInputFileStream testStream2(myTextFilePath);
		NS_ASSERTION(testStream2.is_open(), "File could not be opened");
		char line[1000];
		
		testStream2.seek(0); // check that the seek compiles
		while (!testStream2.eof())
		{
			testStream2.readline(line, sizeof(line));
			nsOut << line << nsEndl;
		}
	}
} // main
