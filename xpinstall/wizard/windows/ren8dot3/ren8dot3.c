/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Sean Su <ssu@netscape.com>
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

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <stdlib.h>

#define _MAX_SIZE_ 64000

char *GetSubString(char *Strings)
{
    return(strchr(Strings,0)+1);
}

void ShowUsage(char *name)
{
    char szBuf[_MAX_SIZE_];

    sprintf(szBuf, "usage: %s <source .ini file>\n", name);
    MessageBox(NULL, szBuf, "Usage", MB_OK);
}

void RemoveQuotes(LPSTR lpszCmdLine, LPSTR szBuf)
{
  char *lpszBegin;

  if(*lpszCmdLine == '\"')
    lpszBegin = &lpszCmdLine[1];
  else
    lpszBegin = lpszCmdLine;

  lstrcpy(szBuf, lpszBegin);

  if(szBuf[lstrlen(szBuf) - 1] == '\"')
    szBuf[lstrlen(szBuf) - 1] = '\0';
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    char *temp;
    char *SubKey;
    char *SubKeyPtr;
    char *SubString;
    char *SubStringPtr;
    char *ReturnedKeys;
    char *ReturnedStrings;
    char *KeyValue;
    char *SrcIni;
    char *DestIni;
    char szBuf[_MAX_SIZE_];

    if(*lpszCmdLine == '\0')
    {
        ShowUsage("Ren8dot3.exe");
        exit(1);
    }

    SrcIni = (char *)calloc(_MAX_DIR, sizeof(char));

    RemoveQuotes(lpszCmdLine, szBuf);
    GetShortPathName(szBuf, SrcIni, _MAX_DIR);

    temp            = (char *)calloc(_MAX_SIZE_, sizeof(char));
    KeyValue        = (char *)calloc(_MAX_SIZE_, sizeof(char));
    ReturnedKeys    = (char *)calloc(_MAX_SIZE_, sizeof(char));
    ReturnedStrings = (char *)calloc(_MAX_SIZE_, sizeof(char));
    SubString       = (char *)calloc(_MAX_SIZE_, sizeof(char));
    DestIni         = (char *)calloc(_MAX_DIR,   sizeof(char));
    SubStringPtr    = SubString;

    GetPrivateProfileString("rename", NULL, "_error_", &ReturnedKeys[1], _MAX_SIZE_, SrcIni);
    SubKey    = (char *)calloc(_MAX_SIZE_, sizeof(char));
    SubKeyPtr = SubKey;
    memcpy(SubKey, ReturnedKeys, _MAX_SIZE_);
    SubKey    = GetSubString(SubKey);
    while((SubKey[0]) != 0)
    {
        GetPrivateProfileString("rename", SubKey, "_error_", KeyValue, _MAX_SIZE_, SrcIni);
        rename(SubKey, KeyValue);
        SubKey = GetSubString(SubKey);
    }

    unlink(SrcIni);

    free(SubKeyPtr);
    free(temp);
    free(ReturnedKeys);
    free(ReturnedStrings);
    free(KeyValue);
    free(SubStringPtr);
    free(SrcIni);
    free(DestIni);

    return(0);
}
