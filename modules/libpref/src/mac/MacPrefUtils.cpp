/*
	MacPrefUtils.cpp
	
	File utilities used by macpref.cp. These come from a variety of sources, and
	should eventually be supplanted by John McMullen's nsFile utilities.

	Based on ufilemgr.cp by atotic.
	
	by Patrick C. Beard.
 */

#include "MacPrefUtils.h"

#ifndef __FILES__
#include <Files.h>
#endif
#ifndef __ERRORS__
#include <Errors.h>
#endif
#ifndef __TEXTUTILS__
#include <TextUtils.h>
#endif
#ifndef __ALIASES__
#include <Aliases.h>
#endif

#include <string.h>
#include "prmem.h"
#include "plstr.h"
#include "FullPath.h"
#include "xp_mem.h" // for XP_FREE


/*
 * valid mask values for NET_Escape() and NET_EscapedSize().
 */
#define URL_XALPHAS     (unsigned char) 1
#define URL_XPALPHAS    (unsigned char) 2
#define URL_PATH        (unsigned char) 4

char * NET_Escape (const char * str, int mask);
char * NET_EscapeBytes (const char * str, int32 len, int mask, int32 * out_len);
int NET_UnEscapeCnt (char * str);
char * NET_UnEscape (char * str);

// Returns a full pathname to the given file
// Returned value is allocated with XP_ALLOC, and must be freed with XP_FREE
// This is taken from FSpGetFullPath in MoreFiles, except that we need to tolerate
// fnfErr.

