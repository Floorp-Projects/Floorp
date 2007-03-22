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

#include <string.h>
#include "nscore.h"
#include "nsInstallResources.h"

static nsXPIResourceTableItem XPIResTable[] = 
{
    /*---------------------------------------------------------------------*
     *   Install Actions 
     *---------------------------------------------------------------------*/
    { "InstallFile",        "Installing: %s" },
    { "InstallSharedFile",  "Installing Shared File: %s" },
    { "ReplaceFile",        "Replacing: %s" },
    { "ReplaceSharedFile",  "Replacing Shared File: %s" },
    { "SkipFile",           "Skipping: %s" },
    { "SkipSharedFile",     "Skipping Shared File: %s" },
    { "DeleteFile",         "Deleting file: %s" },
    { "DeleteComponent",    "Deleting component: %s" },
    { "Execute",            "Executing: %s" },
    { "ExecuteWithArgs",    "Executing: %s with argument: %s" },
    { "Patch",              "Patching: %s" },
    { "Uninstall",          "Uninstalling: %s" },
    { "RegSkin",            "Register Skin: %s" },
    { "RegLocale",          "Register Locale: %s" },
    { "RegContent",         "Register Content: %s" },
    { "RegPackage",         "Register Package: %s" },


    { "CopyFile",              "Copy File: %s to %s" },
    { "ExecuteFile",           "Execute File: %s" },
    { "ExecuteFileWithArgs",   "Execute File: %s with argument: %s" },
    { "MoveFile",              "Move File: %s to %s" },
    { "RenameFile",            "Rename File: %s to %s" },
    { "CreateFolder",          "Create Folder: %s" },
    { "RemoveFolder",          "Remove Folder: %s" },
    { "RenameFolder",          "Rename Folder: %s to %s" },
    { "WindowsShortcut",       "Windows Shortcut: %s" },
    { "MacAlias",              "Mac Alias: %s" },
    { "WindowsRegisterServer", "Windows Register Server: %s" },
    { "UnknownFileOpCommand",  "Unknown file operation command!" },

    // XXX FileOp*() action strings
    // XXX WinReg and WinProfile action strings

    /*---------------------------------------------------------------------*
     *    Miscellaneous 
     *---------------------------------------------------------------------*/
    { "ERROR", "ERROR" },

    { NS_XPI_EOT, "" }
};

char* 
nsInstallResources::GetDefaultVal(const char* aResName)
{
    char    *currResName = XPIResTable[0].resName;
    char    *currResVal = nsnull;
    PRInt32 idx, len = 0;

    for (idx = 0; 0 != strcmp(currResName, NS_XPI_EOT); idx++)
    {
        currResName = XPIResTable[idx].resName;
        len = strlen(currResName);

        if (0 == strncmp(currResName, aResName, len))
        {
            currResVal = XPIResTable[idx].defaultString;
            break;
        }
    }

    return currResVal;
}

