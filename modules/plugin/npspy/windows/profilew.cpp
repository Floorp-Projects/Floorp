/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include <windows.h>

#include "profilew.h"

ProfileWin::ProfileWin() : Profile()
{
  hKey = NULL;
  char szClass[] = "SpyPluginClass";
  DWORD disp = 0L;

  LONG res = RegCreateKeyEx(HKEY_LOCAL_MACHINE, 
                            NPSPY_REG_SUBKEY, 
                            0L, 
                            szClass, 
                            0L, 
                            KEY_READ | KEY_WRITE, 
                            NULL, 
                            &hKey, 
                            &disp);

  if(res != ERROR_SUCCESS)
    hKey = NULL;
}

ProfileWin::~ProfileWin()
{
  if(hKey)
    RegCloseKey(hKey);
}

BOOL ProfileWin::getBool(char * key, BOOL * value)
{
  if(!value)
    return FALSE;

  DWORD size = sizeof(DWORD);
  DWORD val = 1L;
  LONG res = RegQueryValueEx(hKey, key, 0L, NULL, (BYTE *)&val, &size);

  if(res != ERROR_SUCCESS)
    return FALSE;

  *value = (val == 0L) ? FALSE : TRUE;

  return TRUE;
}

BOOL ProfileWin::setBool(char * key, BOOL value)
{
  DWORD size = sizeof(DWORD);
  DWORD val = value ? 1L : 0L;
  LONG res = RegSetValueEx(hKey, key, 0L, REG_DWORD, (const BYTE *)&val, size);
  return (res == ERROR_SUCCESS);
}

BOOL ProfileWin::getString(char * key, char * string, int size)
{
  LONG res = RegQueryValueEx(hKey, key, 0L, NULL, (BYTE *)string, (DWORD *)&size);
  return (res == ERROR_SUCCESS);
}

BOOL ProfileWin::setString(char * key, char * string)
{
  DWORD size = strlen(string);
  LONG res = RegSetValueEx(hKey, key, 0L, REG_SZ, (const BYTE *)string, size);
  return (res == ERROR_SUCCESS);
}

BOOL ProfileWin::getSizeAndPosition(int *width, int *height, int *x, int *y)
{
  DWORD size = sizeof(DWORD);
  LONG res = ERROR_SUCCESS;

  res = RegQueryValueEx(hKey, NPSPY_REG_KEY_WIDTH, 0L, NULL, (BYTE *)width, &size);
  if(res != ERROR_SUCCESS)
    return FALSE; 

  res = RegQueryValueEx(hKey, NPSPY_REG_KEY_HEIGHT, 0L, NULL, (BYTE *)height, &size);
  if(res != ERROR_SUCCESS)
    return FALSE; 

  res = RegQueryValueEx(hKey, NPSPY_REG_KEY_X, 0L, NULL, (BYTE *)x, &size);
  if(res != ERROR_SUCCESS)
    return FALSE; 

  res = RegQueryValueEx(hKey, NPSPY_REG_KEY_Y, 0L, NULL, (BYTE *)y, &size);
  if(res != ERROR_SUCCESS)
    return FALSE; 

  return TRUE;
}

BOOL ProfileWin::setSizeAndPosition(int width, int height, int x, int y)
{
  DWORD size = sizeof(DWORD);
  LONG res = ERROR_SUCCESS;
  
  res = RegSetValueEx(hKey, NPSPY_REG_KEY_WIDTH, 0L, REG_DWORD, (const BYTE *)&width, size);
  if(res != ERROR_SUCCESS)
    return FALSE;
  
  res = RegSetValueEx(hKey, NPSPY_REG_KEY_HEIGHT, 0L, REG_DWORD, (const BYTE *)&height, size);
  if(res != ERROR_SUCCESS)
    return FALSE;
  
  res = RegSetValueEx(hKey, NPSPY_REG_KEY_X, 0L, REG_DWORD, (const BYTE *)&x, size);
  if(res != ERROR_SUCCESS)
    return FALSE;
  
  res = RegSetValueEx(hKey, NPSPY_REG_KEY_Y, 0L, REG_DWORD, (const BYTE *)&y, size);
  if(res != ERROR_SUCCESS)
    return FALSE;

  return TRUE;
}
