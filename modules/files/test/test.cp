#include "nsFileSpec.h"
#include <iostream>

//----------------------------------------------------------------------------------------
void main()
//----------------------------------------------------------------------------------------
{
	nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
	cout << "File URL initialized to:     \"" << (string&)fileURL << "\""<< endl;
	nsUnixFilePath filePath(fileURL);
	cout << "As a unix path:              \"" << (string&)filePath << "\""<< endl;
	nsNativeFileSpec fileSpec(fileURL);
	cout << "As a file spec:               " << fileSpec << endl;
	fileSpec.MakeUnique();
	cout << "Unique file spec:             " << fileSpec << endl;
	fileURL = fileSpec;
	cout << "File URL assigned from spec: \"" << (string&)fileURL << "\""<< endl;
	filePath = "/Development/MPW/SysErrs.err";
	cout << "File path reassigned to:     \"" << (string&)filePath << "\""<< endl;
	cout << flush; // ?
	
	fileSpec = filePath;
	cout << "File spec reassigned to:      " << fileSpec << endl;
	fileSpec.MakeUnique();
	cout << "File spec made unique:        " << fileSpec << endl;
}