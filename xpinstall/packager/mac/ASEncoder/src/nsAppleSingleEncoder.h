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
 *   Implements a simple AppleSingle encoder, as described in RFC1740 
 *	 http://andrew2.andrew.cmu.edu/rfc/rfc1740.html
 *----------------------------------------------------------------------*/

#ifndef macintosh
#error Sorry! This is Mac only functionality!
#endif

#pragma options align=mac68k
 
#ifndef _NS_APPLESINGLEENCODER_H_
#define _NS_APPLESINGLEENCODER_H_

#include <MacTypes.h>
#include <Files.h>

class nsAppleSingleEncoder
{

public:
	nsAppleSingleEncoder();
	~nsAppleSingleEncoder();
	
	static Boolean		HasResourceFork(FSSpecPtr aFile);
	OSErr				Encode(FSSpecPtr aFile);
	OSErr				Encode(FSSpecPtr aInFile, FSSpecPtr aOutFile);
	OSErr				EncodeFolder(FSSpecPtr aFolder);
	void				ReInit();
	
	static OSErr		FSpGetCatInfo(CInfoPBRec *pb, FSSpecPtr aFile);
	
private:
	FSSpecPtr			mInFile;
	FSSpecPtr			mOutFile;
	FSSpecPtr			mTransient;
	short				mTransRefNum;
	
	OSErr				WriteHeader();
	OSErr				WriteEntryDescs();
	OSErr				WriteEntries();
	OSErr				WriteResourceFork();
	OSErr				WriteDataFork();
};

#ifdef __cplusplus
extern "C" {
#endif

pascal void
EncodeDirIterateFilter(const CInfoPBRec * const cpbPtr, Boolean *quitFlag, void *yourDataPtr);

#ifdef __cplusplus
}
#endif

#define kTransientName			"\pzz__ASEncoder_TMP__zz"
#define kAppleSingleMagicNum 	0x00051600
#define kAppleSingleVerNum		0x00020000
#define kNumASEntries			6
#define kConvertTime 			1265437696L

#define ERR_CHECK(_func) 			\
			err = _func;			\
			if (err != noErr)		\
				return err;

#pragma options align=reset

#endif /* _NS_APPLESINGLEENCODER_H_ */

	