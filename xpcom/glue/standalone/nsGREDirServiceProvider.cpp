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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
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

#include "nsGREDirServiceProvider.h"
#include "nsEmbedString.h"
#include "nsXPCOMPrivate.h"

#include "nspr.h"
#include "prenv.h"

#ifdef XP_WIN32
#include <windows.h>
#include <stdlib.h>
#endif 

#include <sys/stat.h>

//*****************************************************************************
// greEmbedFileLocProvider::Constructor/Destructor
//*****************************************************************************   

nsGREDirServiceProvider::nsGREDirServiceProvider()
: mPathEnvString(nsnull)
{
  AddGRELocationToPath();
}

nsGREDirServiceProvider::~nsGREDirServiceProvider()
{
  if (mPathEnvString)
    PR_smprintf_free(mPathEnvString);
}

//*****************************************************************************
// nsGREDirServiceProvider::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(nsGREDirServiceProvider, nsIDirectoryServiceProvider)
  
//*****************************************************************************
// nsGREDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************   

NS_IMETHODIMP
nsGREDirServiceProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{    
  nsCOMPtr<nsILocalFile> localFile;
  nsresult rv = NS_ERROR_FAILURE;

  *_retval = nsnull;
  *persistant = PR_TRUE;

  //---------------------------------------------------------------
  // Note that by returning a valid localFile's for NS_GRE_DIR and
  // NS_GRE_COMPONENT_DIR, your app is indicating to XPCOM that 
  // it found a GRE version with which it's compatible with and 
  // intends to be "run against" that GRE.
  //
  // Please see http://www.mozilla.org/projects/embedding/MRE.html
  // for more info on GRE.
  //---------------------------------------------------------------
  if(strcmp(prop, NS_GRE_DIR) == 0)
  {
    rv = GetGreDirectory(getter_AddRefs(localFile));
  }    
  else if(strcmp(prop, NS_GRE_COMPONENT_DIR) == 0)
  {
    rv = GetGreDirectory(getter_AddRefs(localFile));
    if(NS_SUCCEEDED(rv)) {
      nsEmbedCString leaf;
      leaf.Assign("components");
      rv = localFile->AppendRelativeNativePath(leaf);
    }
  }    

  if(!localFile || NS_FAILED(rv))
    return rv;

  return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
}

// Get the location of the GRE version we're compatible with from 
// the registry
//
char * 
nsGREDirServiceProvider::GetGREDirectoryPath()
{
  char *pGreLocation = nsnull;
  
#ifndef XP_UNIX
  // If there exists a file named ".gre.conf" in the current working directory,
  // then we will not use any GRE.  The assumption here is that the GRE is in the
  // same directory as the executable.

  // we only do this on non-unix platforms per blizzard.  
  struct stat configStat;
  if (stat(".gre.config", &configStat) != -1)
    return nsnull;
#endif

  // if USE_LOCAL_GRE is set, we just fall back on using no gre or rather,
  // the Gecko bits that sit next to the application or in the LD_LIBRARY_PATH
  if (PR_GetEnv("USE_LOCAL_GRE"))
    return nsnull;

  // check in the HOME directory
  char * path = PR_GetEnv("HOME");
  if (path) {
    char* greConfHomePath= (char *)malloc(strlen(path) + sizeof(GRE_CONF_NAME) + sizeof(XPCOM_FILE_PATH_SEPARATOR));
    
	sprintf(greConfHomePath, "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_NAME, path);
    
	pGreLocation = GetPathFromConfigFile(greConfHomePath);
        free(greConfHomePath);
        if (pGreLocation)
          return pGreLocation;
  }
  
  path = PR_GetEnv("MOZ_GRE_CONF");
  if (path) {
    pGreLocation = GetPathFromConfigFile(path);
    if (pGreLocation)
      return pGreLocation;
  }
  
#ifdef XP_UNIX
  pGreLocation = GetPathFromConfigFile(GRE_CONF_PATH);
  if (pGreLocation)
    return pGreLocation;
#endif
  
#if XP_WIN32
  char szKey[256];
  HKEY hRegKey = NULL;
  DWORD dwLength = _MAX_PATH * sizeof(char);
  long rc;
  char keyValue[_MAX_PATH + 1];
  
  // A couple of key points here:
  // 1. Note the usage of the "Software\\Mozilla\\GRE" subkey - this allows
  //    us to have multiple versions of GREs on the same machine by having
  //    subkeys such as 1.0, 1.1, 2.0 etc. under it.
  // 2. In this sample below we're looking for the location of GRE version 1.2
  //    i.e. we're compatible with GRE 1.2 and we're trying to find it's install
  //    location.
  //
  // Please see http://www.mozilla.org/projects/embedding/MRE.html for
  // more info.
  //
  strcpy(szKey, GRE_WIN_REG_LOC MOZILLA_VERSION);

  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_QUERY_VALUE, &hRegKey) == ERROR_SUCCESS) {
    if ((rc = ::RegQueryValueEx(hRegKey, "GreHome", NULL, NULL, (BYTE *)keyValue, &dwLength))==ERROR_SUCCESS) {
      pGreLocation = strdup(keyValue);
    }
    ::RegCloseKey(hRegKey);
  }
  if (pGreLocation)
    return pGreLocation;
