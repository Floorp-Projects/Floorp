/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
#include "nsPluginDirServiceProvider.h"
 
#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsDependentString.h"

#if defined (XP_WIN)
#include <windows.h>
#endif

//*****************************************************************************
// nsPluginDirServiceProvider::Constructor/Destructor
//*****************************************************************************

nsPluginDirServiceProvider::nsPluginDirServiceProvider()
{
    NS_INIT_ISUPPORTS();
}

nsPluginDirServiceProvider::~nsPluginDirServiceProvider()
{
}

//*****************************************************************************
// nsPluginDirServiceProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPluginDirServiceProvider, nsIDirectoryServiceProvider)

//*****************************************************************************
// nsPluginDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsPluginDirServiceProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
    nsCOMPtr<nsILocalFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    NS_ENSURE_ARG(prop);
    *_retval = nsnull;
    *persistant = PR_TRUE;
    
    if (nsCRT::strcmp(prop, "NS_4DOTX_PLUGINS_DIR") == 0)
    {
#if defined(XP_WIN)
        // look for the plugin folder that the user has in their Communicator 4x install
        HKEY keyloc; 
        long result;
        DWORD type; 
        char szKey[512] = "Software\\Netscape\\Netscape Navigator";
        char path[_MAX_PATH];
        
        result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc); 

        if (result == ERROR_SUCCESS) 
        {
            char current_version[80];
            DWORD length = sizeof(current_version);

            result = ::RegQueryValueEx(keyloc, "CurrentVersion", NULL, &type, (LPBYTE)&current_version, &length); 

            ::RegCloseKey(keyloc);
            PL_strcat(szKey, "\\");
            PL_strcat(szKey, current_version);
            PL_strcat(szKey, "\\Main");
            result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc);
            
            if (result == ERROR_SUCCESS) 
            { 
                DWORD pathlen = sizeof(path); 

                result = ::RegQueryValueEx(keyloc, "Plugins Directory", NULL, &type, (LPBYTE)&path, &pathlen); 
                if (result == ERROR_SUCCESS)
                    rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localFile));
                ::RegCloseKey(keyloc); 
            } 
        }
#endif
    }
    else if (nsCRT::strcmp(prop, "NS_WIN_JAVA_JRE_DIR") == 0)
    {
#if defined(XP_WIN)
    // Look for the Java OJI plugin via the JRE install path
        HKEY baseloc;
        HKEY keyloc;
        FILETIME modTime = {0,0};  // initilize variables
        FILETIME curVer = {0,0};
        DWORD type; 
        DWORD index = 0;
        DWORD numChars = _MAX_PATH;
        DWORD pathlen;
        char curKey[_MAX_PATH] = "Software\\JavaSoft\\Java Plug-in";
        char path[_MAX_PATH];
        char newestPath[_MAX_PATH + 4]; // to prevent buffer overrun when adding /bin
        
        newestPath[0] = 0;
        LONG result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0, KEY_READ, &baseloc);

        // we must enumerate through the keys because what if there is more than one version?
        while (ERROR_SUCCESS == result)
        {
            path[0] = 0;
            numChars = _MAX_PATH;
            pathlen = sizeof(path);
            result = ::RegEnumKeyEx(baseloc, index, curKey, &numChars, NULL, NULL, NULL, &modTime);
            index++;
            if (ERROR_SUCCESS == result)
            {
                if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0, KEY_QUERY_VALUE, &keyloc))
                {
                    // we have a sub key
                    if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, "JavaHome", NULL, &type, (LPBYTE)&path, &pathlen))
                    {
                        // Compare time stamps from registry lookup
                        // Only use the key with the latest time stamp because there could be several
                        //
                        // NOTE: This may not be the highest version revsion number (szKey)
                        // if the user installed an older version AFTER a newer one
                        // This assumes the last version installed is the one the user wants to use
                        // We can also tweak this for checking for a minimum version on szKey
                        // Don't tweek. Stay between compatible versions in 1.3.x. JRE 1.4 is an XPCOM
                        // component and needs to be installed explicitly!
                        if (::CompareFileTime(&modTime,&curVer) >= 0 && 
                            (atof(curKey) >= 1.3) && (atof(curKey) < 1.4))
                        {
                            PL_strcpy(newestPath,path);
                            curVer = modTime;
                        }
                        ::RegCloseKey(keyloc);
                    }          
                }        
            }
        }

        ::RegCloseKey(baseloc);
        
        // if nothing is found, then don't add \bin dir
        if (newestPath[0] != 0)
        {
            PL_strcat(newestPath,"\\bin");
            rv = NS_NewNativeLocalFile(nsDependentCString(newestPath), PR_TRUE, getter_AddRefs(localFile));
        }
#endif
    }

    if (localFile && NS_SUCCEEDED(rv))
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
        
    return rv;
}
