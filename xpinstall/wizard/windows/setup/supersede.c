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

#include "setup.h"
#include "extern.h"
#include "extra.h"
#include "ifuncns.h"
#include "supersede.h"

void              SaveGreInfo(grever *aGreInstalledListTmp, greInfo* aGre);
void              ResolveSupersedeGre(siC *siCObject, greInfo *aGre);
HRESULT           GetGreSupersedeVersionList(siC *siCObject, grever **aGreSupersedeList);
HRESULT           GetGreInstalledVersionList(siC *siCObject, grever **aGreInstalledList);

grever *CreateGVerNode()
{
  grever *gverNode;

  if((gverNode = NS_GlobalAlloc(sizeof(struct structGreVer))) == NULL)
    return(NULL);

  gverNode->greHomePath[0]         = '\0';
  gverNode->greInstaller[0]        = '\0';
  gverNode->greUserAgent[0]        = '\0';
  gverNode->version.ullMajor       = 0;
  gverNode->version.ullMinor       = 0;
  gverNode->version.ullRelease     = 0;
  gverNode->version.ullBuild       = 0;
  gverNode->next                   = NULL;
  return(gverNode);
}

void DeleteGverList(grever *gverHead)
{
  grever *tmp;

  while(gverHead != NULL)
  {
    tmp = gverHead;
    gverHead = gverHead->next;
    FreeMemory(&tmp);
  }
}

/* function to build a linked list of GRE supersede versions */
HRESULT GetGreSupersedeVersionList(siC *siCObject, grever **aGreSupersedeList)
{
  grever  *gverTmp = NULL;
  grever  *gverTail = NULL;
  char    key[MAX_BUF_TINY];
  char    versionString[MAX_BUF_TINY];
  DWORD   index;

  if(*aGreSupersedeList)
    /* list already built, just return */
    return(WIZ_OK);

  index = 0;
  wsprintf(key, "SupersedeVersion%d", index);        
  GetPrivateProfileString(siCObject->szReferenceName, key, "", versionString,
                          sizeof(versionString), szFileIniConfig);
  while(*versionString != '\0')
  {
    gverTmp = CreateGVerNode();
    if(!gverTmp)
      // error has already been displayed.
      exit(WIZ_OUT_OF_MEMORY);

    TranslateVersionStr(versionString, &gverTmp->version);
    if(*aGreSupersedeList == NULL)
    {
      /* brand new list */
      *aGreSupersedeList = gverTmp;
      gverTail = *aGreSupersedeList;
    }
    else if(gverTail)
    {
      /* append the new node to the end of the list */
      gverTail->next = gverTmp;
      gverTail = gverTail->next;
    }
    
    wsprintf(key, "SupersedeVersion%d", ++index);        
    GetPrivateProfileString(siCObject->szReferenceName, key, "", versionString,
                            sizeof(versionString), szFileIniConfig);
  }
  return(WIZ_OK);
}

