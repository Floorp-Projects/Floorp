/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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

	