/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */


#ifndef __NS_INSTALLFOLDER_H__
#define __NS_INSTALLFOLDER_H__

#include "nscore.h"
#include "prtypes.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsSpecialSystemDirectory.h"

#ifdef XP_MAC
#define INSTALL_PLUGINS_DIR     "Plug-ins"
#define INSTALL_COMPONENTS_DIR  "Components"
#define INSTALL_CHROME_DIR      "Chrome"
#else
#define INSTALL_PLUGINS_DIR     "plugins"
#define INSTALL_COMPONENTS_DIR  "components"
#define INSTALL_CHROME_DIR      "chrome"
#endif

class nsInstallFolder
{
    public:
        
       nsInstallFolder();
       virtual ~nsInstallFolder();

       nsresult Init(nsInstallFolder& inFolder, const nsString& subString);
       nsresult Init(const nsString& aFolderID, const nsString& aRelativePath);
       nsresult Init(nsIFile* rawIFile);

       void GetDirectoryPath(nsCString& aDirectoryPath);
       nsIFile* GetFileSpec();
       PRInt32 ToString(nsAutoString* outString);
       
    private:
        
        nsCOMPtr<nsIFile>  mFileSpec;

        void         SetDirectoryPath(const nsString& aFolderID, const nsString& aRelativePath);
        void         AppendXPPath(const nsString& aRelativePath);
        PRInt32      MapNameToEnum(const nsString&  name);
};


#endif