/* function to build a list of GRE's installed on the local system */
HRESULT GetGreInstalledVersionList(siC *siCObject, grever **aGreInstalledList)
{
  DWORD     index;
  DWORD     enumIndex;
  DWORD     greVersionKeyLen;
  grever    *gverTmp = NULL;
  grever    *gverTail = NULL;
  char      szSupersedeWinRegPath[MAX_BUF];
  char      key[MAX_BUF_TINY];
  char      greVersionKey[MAX_BUF_TINY];
  char      subKey[MAX_BUF];
  char      subKeyInstaller[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      greXpcomPath[MAX_BUF];
  char      greXpcomFile[MAX_BUF];
  int       greXpcomPathLen;
  char      xpcomFilename[] = "xpcom.dll";
  char      greKeyPath[MAX_BUF];
  verBlock  vbInstalledVersion;
  HKEY      hkeyRoot = NULL;
  HKEY      hkGreKeyParentPath = NULL;
  HKEY      hkGreKeyPath = NULL;
  LONG      rv;

  if(*aGreInstalledList)
    /* list already built, just return */
    return(WIZ_OK);

  index = 0;
  wsprintf(key, "SupersedeWinReg%d", index);        
  GetPrivateProfileString(siCObject->szReferenceName, key, "", szSupersedeWinRegPath,
                          sizeof(szSupersedeWinRegPath), szFileIniConfig);
  while(*szSupersedeWinRegPath != '\0')
  {
    BOOL skip = FALSE;
    if(!GetKeyInfo(szSupersedeWinRegPath, szBuf, sizeof(szBuf), KEY_INFO_ROOT))
      // Malformed windows registry key, continue to reading the next key.
      skip = TRUE;

    hkeyRoot = ParseRootKey(szBuf);
    if(!GetKeyInfo(szSupersedeWinRegPath, subKey, sizeof(subKey), KEY_INFO_SUBKEY))
      // Malformed windows registry key, continue to reading the next key.
      skip = TRUE;

    if(RegOpenKeyEx(hkeyRoot, subKey, 0, KEY_READ, &hkGreKeyParentPath) != ERROR_SUCCESS)
      // Problems opening the registry key.  Ignore and continue reading the next key.
      skip = TRUE;

    greVersionKeyLen = sizeof(greVersionKey);
    enumIndex = 0;
    while(!skip &&
         (RegEnumKeyEx(hkGreKeyParentPath, enumIndex++, greVersionKey, &greVersionKeyLen,
                       NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS))
    {
      sprintf(greKeyPath, "%s\\%s", subKey, greVersionKey);
      if(RegOpenKeyEx(hkeyRoot, greKeyPath, 0, KEY_READ, &hkGreKeyPath) != ERROR_SUCCESS)
        // Encoutered problems trying to open key. Just continue.
        // We don't care about any errors.  If we can't open the key,
        // we just go on to other keys.  These errors are nonfatal.
        continue;

      greXpcomPathLen = sizeof(greXpcomPath);
      rv = RegQueryValueEx(hkGreKeyPath, "GreHome", 0, NULL, (BYTE *)greXpcomPath, &greXpcomPathLen);
      RegCloseKey(hkGreKeyPath);

      if(rv != ERROR_SUCCESS)
        // Encoutered problems trying to read a var name. Just continue.
        // We don't care about any errors.  If we can't open the key,
        // we just go on to other keys.  These errors are nonfatal.
        continue;
 
      if(MozCopyStr(greXpcomPath, greXpcomFile, sizeof(greXpcomFile)))
      {
        RegCloseKey(hkGreKeyParentPath);
        PrintError(szEOutOfMemory, ERROR_CODE_HIDE);
        exit(WIZ_OUT_OF_MEMORY);
      }

      if(sizeof(greXpcomFile) <= lstrlen(greXpcomFile) + sizeof(xpcomFilename) + 1)
      {
        RegCloseKey(hkGreKeyParentPath);
        PrintError(szEOutOfMemory, ERROR_CODE_HIDE);
        exit(WIZ_OUT_OF_MEMORY);
      }

      AppendBackSlash(greXpcomFile, sizeof(greXpcomFile));
      lstrcat(greXpcomFile, xpcomFilename);

      if(GetFileVersion(greXpcomFile, &vbInstalledVersion))
      {
        // Don't create a node list unless xpcom.dll file actually exists.
        gverTmp = CreateGVerNode();
        if(!gverTmp)
        {
          RegCloseKey(hkGreKeyParentPath);
          // error has already been displayed.
          exit(WIZ_OUT_OF_MEMORY);
        }

        if(MozCopyStr(greXpcomPath, gverTmp->greHomePath, sizeof(gverTmp->greHomePath)))
        {
          RegCloseKey(hkGreKeyParentPath);
          PrintError(szEOutOfMemory, ERROR_CODE_HIDE);
          exit(WIZ_OUT_OF_MEMORY);
        }

        gverTmp->version.ullMajor   = vbInstalledVersion.ullMajor;
        gverTmp->version.ullMinor   = vbInstalledVersion.ullMinor;
        gverTmp->version.ullRelease = vbInstalledVersion.ullRelease;
        gverTmp->version.ullBuild   = vbInstalledVersion.ullBuild;
        if(*aGreInstalledList == NULL)
        {
          /* brand new list */
          *aGreInstalledList = gverTmp;
          gverTail = *aGreInstalledList;
        }
        else if(gverTail)
        {
          /* append the new node to the end of the list */
          gverTail->next = gverTmp;
          gverTail = gverTail->next;
        }

        /* get the GRE's installer app path */
        sprintf(subKeyInstaller, "%s\\%s\\Installer", subKey, greVersionKey);
        GetWinReg(hkeyRoot, subKeyInstaller, "PathToExe", gverTmp->greInstaller, sizeof(gverTmp->greInstaller));
        MozCopyStr(greVersionKey, gverTmp->greUserAgent, sizeof(gverTmp->greUserAgent));
      }

      greVersionKeyLen = sizeof(greVersionKey);
    }

    if(hkGreKeyParentPath)
    {
      RegCloseKey(hkGreKeyParentPath);
      hkGreKeyParentPath = NULL;
    }

    ++index;
    wsprintf(key, "SupersedeWinReg%d", index);        
    GetPrivateProfileString(siCObject->szReferenceName, key, "", szSupersedeWinRegPath, sizeof(szSupersedeWinRegPath), szFileIniConfig);
  }
  return(WIZ_OK);
}

void SaveGreInfo(grever *aGreInstalledListTmp, greInfo* aGre)
{
  MozCopyStr(aGreInstalledListTmp->greInstaller,     aGre->installerAppPath, sizeof(aGre->installerAppPath));
  MozCopyStr(aGreInstalledListTmp->greUserAgent,     aGre->userAgent,        sizeof(aGre->userAgent));
  MozCopyStr(aGreInstalledListTmp->greHomePath,      aGre->homePath,         sizeof(aGre->homePath));
}

void ResolveSupersedeGre(siC *siCObject, greInfo *aGre)
{
  grever  *greSupersedeListTmp = NULL;
  grever  *greInstalledListTmp = NULL;
  char    versionStr[MAX_BUF_TINY];
  siC     *siCTmp = NULL;
  BOOL    foundVersionWithinRange = FALSE;
  BOOL    minVerRead = FALSE;
  BOOL    maxVerRead = FALSE;
  BOOL    stillInRange = FALSE;

  if(GetGreSupersedeVersionList(siCObject, &aGre->greSupersedeList))
    return;
  if(GetGreInstalledVersionList(siCObject, &aGre->greInstalledList))
    return;
  if(!aGre->greSupersedeList || !aGre->greInstalledList)
    // nothing to compare, return
    return;

  GetPrivateProfileString(siCObject->szReferenceName, "SupersedeMinVersion", "",
                          versionStr, sizeof(versionStr), szFileIniConfig);
  if(*versionStr != '\0')
  {
    TranslateVersionStr(versionStr, &aGre->minVersion);
    minVerRead = TRUE;
  }

  GetPrivateProfileString(siCObject->szReferenceName, "SupersedeMaxVersion", "",
                          versionStr, sizeof(versionStr), szFileIniConfig);
  if(*versionStr != '\0')
  {
    TranslateVersionStr(versionStr, &aGre->maxVersion);
    maxVerRead = TRUE;
  }


  // do the version comparison here.
  greInstalledListTmp = aGre->greInstalledList;
  while(greInstalledListTmp)
  {
    greSupersedeListTmp = aGre->greSupersedeList;
    while(greSupersedeListTmp)
    {
      if(CompareVersion(greInstalledListTmp->version, greSupersedeListTmp->version) == 0)
      {
        SaveGreInfo(greInstalledListTmp, aGre);
        siCObject->bSupersede = TRUE;
        aGre->siCGreComponent = siCObject;
        break;
      }
      else if(!foundVersionWithinRange && (minVerRead || maxVerRead))
      {
        stillInRange = TRUE;

        if(minVerRead)
          stillInRange = CompareVersion(greInstalledListTmp->version, aGre->minVersion) >= 0;

        if(stillInRange && maxVerRead)
          stillInRange = CompareVersion(greInstalledListTmp->version, aGre->maxVersion) <= 0;

        if(stillInRange)
        {
          // Find the first version within the range.
          SaveGreInfo(greInstalledListTmp, aGre);
          foundVersionWithinRange = TRUE;
        }
      }
      greSupersedeListTmp = greSupersedeListTmp->next;
    }

    if(siCObject->bSupersede)
      break;

    greInstalledListTmp = greInstalledListTmp->next;
  }

  if(!siCObject->bSupersede && foundVersionWithinRange)
    siCObject->bSupersede = TRUE;

  if(siCObject->bSupersede)
  {
    siCObject->dwAttributes &= ~SIC_SELECTED;
    siCObject->dwAttributes |= SIC_DISABLED;
    siCObject->dwAttributes |= SIC_INVISIBLE;
  }
  else
    /* Make sure to unset the DISABLED bit.  If the Setup Type is other than
     * Custom, then we don't care if it's DISABLED or not because this flag
     * is only used in the Custom dialogs.
     *
     * If the Setup Type is Custom and this component is DISABLED by default
     * via the config.ini, it's default value will be restored in the
     * SiCNodeSetItemsSelected() function that called ResolveSupersede(). */
    siCObject->dwAttributes &= ~SIC_DISABLED;
}

BOOL ResolveSupersede(siC *siCObject, greInfo *aGre)
{
  DWORD dwIndex;
  char  szFilePath[MAX_BUF];
  char  szSupersedeFile[MAX_BUF];
  char  szSupersedeVersion[MAX_BUF];
  char  szType[MAX_BUF_TINY];
  char  szKey[MAX_BUF_TINY];
  verBlock  vbVersionNew;
  verBlock  vbFileVersion;

  siCObject->bSupersede = FALSE;
  if(siCObject->dwAttributes & SIC_SUPERSEDE)
  {
    dwIndex = 0;
    GetPrivateProfileString(siCObject->szReferenceName, "SupersedeType", "", szType, sizeof(szType), szFileIniConfig);
    if(*szType !='\0')
    {
      if(lstrcmpi(szType, "File Exists") == 0)
      {
        wsprintf(szKey, "SupersedeFile%d", dwIndex);        
        GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szSupersedeFile, sizeof(szSupersedeFile), szFileIniConfig);
        while(*szSupersedeFile != '\0')
        {
          DecryptString(szFilePath, szSupersedeFile);
          if(FileExists(szFilePath))
          {
            wsprintf(szKey, "SupersedeMinVersion%d",dwIndex);
            GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szSupersedeVersion, sizeof(szSupersedeVersion), szFileIniConfig);
            if(*szSupersedeVersion != '\0')
            {
              if(GetFileVersion(szFilePath,&vbFileVersion))
              {
                /* If we can get the version, and it is greater than or equal to the SupersedeVersion
                 * set supersede.  If we cannot get the version, do not supersede the file. */
                TranslateVersionStr(szSupersedeVersion, &vbVersionNew);
                if(CompareVersion(vbFileVersion,vbVersionNew) >= 0)
                {  
                  siCObject->bSupersede = TRUE;
                  break;  /* Found at least one file, so break out of while loop */
                }
              }
            }
            else
            { /* The file exists, and there's no version to check.  set Supersede */
              siCObject->bSupersede = TRUE;
              break;  /* Found at least one file, so break out of while loop */
            }
          }
          wsprintf(szKey, "SupersedeFile%d", ++dwIndex);        
          GetPrivateProfileString(siCObject->szReferenceName, szKey, "", szSupersedeFile, sizeof(szSupersedeFile), szFileIniConfig);
        }
      }
      else if(lstrcmpi(szType, "GRE") == 0)
      {
        /* save the GRE component */
        aGre->siCGreComponent = siCObject;

        /* If -fgre is passed in, and the current product to install is !GRE,
         * and the current component is 'Component GRE' then select and
         * disable it to force it to be installed regardless of supersede
         * rules.
         *
         * If the product is GRE, then it won't have a 'Component GRE', but
         * rather a 'Component XPCOM', in which case it will always get
         * installed */
        if((gbForceInstallGre) && (lstrcmpi(sgProduct.szProductNameInternal, "GRE") != 0))
        {
          siCObject->dwAttributes |= SIC_SELECTED;
          siCObject->dwAttributes |= SIC_DISABLED;
        }
        else
          ResolveSupersedeGre(siCObject, aGre);
      }
    }

    if(siCObject->bSupersede)
    {
      siCObject->dwAttributes &= ~SIC_SELECTED;
      siCObject->dwAttributes |= SIC_DISABLED;
      siCObject->dwAttributes |= SIC_INVISIBLE;
    }
    else
      /* Make sure to unset the DISABLED bit.  If the Setup Type is other than
       * Custom, then we don't care if it's DISABLED or not because this flag
       * is only used in the Custom dialogs.
       *
       * If the Setup Type is Custom and this component is DISABLED by default
       * via the config.ini, it's default value will be restored in the
       * SiCNodeSetItemsSelected() function that called ResolveSupersede(). */
      siCObject->dwAttributes &= ~SIC_DISABLED;
  }
  return(siCObject->bSupersede);
}

