/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xp.h"

// file utils

BOOL XP_IsFile(char * szFileName)
{
#ifdef XP_WIN
  OFSTRUCT of;
  return (HFILE_ERROR != OpenFile(szFileName, &of, OF_EXIST));
#endif
#ifdef XP_UNIX
  struct stat s;
  return (stat(szFileName, &s) != -1);
#endif
#ifdef XP_MAC /*  HACK */
	return 1;
#endif
}

void XP_DeleteFile(char * szFileName)
{
#ifdef XP_WIN
  DeleteFile(szFileName);
#else
  remove(szFileName);
#endif
}

XP_HFILE XP_CreateFile(char * szFileName)
{
#ifdef XP_WIN
  OFSTRUCT of;
  HFILE hFile = OpenFile(szFileName, &of, OF_CREATE | OF_WRITE);
  return (hFile != HFILE_ERROR) ? hFile : NULL;
#else
  return (XP_HFILE)fopen(szFileName, "w+");
#endif
}

XP_HFILE XP_OpenFile(char * szFileName)
{
#ifdef XP_WIN
  OFSTRUCT of;
  HFILE hFile = OpenFile(szFileName, &of, OF_READ | OF_WRITE);
  return (hFile != HFILE_ERROR) ? hFile : NULL;
#else
  return (XP_HFILE)fopen(szFileName, "r+");
#endif
}

void XP_CloseFile(XP_HFILE hFile)
{
  if(hFile != NULL)
  {
#ifdef XP_WIN
    CloseHandle((HANDLE)hFile);
#else
    fclose(hFile);
#endif
  }
}

DWORD XP_WriteFile(XP_HFILE hFile, void * pBuf, int iSize)
{
#ifdef XP_WIN
  DWORD dwRet;
  WriteFile((HANDLE)hFile, pBuf, iSize, &dwRet, NULL);
  return dwRet;
#else
  return (DWORD)fwrite(pBuf, iSize, 1, hFile);
#endif
}

void XP_FlushFileBuffers(XP_HFILE hFile)
{
#ifdef XP_WIN
  FlushFileBuffers((HANDLE)hFile);
#else
  fflush(hFile);
#endif 
}

// misc utils

void * XP_GetSymbol(XP_HLIB hLib, char * szProcName)
{
#ifdef XP_WIN
  return (void *)GetProcAddress(hLib, szProcName);
#endif
}