static char* pathNameFromFSSpec( const FSSpec& inSpec, Boolean wantLeafName )
{	
	char* result = nil;
	FSSpec tempSpec;
	OSErr err = noErr;
	
	short fullPathLength = 0;
	Handle fullPath = NULL;
	
	/* Make a copy of the input FSSpec that can be modified */
	BlockMoveData(&inSpec, &tempSpec, sizeof(FSSpec));
	
	if ( tempSpec.parID == fsRtParID )
	{
		/* The object is a volume */
		
		/* Add a colon to make it a full pathname */
		++tempSpec.name[0];
		tempSpec.name[tempSpec.name[0]] = ':';
		
		/* We're done */
		err = PtrToHand(&tempSpec.name[1], &fullPath, tempSpec.name[0]);
	}
	else
	{
		/* The object isn't a volume */
		
		CInfoPBRec	pb = { 0 };
		Str63 dummyFileName = { "\p" };

		/* Is the object a file or a directory? */
		pb.dirInfo.ioNamePtr = (! tempSpec.name[0] ) ? (StringPtr)dummyFileName : tempSpec.name;
		pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
		pb.dirInfo.ioDrDirID = tempSpec.parID;
		pb.dirInfo.ioFDirIndex = 0;
		err = PBGetCatInfoSync(&pb);
		if ( err == noErr || err == fnfErr)
		{
			// if the object is a directory, append a colon so full pathname ends with colon
			// Beware of the "illegal spec" case that Netscape uses (empty name string). In
			// this case, we don't want the colon.
			if ( err == noErr && tempSpec.name[0] && (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				++tempSpec.name[0];
				tempSpec.name[tempSpec.name[0]] = ':';
			}
			
			/* Put the object name in first */
			err = PtrToHand(&tempSpec.name[1], &fullPath, tempSpec.name[0]);
			if ( err == noErr )
			{
				/* Get the ancestor directory names */
				pb.dirInfo.ioNamePtr = tempSpec.name;
				pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
				pb.dirInfo.ioDrParID = tempSpec.parID;
				do	/* loop until we have an error or find the root directory */
				{
					pb.dirInfo.ioFDirIndex = -1;
					pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
					err = PBGetCatInfoSync(&pb);
					if ( err == noErr )
					{
						/* Append colon to directory name */
						++tempSpec.name[0];
						tempSpec.name[tempSpec.name[0]] = ':';
						
						/* Add directory name to beginning of fullPath */
						(void) Munger(fullPath, 0, NULL, 0, &tempSpec.name[1], tempSpec.name[0]);
						err = MemError();
					}
				} while ( err == noErr && pb.dirInfo.ioDrDirID != fsRtDirID );
			}
		}
	}
	if ( err != noErr && err != fnfErr)
		goto Clean;

	fullPathLength = GetHandleSize(fullPath);
	err = noErr;	
	int allocSize = 1 + fullPathLength;
	// We only want the leaf name if it's the root directory or wantLeafName is true.
	if (inSpec.parID != fsRtParID && !wantLeafName)
		allocSize -= inSpec.name[0];
	result = (char*)PR_Malloc(allocSize);
	if (!result)
		goto Clean;
	BlockMoveData(*fullPath, result, allocSize - 1);
	// memcpy(result, *fullPath, allocSize - 1);
	result[ allocSize - 1 ] = 0;
Clean:
	if (fullPath)
		DisposeHandle(fullPath);
	return result;
}

/**
 * Swaps ':' with '/'
 */
static void swapSlashColon(char * s)
{
	for (char c = *s; c != 0; ++s) {
		if (c == '/')
			*s = ':';
		else if (c == ':')
			*s = '/';
	}
}

/**
 *	Transforms Macintosh style path into Unix one
 *	Method: Swap ':' and '/', hex escape the result
 */
static char* encodeMacPath(char* inPath, Boolean prependSlash)
{
	if (inPath == NULL)
		return NULL;
	int pathSize = strlen(inPath);
	
	// XP code sometimes chokes if there's a final slash in the unix path.
	// Since correct mac paths to folders and volumes will end in ':', strip this
	// first.
	char* c = inPath + pathSize - 1;
	if (*c == ':') {
		*c = 0;
		pathSize--;
	}

	char * newPath = NULL;
	char * finalPath = NULL;
	
	if (prependSlash) {
		newPath = (char*) PR_Malloc(pathSize + 2);
		newPath[0] = ':';	// It will be converted to '/'
		BlockMoveData(inPath, &newPath[1], pathSize + 1);
	} else {
		newPath = (char*) PR_Malloc(pathSize + 1);
		if (newPath != NULL)
			BlockMoveData(inPath, newPath, pathSize + 1);
	}
	
	if (newPath != NULL) {
		swapSlashColon(newPath);
		finalPath = NET_Escape(newPath, URL_PATH);
		XP_FREE(newPath);
	}

	PR_Free( inPath );
	return finalPath;
}

char* EncodedPathNameFromFSSpec(const FSSpec& inSpec, Boolean wantLeafName)
{
	return encodeMacPath(pathNameFromFSSpec(inSpec, wantLeafName), true);
}

static char* macPathFromUnixPath(const char* unixPath)
{
	// Relying on the fact that the unix path is always longer than the mac path:
	size_t len = strlen(unixPath);
	char* result = (char*)PR_Malloc(len + 2); // ... but allow for the initial colon in a partial name
	if (result != NULL) {
		char* dst = result;
		const char* src = unixPath;
		if (*src == '/')		 	// ¥ full path
			src++;
		else if (strchr(src, '/'))	// ¥ partial path, and not just a leaf name
			*dst++ = ':';
		strcpy(dst, src);
		NET_UnEscape(dst);		// Hex Decode
		swapSlashColon(dst);
	}
	return result;
}

struct CStr255 {
	Str255 pstr;
	
	CStr255(const char* str, int length)
	{
		pstr[0] = length;
		::BlockMoveData(str, pstr + 1, pstr[0]);
	}
	
	operator StringPtr ()
	{
		return pstr;
	}
};

static OSErr FSSpecFromPathname(const char* path, FSSpec* outSpec)
{
	// Simplify this routine to use FSMakeFSSpec if length < 255. Otherwise use the MoreFiles
	// routine FSpLocationFromFullPath, which allocates memory, to handle longer pathnames. 
	int pathLength = strlen(path);
	if (pathLength <= 255) {
		CStr255 pathStr(path, pathLength);
		return ::FSMakeFSSpec(0, 0, pathStr, outSpec);
	}
	return FSpLocationFromFullPath(pathLength, path, outSpec);
}

//-----------------------------------
// File spec from URL. Reverses GetURLFromFileSpec 
// Its input is only the <path> part of the URL
// JRM 97/01/08 changed this so that if it's a partial path (doesn't start with '/'),
// then it is combined with inOutSpec's vRefNum and parID to form a new spec.
//-----------------------------------

OSErr FSSpecFromLocalUnixPath(const char * unixPath, FSSpec * inOutSpec, Boolean resolveAlias)
{
	if (unixPath == NULL)
		return badFidErr;
	
	char* macPath = macPathFromUnixPath(unixPath);
	if (!macPath)
		return memFullErr;

	OSErr err = noErr;
	if (*unixPath == '/' /*full path*/)
		err = FSSpecFromPathname(macPath, inOutSpec);
	else {
		CStr255 pathStr(macPath, strlen(macPath));
		err = ::FSMakeFSSpec(inOutSpec->vRefNum, inOutSpec->parID, pathStr, inOutSpec);
	}
	if (err == fnfErr)
		err = noErr;
	Boolean dummy, dummy2;	
	if (err == noErr && resolveAlias)	// Added 
		err = ::ResolveAliasFile(inOutSpec,TRUE,&dummy,&dummy2);
	PR_Free(macPath);
	return err;
}

Boolean FileExists(const FSSpec& fsSpec)
{	
	FSSpec		temp;
	OSErr		err;
	
	err = FSMakeFSSpec( fsSpec.vRefNum, fsSpec.parID, fsSpec.name, &temp );
	
	return (err == noErr);
}


// beard:  brought over from mkparse.c to break dependency.

/* encode illegal characters into % escaped hex codes.
 *
 * mallocs and returns a string that must be freed
 */
static int netCharType[256] =
/*	Bit 0		xalpha		-- the alphas
**	Bit 1		xpalpha		-- as xalpha but 
**                             converts spaces to plus and plus to %20
**	Bit 3 ...	path		-- as xalphas but doesn't escape '/'
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x */
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 1x */
		 0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,	/* 2x   !"#$%&'()*+,-./	 */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?	 */
	     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 4x  @ABCDEFGHIJKLMNO  */
	     /* bits for '@' changed from 7 to 0 so '@' can be escaped   */
	     /* in usernames and passwords in publishing.                */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,	/* 5X  PQRSTUVWXYZ[\]^_	 */
	     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 6x  `abcdefghijklmno	 */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,	/* 7X  pqrstuvwxyz{\}~	DEL */
		 0, };

#define HEX_ESCAPE '%'

#define IS_OK(C) (netCharType[((unsigned int) (C))] & (mask))

char *
NET_Escape (const char * str, int mask)
{
    if(!str)
        return NULL;
    return NET_EscapeBytes (str, (int32)PL_strlen(str), mask, NULL);
}

char *
NET_EscapeBytes (const char * str, int32 len, int mask, int32 * out_len)
{
    register const unsigned char *src;
    register unsigned char *dst;
    char        *result;
    int32       i, extra = 0;
    char        *hexChars = "0123456789ABCDEF";

	if(!str)
		return(0);

	src = (unsigned char *) str;
    for(i = 0; i < len; i++)
	  {
        if (!IS_OK(src[i]))
            extra+=2; /* the escape, plus an extra byte for each nibble */
	  }

    if(!(result = (char *) PR_Malloc(len + extra + 1)))
        return(0);

    dst = (unsigned char *) result;
    for(i = 0; i < len; i++)
	  {
		unsigned char c = src[i];
		if (IS_OK(c))
		  {
			*dst++ = c;
		  }
		else if(mask == URL_XPALPHAS && c == ' ')
		  {
			*dst++ = '+'; /* convert spaces to pluses */
		  }
		else 
		  {
			*dst++ = HEX_ESCAPE;
			*dst++ = hexChars[c >> 4];		/* high nibble */
			*dst++ = hexChars[c & 0x0f];	/* low nibble */
		  }
	  }

    *dst = '\0';     /* tack on eos */
	if(out_len)
		*out_len = dst - (unsigned char *) result;
    return result;
}

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

int
NET_UnEscapeCnt (char * str)
{
    register char *src = str;
    register char *dst = str;

    while(*src)
        if (*src != HEX_ESCAPE)
		  {
        	*dst++ = *src++;
		  }
        else 	
		  {
        	src++; /* walk over escape */
        	if (*src)
              {
            	*dst = UNHEX(*src) << 4;
            	src++;
              }
        	if (*src)
              {
            	*dst = (*dst + UNHEX(*src));
            	src++;
              }
        	dst++;
          }

    *dst = 0;

    return (int)(dst - str);

} /* NET_UnEscapeCnt */

char *
NET_UnEscape(char * str)
{
	(void)NET_UnEscapeCnt(str);

	return str;
}
