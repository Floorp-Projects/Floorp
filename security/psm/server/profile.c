/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "ctrlconn.h"
#include "serv.h"
#ifdef WIN32
#include <windows.h>
#include <winreg.h>
#endif

#ifdef WIN32
/* local prototypes */
char* SSM_PREF_WinRegQueryCharValueEx(HKEY key, const char* subKey);

char* SSM_PROF_WinGetNetworkProfileDir(void);
char* SSM_PROF_WinGetProfileDirFromName(const char* profName);
char* SSM_PROF_WinGetDefaultProfileDB(void);
#endif


char* SSM_PROF_GetProfileDirectory(SSMControlConnection* conn)
{
    char* path = NULL;
    PRFileInfo info;
#ifdef WIN32
    char* profDB = NULL;
#endif

    if (conn == NULL) {
	goto loser;
    }

#if defined(XP_UNIX)
    path = PR_smprintf("%s/.netscape", PR_GetEnv("HOME"));
    if (path == NULL) {
	goto loser;
    }
#elif defined(WIN32)
    /* Folks tell me that the correct way to get it is through the Netscape
     * registry, not the Windows registry, but I can't confirm that story.
     * I will keep investigating.
     * Furthermore, if we complete migration and once Cartman owns its own
     * registry space, we will not have to worry about locating the profile
     * directory this way.
     */
#if 0    /* XXX for now we only get the default directory */
    /* check network installations */
    path = SSM_PROF_WinGetNetworkProfileDir();
    if (path != NULL) {
	goto check;
    }

    /* next, try to get it directly from the user registry */
    path = SSM_PROF_WinGetProfileDirFromName(conn->m_profileName);
    if (path != NULL) {
	goto check;
    }
#endif

    /* couldn't find the profile; look for the default */
    profDB = SSM_PROF_WinGetDefaultProfileDB();
    if (profDB == NULL) {
	goto loser;
    }

    /* get the profile directory */
    path = PR_smprintf("%s\\%s", profDB, conn->m_profileName);
    if ((PR_GetFileInfo(path, &info) != PR_SUCCESS) ||
	(info.type != PR_FILE_DIRECTORY)) {
	/* couldn't find it there either; try to guess it */
	SSM_DEBUG("Cannot find a profile in %s.  Trying again...\n", path);
	PR_Free(path);
	path = PR_smprintf("C:\\Program Files\\Netscape\\Users\\%s",
			   conn->m_profileName);
	if (path == NULL) {
	    goto loser;
	}
    }
    else {
	goto done;
    }
#endif

    /* check whether the directory exists */
    if ((PR_GetFileInfo(path, &info) != PR_SUCCESS) || 
	(info.type != PR_FILE_DIRECTORY)) {
	SSM_DEBUG("Can't find a profile in %s.\n", path);
	goto loser;
    }

    /* success */
    SSM_DEBUG("The directory is %s.\n", path);
    goto done;
loser:
    if (path != NULL) {
	PR_Free(path);
	path = NULL;
    }
done:
#ifdef WIN32
    if (profDB != NULL) {
	PR_Free(profDB);
    }
#endif
    return path;
}


#ifdef WIN32
/* Function: char* SSM_PROF_WinGetNetworkProfileDir(void)
 * Purpose: returns the profile directory from Windows registry in case of
 *          network installation
 * Arguments and return values:
 * - returns: profile directory; NULL otherwise
 *
 * Note: this is still Nova-specific.  Seamonkey does not support network
 *       installations?
 * Key: "HKEY_CURRENT_USER\SOFTWARE\Netscape\Netscape Navigator\UserInfo\
 *       DirRoot"
 */
char* SSM_PROF_WinGetNetworkProfileDir(void)
{
    char* subKey = "SOFTWARE\\Netscape\\Netscape Navigator\\UserInfo";
    LONG rv;
    char* ret = NULL;
    HKEY keyRet = NULL;

    rv = RegOpenKeyEx(HKEY_CURRENT_USER, subKey, (DWORD)0, KEY_QUERY_VALUE,
		      &keyRet);
    if (rv != ERROR_SUCCESS) {
	return ret;
    }

    ret = SSM_PREF_WinRegQueryCharValueEx(keyRet, "DirRoot");
    if (ret == NULL) {
	goto loser;
    }

    /* success */
    goto done;
loser:
    if (ret != NULL) {
	PR_Free(ret);
	ret = NULL;
    }
done:
    if (keyRet != NULL) {
	RegCloseKey(keyRet);
    }
    return ret;
}


/* Function: char* SSM_PROF_WinGetProfileDirFromName()
 * Purpose: returns the profile directory that belongs to the profile from
 *          the registry
 * Arguments and return values:
 * - profName: profile name
 * - returns: profile directory; NULL if failure
 *
 * Note: this is still Nova-specific.  A lot has to be ifdef'd when Seamonkey
 *       comes online.
 * key: "HKEY_LOCAL_MACHINE\SOFTWARE\Netscape\Netscape Navigator\Users\(name)\
 *       DirRoot"
 */
char* SSM_PROF_WinGetProfileDirFromName(const char* profName)
{
    char* subKey = NULL;
    LONG rv;
    char* ret = NULL;
    HKEY keyRet = NULL;

    PR_ASSERT(profName != NULL);

    subKey = PR_smprintf("SOFTWARE\\Netscape\\Netscape Navigator\\Users\\%s",
			 profName);
    if (subKey == NULL) {
	return ret;
    }

    /* open the user's registry key */
    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey, (DWORD)0, KEY_QUERY_VALUE,
		      &keyRet);
    if (rv != ERROR_SUCCESS) {
	goto loser;
    }

    /* get the string */
    ret = SSM_PREF_WinRegQueryCharValueEx(keyRet, "DirRoot");
    if (ret == NULL) {
	goto loser;
    }
    
    /* success */
    goto done;
