/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Navigator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape Communications Corp. are
 * Copyright (C) 1998, 1999, 2000, 2001 Netscape Communications Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Sean Su <ssu@netscape.com>
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_BUF 4096
int main(int argc, char *argv[])
{
  DWORD dwError;
  char  szShortPath[MAX_BUF];

  if(argc <= 1)
  {
    printf("\n usage: %s <path>\n", argv[0]);
    exit(1);
  }

  dwError = GetShortPathName(argv[1], szShortPath, sizeof(szShortPath));
  printf("%s", szShortPath);
  return(dwError);
}

