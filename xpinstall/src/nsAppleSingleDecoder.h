/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */


/*----------------------------------------------------------------------*
 *   Implements a simple AppleSingle decoder, as described in RFC1740 
 *	 http://andrew2.andrew.cmu.edu/rfc/rfc1740.html
 *----------------------------------------------------------------------*/

#ifndef macintosh
#error Sorry! This is Mac only functionality!
#endif

#pragma options align=mac68k

#ifndef _NS_APPLESINGLEDECODER_H_
#define _NS_APPLESINGLEDECODER_H_

#include <stdlib.h>
#include <string.h>
#include <Files.h>
#include <Errors.h>


/*----------------------------------------------------------------------*
 *   Struct definitions from RFC1740
 *----------------------------------------------------------------------*/
typedef struct ASHeader /* header portion of AppleSingle */ 
{
    /* AppleSingle = 0x00051600; AppleDouble = 0x00051607 */
       UInt32 magicNum; 	/* internal file type tag */ 
       UInt32 versionNum; 	/* format version: 2 = 0x00020000 */ 
       UInt8 filler[16]; 	/* filler, currently all bits 0 */ 
       UInt16 numEntries; 	/* number of entries which follow */
} ASHeader ; /* ASHeader */ 

typedef struct ASEntry /* one AppleSingle entry descriptor */ 
{
	UInt32 entryID; 	/* entry type: see list, 0 invalid */
	UInt32 entryOffset; /* offset, in octets, from beginning */
	                    /* of file to this entry's data */
	UInt32 entryLength; /* length of data in octets */
} ASEntry; /* ASEntry */

typedef struct ASFinderInfo
{
	FInfo ioFlFndrInfo; 	/* PBGetFileInfo() or PBGetCatInfo() */
	FXInfo ioFlXFndrInfo; 	/* PBGetCatInfo() (HFS only) */
} ASFinderInfo; /* ASFinderInfo */

typedef struct ASMacInfo        /* entry ID 10, Macintosh file information */
{
       UInt8 filler[3]; 	/* filler, currently all bits 0 */ 
       UInt8 ioFlAttrib; 	/* PBGetFileInfo() or PBGetCatInfo() */
} ASMacInfo;

typedef struct ASFileDates      /* entry ID 8, file dates info */
{
	SInt32 create; /* file creation date/time */
	SInt32 modify; /* last modification date/time */
	SInt32 backup; /* last backup date/time */
	SInt32 access; /* last access date/time */
} ASFileDates; /* ASFileDates */

/* entryID list */
#define AS_DATA         1 /* data fork */
#define AS_RESOURCE     2 /* resource fork */
#define AS_REALNAME     3 /* File's name on home file system */
#define AS_COMMENT      4 /* standard Mac comment */
#define AS_ICONBW       5 /* Mac black & white icon */
#define AS_ICONCOLOR    6 /* Mac color icon */
   		/*              7       /* not used */
#define AS_FILEDATES    8 /* file dates; create, modify, etc */
#define AS_FINDERINFO   9 /* Mac Finder info & extended info */
#define AS_MACINFO      10 /* Mac file info, attributes, etc */
#define AS_PRODOSINFO   11 /* Pro-DOS file info, attrib., etc */
#define AS_MSDOSINFO    12 /* MS-DOS file info, attributes, etc */
#define AS_AFPNAME      13 /* Short name on AFP server */
#define AS_AFPINFO      14 /* AFP file info, attrib., etc */

#define AS_AFPDIRID     15 /* AFP directory ID */


/*----------------------------------------------------------------------*
 *   Macros
 *----------------------------------------------------------------------*/
#define MAC_ERR_CHECK(_funcCall) 	\
	err = _funcCall; 				\
	if (err!=noErr) 				\
		if (err==-37)				\
			DebugStr("\perror -37:bdNamErr\r");		\
		else if (err==-43)			\
			DebugStr("\perror -43: fnfErr\r");	\
		else if (err==-48)			\
			DebugStr("\p error -48: dupFNErr");	\
		else						\
			return err;
	
	

class nsAppleSingleDecoder 
{

public:
	nsAppleSingleDecoder(FSSpec *inSpec, FSSpec *outSpec);
	nsAppleSingleDecoder();
	~nsAppleSingleDecoder();
		
	/** 
	 * Decode
	 *
	 * Takes an "in" FSSpec for the source file in AppleSingle
	 * format to decode and write out to an "out" FSSpec.
	 * This form is used when the Decode(void) method has already
	 * been invoked once and this object is reused to decode 
	 * another AppleSingled file: useful in iteration to avoid
	 * nsAppleSingleDecoder object instantiation per file.
	 *
	 * @param  inSpec 	the AppleSingled file to decode
	 * @param  outSpec 	the destination file in which the decoded
	 *					data was written out (empty when passed in
	 *					and filled on return)
	 * @return err		a standard MacOS OSErr where noErr means OK
	 */
	OSErr Decode(FSSpec *inSpec, FSSpec *outSpec);
		
	/**
	 * Decode
	 *
	 * Decodes the AppleSingled file passed in to the constructor
	 * and writes out the decoded data to the outSpec passed to the 
	 * constructor.
	 *
	 * @return err		a standard MacOS OSErr where noErr = OK
	 */
	OSErr Decode();
	
	/**
	 * IsAppleSingleFile
	 *
	 * Checks the file header to see whether this is an AppleSingle
	 * version 2 file by matching the magicNum field in the header.
	 *
	 * @param  inSpec			the file to check
	 * @return bAppleSingle		a Boolean where true indicates this is
	 *							in fact an AppleSingle file
	 */
	static Boolean IsAppleSingleFile(FSSpec *inSpec);
	
	/**
	 * String utilities to ensure building standalone
	 * since Mozilla doesn't use PLStringFuncs.
	 */
	static StringPtr PLstrncpy(StringPtr dst, ConstStr255Param src, short max);
	static StringPtr PLstrncat(StringPtr dst, ConstStr255Param src, short max);
	static Boolean   PLstrcmp(StringPtr str1, StringPtr str2);
	
	/**
 	 * ltoa -- long to ascii
 	 *
 	 * Converts a long to a C string. We allocate 
 	 * a string of the appropriate size and the caller
 	 * should assume ownership of the returned pointer.
 	 */
	static char *ltoa(long n);
	
private:
	FSSpec	*mInSpec;
	FSSpec	*mOutSpec;
	short	mInRefNum;	// cache since it's used through the life of one Decode cycle
	Boolean	mRenameReqd;
	
	OSErr	ProcessASEntry(ASEntry inEntry);	
	OSErr	ProcessDataFork(ASEntry inEntry);
	OSErr	ProcessResourceFork(ASEntry inEntry);
	OSErr	ProcessRealName(ASEntry inEntry);
	OSErr	ProcessFileDates(ASEntry inEntry);
	OSErr	ProcessFinderInfo(ASEntry inEntry);
	OSErr	ProcessMacInfo(ASEntry inEntry);
	OSErr	EntryToMacFile(ASEntry inEntry, UInt16 inTargetSpecRefNum);
	
	OSErr	FSMakeUnique(FSSpec *ioSpec);
};



#endif /* _NS_APPLESINGLEDECODER_H_ */