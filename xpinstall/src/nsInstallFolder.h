/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
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

class nsInstallFolder
{
    public:
        
       nsInstallFolder(const nsString& aFolderID);
       nsInstallFolder(const nsString& aFolderID, const nsString& aRelativePath);
       ~nsInstallFolder();

       void GetDirectoryPath(nsString& aDirectoryPath);
       
    private:
        
        nsFileSpec*  mUrlPath;

		void         SetDirectoryPath(const nsString& aFolderID, const nsString& aRelativePath);
        void         PickDefaultDirectory();
        PRInt32      MapNameToEnum(const nsString&  name);

        void         operator = (enum nsSpecialSystemDirectory::SystemDirectories aSystemSystemDirectory);
};


#endif