#endif
  return pGreLocation;
}

char*
nsGREDirServiceProvider::GetPathFromConfigFile(const char* filename)
{
  char* pGreLocation = nsnull;
  char buffer[1024];
  FILE *cfg;
  PRBool foundHeader = PR_FALSE;
  PRInt32 versionLen = sizeof(MOZILLA_VERSION)-1;
  
  if((cfg=fopen(filename,"r"))==nsnull) {
    return nsnull;
  }
  
  while (fgets(buffer, 1024, cfg) != nsnull) {
    // skip over comment lines and blank lines
    if (buffer[0] == '#' || buffer[0] == '\n') {
      continue;
    }
    
    // we found a section heading, check to see if it is the one we are intersted in.
    if (buffer[0] == '[') {
      if (!strncmp (buffer+1, MOZILLA_VERSION, versionLen)) {
        foundHeader = PR_TRUE;
      }
      continue;
    }
    
    if (foundHeader && !strncmp (buffer, "GRE_PATH=", 9)) {
      pGreLocation = strdup(buffer + 9 );
      // kill the line feed if any
      PRInt32 len = strlen(pGreLocation);
      len--;
      
      if (pGreLocation[len] == '\n')
        pGreLocation[len] = '\0';
      break;
    }
  }
  fclose(cfg);
  return pGreLocation;
}



nsresult
nsGREDirServiceProvider::GetGreDirectory(nsILocalFile **aLocalFile)
{
  NS_ENSURE_ARG_POINTER(aLocalFile);
  nsresult rv = NS_ERROR_FAILURE;
  
  // Get the path of the GRE which is compatible with our embedding application
  // from the registry
  //
  char *pGreDir = GetGREDirectoryPath();
  if(pGreDir) {
    nsCOMPtr<nsILocalFile> tempLocal;
    nsEmbedCString leaf;
    leaf.Assign(pGreDir);
    rv = NS_NewNativeLocalFile(leaf, PR_TRUE, getter_AddRefs(tempLocal));
    
    if (NS_SUCCEEDED(rv)) {
      *aLocalFile = tempLocal;
      NS_ADDREF(*aLocalFile);
    }
    
    free(pGreDir);
  }
  return rv;
}


char* 
nsGREDirServiceProvider::GetXPCOMPath()
{
  char* grePath = GetGREDirectoryPath();
  if (!grePath) {
    char* greEnv = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (!greEnv) {
      return nsnull;
    }
    grePath = strdup(greEnv);
  }

  int len = strlen(grePath);
  char* xpcomPath = (char*) malloc(len + sizeof(XPCOM_DLL) + sizeof(XPCOM_FILE_PATH_SEPARATOR) + 1);

  sprintf(xpcomPath, "%s" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL, grePath);

  free(grePath);
  return xpcomPath;
}


void
nsGREDirServiceProvider::AddGRELocationToPath()
{
  char* grePath = GetGREDirectoryPath();
  if (!grePath)
    return;

  char* path = PR_GetEnv(XPCOM_SEARCH_KEY);
  if (!path) {
    path = "";
  }

  if (mPathEnvString)
    PR_Free(mPathEnvString);

  mPathEnvString = PR_smprintf("%s=%s;%s",
                               XPCOM_SEARCH_KEY,
                               path,
                               grePath);
  PR_SetEnv(mPathEnvString);
  free(grePath);
}

