/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *     Sean Su <ssu@netscape.com>
 */

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
