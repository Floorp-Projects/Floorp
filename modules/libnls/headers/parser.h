// parser.h: _PUBLIC_ interface for the FileParser class.
//
//////////////////////////////////////////////////////////////////////

#include "nlsprs.h"
#include "nlsxp.h"

#if !defined(PARSER_H_)
#define PARSER_H_

class NLSPRSAPI_PUBLIC_CLASS FileParser  
{
public:
	// take a filename, prepend path, suffix with export/resource designations,
	//  then recombine the two files.
	void mergeFile(char *filename, char *encoding);
	// fully-specified filename version (relative to current directory)
	//  just recombine. Called by single-parameter version.
	void mergeFile(char * exFile, char *exEnc, char * resFile, char *resEnc, char * outFile, char *outEnc);
	// take a filename, prepend path, suffix with export/resource designations,
	//  make export html and resource files.
	void separateFile(char *filename, char *encoding);
	// fully-specified filename version (relative to current directory)
	//  just separate. Called by single-parameter version.
	void separateFile(char *inFile, char *inEnc, char *exFile, char *exEnc, char *resFile, char *resEnc);
	// Instantiate a parser using the directory specified by PATH, relative
	//  to current directory if non-absolute.
	FileParser(char *path);
	// Instantiate a parser using the current directory for file searches.
	FileParser();
	// Return the current status of the parser.
	PRS_ErrorCode getStatus();
	static void getDefaultEncodings(char * & in, char * & ex, char * & res, char * & out);
	~FileParser();
};



#endif // !defined(PARSER_H_)
