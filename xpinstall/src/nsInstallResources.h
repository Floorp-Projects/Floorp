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


#ifndef __NS_INSTALLRESOURCES_H__
#define __NS_INSTALLRESOURCES_H__

class nsInstallResources
{
    public:
       
       static char* GetInstallFileString(void);
       static char* GetReplaceFileString(void);     
       static char* GetDeleteFileString(void);      
       static char* GetDeleteComponentString(void); 
       static char* GetExecuteString(void);         
       static char* GetExecuteWithArgsString(void); 
       static char* GetPatchFileString(void);       
       static char* GetUninstallString(void);       
};


#endif