loser:
    if (ret != NULL) {
	PR_Free(ret);
	ret = NULL;
    }
done:
    if (keyRet != NULL) {
	RegCloseKey(keyRet);
    }
    return ret;
}


void ssm_prof_cut_idstring(char** idString)
{
    char* marker = NULL;
    char* subString = NULL;
    PRUint32 fullLength;
    int subSize;

    PR_ASSERT(*idString != NULL);

    fullLength = strlen(*idString);
    /* traverse backward to find the last backslash */
    marker = strrchr(*idString, '\\');
    if (marker == NULL) {
	/* no backslash?  strange but return */
	return;
    }

    /* found the backslash */
    subSize = marker - *idString;
    subString = (char*)PR_Calloc(fullLength-subSize+1, sizeof(char));
    if (subString == NULL) {
	goto loser;
    }

    memcpy(subString, marker, fullLength-subSize);
    if (_stricmp(subString, "Communicator") == 0 ||
	_stricmp(subString, "Commun~1") == 0) {
	/* "Communicator" is at the end of the installation directory path.
	 * Take it out.
	 */
	char* newID = NULL;

	newID = (char*)PR_Calloc(subSize+1, sizeof(char));
	if (newID != NULL) {
	    memcpy(newID, *idString, subSize);
	    PR_Free(*idString);
	    *idString = PL_strdup(newID);
	    PR_Free(newID);
	}
    }
loser:
    return;
}

/* Function: char* SSM_PROF_WinGetDefaultProfileDB()
 * Purpose: tries to deduce the default profile db of the current version of 
 *          Communicator
 * Arguments and return values:
 * - returns: newly allocated profile db string; NULL if failure
 *
 * Note: This is Nova-specific.  Now, this function is being used to supply
 *       the "default" profile directory for 3rd party applications.
 * Key: "HKEY_LOCAL_MACHINE\SOFTWARE\Netscape\Netscape Navigator\(current
 *       version)\Main\Install Directory"
 */
char* SSM_PROF_WinGetDefaultProfileDB(void)
{
    char* subKey = "SOFTWARE\\Netscape\\Netscape Navigator";
    LONG rv;
    char* ret = NULL;
    HKEY keyRet = NULL;
    char* cvString = NULL;
    char* idKey = NULL;
    char* idString = NULL;

    /* open the program registry */
    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey, (DWORD)0, KEY_QUERY_VALUE,
		      &keyRet);
    if (rv != ERROR_SUCCESS) {
	return ret;
    }

    /* get the current version info first */
    cvString = SSM_PREF_WinRegQueryCharValueEx(keyRet, "currentVersion");
    if (cvString == NULL) {
	goto loser;
    }

    /* now, get the actual default install directory for the current version */
    idKey = PR_smprintf("%s\\%s\\Main", subKey, cvString);
    if (idKey == NULL) {
	goto loser;
    }

    /* open a new key for the install directory */
    RegCloseKey(keyRet);
    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, idKey, (DWORD)0, KEY_QUERY_VALUE,
		      &keyRet);
    if (rv != ERROR_SUCCESS) {
	return ret;
    }

    idString = SSM_PREF_WinRegQueryCharValueEx(keyRet, "Install Directory");
    if (idString == NULL) {
	goto loser;
    }

    /* wait, we would like to remove "Communicator" at the end of the
     * installation path to get the profiles directory correctly
     */
    ssm_prof_cut_idstring(&idString);
    if (idString == NULL) {
	goto loser;
    }

    /* append the user name */
    ret = PR_smprintf("%s\\Users", idString);
    if (ret == NULL) {
	goto loser;
    }

    goto done;
loser:
    if (ret != NULL) {
	PR_Free(ret);
	ret = NULL;
    }
done:
    if (cvString != NULL) {
	PR_Free(cvString);
    }
    if (idKey != NULL) {
	PR_Free(idKey);
    }
    if (idString != NULL) {
	PR_Free(idString);
    }
    if (keyRet != NULL) {
	RegCloseKey(keyRet);
    }
    return ret;
}


/* allocates memory for and retrieves string data from the Windows registry
 * (wrapper for RegQueryValueEx())
 * this should be incorporated in the prefs headers later
 * Warning: key should have been already opened with RegOpenKeyEx()!
 */
char* SSM_PREF_WinRegQueryCharValueEx(HKEY key, const char* subKey)
{
    LONG rv;
    DWORD type;
    DWORD size = 0;
    char* data = NULL;

    PR_ASSERT(key != NULL && subKey != NULL);

    /* size the string */
    rv = RegQueryValueEx(key, (LPTSTR)subKey, NULL, &type, NULL, &size);
    if (rv != ERROR_SUCCESS || size == 0) {
	goto loser;
    }

    data = (char*)PR_Malloc(size);
    if (data == NULL) {
	goto loser;
    }

    rv = RegQueryValueEx(key, (LPTSTR)subKey, NULL, &type, (LPBYTE)data, 
			 &size);
    if (rv != ERROR_SUCCESS || size == 0) {
	goto loser;
    }

    goto done;
loser:
    if (data != NULL) {
	PR_Free(data);
	data = NULL;
    }
done:
    return data;
}
#endif
