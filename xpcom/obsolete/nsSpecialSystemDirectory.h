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
 *   Doug Turner <dougt@netscape.com>
 *   IBM Corp.
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

#ifndef _NSSPECIALSYSTEMDIRECTORY_H_
#define _NSSPECIALSYSTEMDIRECTORY_H_

#include "xpcomobsolete.h"
#include "nsFileSpec.h"

#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Types.h>
#endif


extern NS_COM_OBSOLETE void StartupSpecialSystemDirectory();
extern NS_COM_OBSOLETE void ShutdownSpecialSystemDirectory();


// SEE ALSO:
//      mozilla/xpfe/appshell/public/nsFileLocations.h

class NS_COM_OBSOLETE nsSpecialSystemDirectory : public nsFileSpec
{

    public:
        enum SystemDirectories
        {
            OS_DriveDirectory         =   1
        ,   OS_TemporaryDirectory     =   2
        ,   OS_CurrentProcessDirectory=   3
        ,   OS_CurrentWorkingDirectory=   4

        ,   XPCOM_CurrentProcessComponentDirectory=   5
        ,   XPCOM_CurrentProcessComponentRegistry=   6            
          
        ,   Moz_BinDirectory          = 10

        ,   Mac_SystemDirectory       =   101
        ,   Mac_DesktopDirectory      =   102
        ,   Mac_TrashDirectory        =   103
        ,   Mac_StartupDirectory      =   104
        ,   Mac_ShutdownDirectory     =   105
        ,   Mac_AppleMenuDirectory    =   106
        ,   Mac_ControlPanelDirectory =   107
        ,   Mac_ExtensionDirectory    =   108
        ,   Mac_FontsDirectory        =   109
        ,   Mac_ClassicPreferencesDirectory  =   110
        ,   Mac_DocumentsDirectory    =   111
        ,   Mac_InternetSearchDirectory    =   112
        ,   Mac_DefaultDownloadDirectory   =   113
        ,   Mac_UserLibDirectory      =   114
        ,   Mac_PreferencesDirectory  =   115
        
        ,   Win_SystemDirectory       =   201
        ,   Win_WindowsDirectory      =   202

        ,   Win_HomeDirectory         =   203
        ,   Win_Desktop               =   204    
        ,   Win_Programs              =   205    
        ,   Win_Controls              =   206    
        ,   Win_Printers              =   207    
        ,   Win_Personal              =   208    
        ,   Win_Favorites             =   209    
        ,   Win_Startup               =   210    
        ,   Win_Recent                =   211    
        ,   Win_Sendto                =   212    
        ,   Win_Bitbucket             =   213    
        ,   Win_Startmenu             =   214    
        ,   Win_Desktopdirectory      =   215    
        ,   Win_Drives                =   216    
        ,   Win_Network               =   217    
        ,   Win_Nethood               =   218    
        ,   Win_Fonts                 =   219    
        ,   Win_Templates             =   220    
        ,   Win_Common_Startmenu      =   221    
        ,   Win_Common_Programs       =   222    
        ,   Win_Common_Startup        =   223   
        ,   Win_Common_Desktopdirectory = 224   
        ,   Win_Appdata               =   225    
        ,   Win_Printhood             =   226    
        ,   Win_Cookies               =   227
         
        ,   Unix_LocalDirectory       =   301
        ,   Unix_LibDirectory         =   302
        ,   Unix_HomeDirectory        =   303

        ,   BeOS_SettingsDirectory    =   401
        ,   BeOS_HomeDirectory        =   402
        ,   BeOS_DesktopDirectory     =   403
        ,   BeOS_SystemDirectory      =   404

        ,   OS2_SystemDirectory       =   501
        ,   OS2_OS2Directory          =   502
        ,   OS2_DesktopDirectory      =   503
        ,   OS2_HomeDirectory         =   504
        };

                    //nsSpecialSystemDirectory();
                    nsSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory);
        
    virtual         ~nsSpecialSystemDirectory();

    void            operator = (SystemDirectories aSystemSystemDirectory);
 
#if defined(XP_MAC) || defined(XP_MACOSX)
    void            operator = (OSType folderType);
                    nsSpecialSystemDirectory(OSType folderType);
    enum {
      kDefaultDownloadFolderType = FOUR_CHAR_CODE('DfDÄ')    /* Default Download Folder */
    };              
#endif

    /**

     * @param: dirToSet, the value to set for this safeLocation

     * @param: dirSpec, the directory specified as a filespec

     */

    static void Set(SystemDirectories dirToSet, nsFileSpec *dirSpec);


private:
    void            operator = (const char* inPath) { *(nsFileSpec*)this = inPath; }

}; // class NS_COM_OBSOLETE nsSpecialSystemDirectory


#endif
