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
 *     John R. McMullen <mcmullen@netscape.com>
 */

#ifndef _NSFILELOCATIONS_H_
#define _NSFILELOCATIONS_H_

#include "nsFileSpec.h"

#ifdef XP_MAC
#include "Types.h"
#endif

// SEE ALSO:
//      mozilla/base/public/nsSpecialSystemDirectory.h

//========================================================================================
class NS_APPSHELL nsSpecialFileSpec : public nsFileSpec
//========================================================================================
{

    public:
		enum Type
		{
		    App_DirectoryBase              =     0
		,   App_PrefsDirectory30           =     1
		,   App_PrefsDirectory40           =     2
		,   App_PrefsDirectory50           =     3

		,   App_UserProfileDirectory30     =    10
		,   App_UserProfileDirectory40     =    11
		,   App_UserProfileDirectory50     =    12

		,   App_FileBase                   =  1000
		,   App_PreferencesFile30          =  1001
		,   App_PreferencesFile40          =  1002
		,   App_PreferencesFile50          =  1003

		,   App_BookmarksFile30            =  1010
		,   App_BookmarksFile40            =  1011
		,   App_BookmarksFile50            =  1012

		,	App_Registry40                 =  1020
		,   App_Registry50                 =  1021
		};
                    //nsSpecialFileSpec();
                    nsSpecialFileSpec(Type aType);        
    virtual         ~nsSpecialFileSpec();
    void            operator = (Type aType);
    void            operator = (const nsFileSpec& inOther) { *(nsFileSpec*)this = inOther; }
    PRBool          operator == (const nsFileSpec& inOther)
                        { return nsFileSpec::operator == (inOther); }
 
private:
    void            operator = (const char* inPath) { *(nsFileSpec*)this = inPath; }

}; // class NS_APPSHELL nsSpecialFileSpec

#endif // _NSFILELOCATIONS_H_
