/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *     Doug Turner <dougt@netscape.com>
 */

#include "nsFileLocations.h"

#include "nsSpecialSystemDirectory.h"
#include "nsDebug.h"

#ifdef XP_MAC
#include <Folders.h>
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#elif defined(XP_PC)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <sys/param.h>
#endif

#include "plstr.h"

#if XP_PC
//----------------------------------------------------------------------------------------
static char* MakeUpperCase(char* aPath)
//----------------------------------------------------------------------------------------
{
  // windows does not care about case.  push to uppercase:
  int length = strlen(aPath);
  for (int i = 0; i < length; i++)
      if (islower(aPath[i]))
        aPath[i] = _toupper(aPath[i]);
    
  return aPath;
}
#endif

//----------------------------------------------------------------------------------------
nsSpecialFileSpec::nsSpecialFileSpec(LocationType aType)
//----------------------------------------------------------------------------------------
:    nsFileSpec((const char*)nsnull)
{
    *this = aType;
}

//----------------------------------------------------------------------------------------
nsSpecialFileSpec::~nsSpecialFileSpec()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void nsSpecialFileSpec::operator = (LocationType aType)
//----------------------------------------------------------------------------------------
{
    *this = (const char*)nsnull;
    switch (aType)
    {
        
        case App_PrefsDirectory30:
	        #ifdef XP_MAC
	        {
	            *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::Mac_PreferencesDirectory);
	            *this += "Netscape \xC4";
	            break;
	        }
	        #endif
        case App_PrefsDirectory40:
        case App_PrefsDirectory50:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        
        case App_UserProfileDirectory30:
        case App_UserProfileDirectory40:
        case App_UserProfileDirectory50:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        
        case App_PreferencesFile30:
	        {
	            *this = nsSpecialFileSpec(App_PrefsDirectory30);
	        #ifdef XP_MAC
	            *this += "Netscape Preferences";
	        #else
	            *this += "prefs.js";
	        #endif
	            break;
	        }
        case App_PreferencesFile40:
        case App_PreferencesFile50:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        
        case App_BookmarksFile30:
	        #ifdef XP_MAC
	        {
	            // This is possibly correct on all platforms
	            *this = nsSpecialFileSpec(App_PrefsDirectory30);
	            *this += "Bookmarks.html";
	            break;
	        }
	        #endif
        case App_BookmarksFile40:
	        #ifdef XP_MAC
	        {
	            // This is possibly correct on all platforms
	            *this = nsSpecialFileSpec(App_PrefsDirectory40);
	            *this += "Bookmarks.html";
	            break;
	        }
	        #endif
        case App_BookmarksFile50:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        
        case App_Registry40:
	        #ifdef XP_MAC
	        {
	            *this = nsSpecialFileSpec(App_PrefsDirectory30);
	            *this += "Netscape Registry";
	            break;
	        }
	        #endif
        case App_Registry50:
	        #ifdef XP_MAC
	        {
	            *this = nsSpecialFileSpec(App_PrefsDirectory30);
	            *this += "Mozilla Registry";
	            break;
	        }
	        #endif
        
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    

        case App_DirectoryBase:
        case App_FileBase:
        default:
            NS_ERROR("Invalid location type");
            break;    
    }
} // nsSpecialFileSpec::operator =
