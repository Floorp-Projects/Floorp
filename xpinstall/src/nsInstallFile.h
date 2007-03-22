/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
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


#ifndef nsInstallFile_h__
#define nsInstallFile_h__

#include "prtypes.h"
#include "nsString.h"

#include "nsInstallObject.h"

#include "nsInstall.h"
#include "nsInstallVersion.h"


/* Global defines for file handling mode bitfield values */
#define INSTALL_NO_COMPARE        0x1
#define INSTALL_IF_NEWER          0x2
#define INSTALL_IF_EQUAL_OR_NEWER 0x4


class nsInstallFile : public nsInstallObject 
{
    public:

      /*************************************************************
       * Public Methods 
       *
       *	Constructor
       *    inSoftUpdate    - softUpdate object we belong to
       *    inComponentName	- full path of the registry component
       *    inVInfo	        - full version info
       *    inJarLocation   - location inside the JAR file
       *    inFinalFileSpec	- final	location on	disk
       *************************************************************/
        nsInstallFile(  nsInstall* inInstall,
                        const nsString& inVRName,
                        const nsString& inVInfo,
                        const nsString& inJarLocation,
                        nsInstallFolder *folderSpec,
                        const nsString& inPartialPath,
                        PRInt32 mode,
                        PRBool  bRegister,
                        PRInt32 *error);

        virtual ~nsInstallFile();

        PRInt32 Prepare();
        PRInt32 Complete();
        void  Abort();
        char* toString();

        PRBool CanUninstall();
        PRBool RegisterPackageNode();


    private:

        /* Private Fields */
        nsString*         mVersionInfo;	  /* Version info for this file*/
        
        nsString*         mJarLocation;	      /* Location in the JAR */
        nsCOMPtr<nsIFile> mExtractedFile;	  /* temporary file location */
        nsCOMPtr<nsIFile> mFinalFile;	      /* final file destination */

        nsString*   mVersionRegistryName; /* full version path */

        PRBool      mReplaceFile;    /* whether file exists */
        PRBool      mRegister;       /* if true register this file */
        PRUint32    mFolderCreateCount; /* int to keep count of the number of folders created for a given path */
        
        PRInt32    mMode;            /* an integer used like a bitfield to control *
                                      * how a file is installed or registered      */

        PRInt32     CompleteFileMove();
        void        CreateAllFolders(nsInstall *inInstall, nsIFile *inFolderPath, PRInt32 *error);
    

};

#endif /* nsInstallFile_h__ */
