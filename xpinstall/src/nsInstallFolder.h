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

#define PLUGIN_DIR                         100
#define PROGRAM_DIR                        101

#define TEMP_DIR                           104
#define OS_HOME_DIR                        105
#define PROFILE_DIR                        106
#define PREFERENCES_DIR                    107
#define OS_DRIVE                           108
#define FILE_TARGET                        109

#define COMPONENTS_DIR                     110
#define CHROME_DIR                         111

#define WIN_SYS_DIR                        200
#define WINDOWS_DIR                        201
#define WIN_DESKTOP_DIR                    202
#define WIN_DESKTOP_COMMON                 203
#define WIN_STARTMENU                      204
#define WIN_STARTMENU_COMMON               205
#define WIN_PROGRAMS_DIR                   206
#define WIN_PROGRAMS_COMMON                207
#define WIN_STARTUP_DIR                    208
#define WIN_STARTUP_COMMON                 209
#define WIN_APPDATA_DIR                    210
#define WIN_PROGRAM_FILES                  211
#define WIN_COMMON_FILES                   212

#define MAC_SYSTEM                         300
#define MAC_DESKTOP                        301
#define MAC_TRASH                          302
#define MAC_STARTUP                        303
#define MAC_SHUTDOWN                       304
#define MAC_APPLE_MENU                     305
#define MAC_CONTROL_PANEL                  306
#define MAC_EXTENSION                      307
#define MAC_FONTS                          308
#define MAC_PREFERENCES                    309
#define MAC_DOCUMENTS                      310

#define MACOSX_HOME                        500
#define MACOSX_DEFAULT_DOWNLOAD            501
#define MACOSX_USER_DESKTOP                502
#define MACOSX_LOCAL_DESKTOP               503
#define MACOSX_USER_APPLICATIONS           504
#define MACOSX_LOCAL_APPLICATIONS          505
#define MACOSX_USER_DOCUMENTS              506
#define MACOSX_LOCAL_DOCUMENTS             507
#define MACOSX_USER_INTERNET_PLUGIN        508
#define MACOSX_LOCAL_INTERNET_PLUGIN       509
#define MACOSX_USER_FRAMEWORKS             510
#define MACOSX_LOCAL_FRAMEWORKS            511
#define MACOSX_USER_PREFERENCES            512
#define MACOSX_LOCAL_PREFERENCES           513
#define MACOSX_PICTURE_DOCUMENTS           514
#define MACOSX_MOVIE_DOCUMENTS             515
#define MACOSX_MUSIC_DOCUMENTS             516
#define MACOSX_INTERNET_SITES              517

#define UNIX_LOCAL                         400
#define UNIX_LIB                           401

#ifdef XP_MAC
#define INSTALL_PLUGINS_DIR     NS_LITERAL_CSTRING("Plug-ins")
#define INSTALL_COMPONENTS_DIR  NS_LITERAL_CSTRING("Components")
#define INSTALL_CHROME_DIR      NS_LITERAL_CSTRING("Chrome")
#else
#define INSTALL_PLUGINS_DIR     NS_LITERAL_CSTRING("plugins")
#define INSTALL_COMPONENTS_DIR  NS_LITERAL_CSTRING("components")
#define INSTALL_CHROME_DIR      NS_LITERAL_CSTRING("chrome")
#endif

class nsInstallFolder
{
    public:
        
       nsInstallFolder();
       virtual ~nsInstallFolder();

       nsresult Init(nsInstallFolder& inFolder, const nsString& subString);
       nsresult Init(const nsAString& aFolderID, const nsString& aRelativePath);
       nsresult Init(nsIFile* rawIFile, const nsString& aRelativePath);

       void GetDirectoryPath(nsCString& aDirectoryPath);
       nsIFile* GetFileSpec();
       PRInt32 ToString(nsAutoString* outString);
       
    private:
        
        nsCOMPtr<nsIFile>  mFileSpec;

        void         SetDirectoryPath(const nsAString& aFolderID, const nsString& aRelativePath);
        void         AppendXPPath(const nsString& aRelativePath);
        PRInt32      MapNameToEnum(const nsAString&  name);
};


#endif
