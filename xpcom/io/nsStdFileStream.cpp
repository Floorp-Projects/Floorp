/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
//	First checked in on 98/12/08 by John R. McMullen.
//  Since nsFileStream.h is entirely templates, common code (such as open())
//  which does not actually depend on the charT, can be placed here.

#ifdef XP_UNIX
// Compile the un-inlined functions in this file only.
#define DEFINING_FILE_STREAM
#endif

#include "nsStdFileStream.h"

#ifdef XP_MAC
#include <Errors.h>
#endif

//----------------------------------------------------------------------------------------
PRFileDesc* nsFileStreamHelpers::open(
	const nsFilePath& inFile,
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    PRFileDesc* descriptor = 0;
    const IOS_BASE::openmode valid_modes[]=
    {
        IOS_BASE::out, 
        IOS_BASE::out | IOS_BASE::app, 
        IOS_BASE::out | IOS_BASE::trunc, 
        IOS_BASE::in, 
        IOS_BASE::in  | IOS_BASE::out, 
        IOS_BASE::in  | IOS_BASE::out    | IOS_BASE::trunc, 
//      IOS_BASE::out | IOS_BASE::binary, 
//      IOS_BASE::out | IOS_BASE::app    | IOS_BASE::binary, 
//      IOS_BASE::out | IOS_BASE::trunc  | IOS_BASE::binary, 
//      IOS_BASE::in  | IOS_BASE::binary, 
//      IOS_BASE::in  | IOS_BASE::out    | IOS_BASE::binary, 
//      IOS_BASE::in  | IOS_BASE::out    | IOS_BASE::trunc | IOS_BASE::binary,
        0 
    };

    const int nspr_modes[]={
        PR_WRONLY | PR_CREATE_FILE,
        PR_WRONLY | PR_CREATE_FILE | PR_APPEND,
        PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
        PR_RDONLY,
        PR_RDONLY | PR_APPEND,
        PR_RDWR | PR_CREATE_FILE,
        PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
//      "wb",
//      "ab", 
//      "wb",
//      "rb",
//      "r+b",
//      "w+b",
        0 };
    int ind=0;
    while (valid_modes[ind] && valid_modes[ind] != (mode&~IOS_BASE::ate))
        ++ind;
    if (!nspr_modes[ind]) 
        return 0;

#ifdef XP_MAC
     // Use the file spec to open the file, because one path can be common to
     // several files on the Macintosh (you can have several volumes with the
     // same name, see).
    descriptor = 0;
    if (inFile.GetNativeSpec().Error() != noErr)
        return 0;
    OSErr err = noErr;
#if DEBUG
	const OSType kCreator = 'CWIE';
#else
    const OSType kCreator = 'MOSS';
#endif
	nsNativeFileSpec nativeSpec = inFile.GetNativeSpec();
    FSSpec* spec = (FSSpec*)nativeSpec;
    if (nspr_modes[ind] & PR_CREATE_FILE)
    	err = FSpCreate(spec, kCreator, 'TEXT', 0);
    if (err == dupFNErr)
    	err = noErr;
    if (err != noErr)
       return 0;
    
    SInt8 perm;
    if (nspr_modes[ind] & PR_RDWR)
       perm = fsRdWrPerm;
    else if (nspr_modes[ind] & PR_WRONLY)
       perm = fsWrPerm;
    else
       perm = fsRdPerm;

    short refnum;
    err = FSpOpenDF(spec, perm, &refnum);

    if (err == noErr && (nspr_modes[ind] & PR_TRUNCATE))
    	err = SetEOF(refnum, 0);
    if (err == noErr && (nspr_modes[ind] & PR_APPEND))
    	err = SetFPos(refnum, fsFromLEOF, 0);
    if (err != noErr)
       return 0;

    if ((descriptor = PR_ImportFile(refnum)) == 0)
    	return 0;
#else
	//	Platforms other than Macintosh...
    if ((descriptor = PR_Open(inFile, nspr_modes[ind], accessMode)) != 0)
#endif
       if (mode&IOS_BASE::ate && PR_Seek(descriptor, 0, PR_SEEK_END) >= 0)
       {
          PR_Close(descriptor);
          descriptor = 0;
          return 0;
       }
    return descriptor;
} // nsFileStreamHelpers::open
