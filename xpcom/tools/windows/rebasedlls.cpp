/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include <windows.h>
#include <stdio.h>
#include <imagehlp.h>
#include <time.h>

static char* gSymbolPath = NULL;
static DWORD gBaseAddr = (DWORD) 0x60000000;
static time_t now;

static void
ReBase(char* aDLLName)
{
  DWORD oldsize, oldbase, newsize;
  DWORD newbase = gBaseAddr;
  BOOL b = ReBaseImage(aDLLName, gSymbolPath, TRUE, FALSE, FALSE,
		       0, &oldsize, &oldbase, &newsize, &newbase,
		       now);
  DWORD lastError = ::GetLastError();
  printf("%-20s %08x %08x %08x %08x",
	 aDLLName, oldbase, oldsize, newbase, newsize);
  if (!b) {
    printf(" (failed: error=%d/0x%x)", lastError, lastError);
  }
  fputs("\n", stdout);

  // advance base; round size up to nearest 64k
  newsize = ((newsize + 65535) >> 16) << 16;
  gBaseAddr = newbase + newsize;
}

static void
Usage()
{
  printf("Usage: ReBase [-libs library-path] dlls...\n");
}

int
main(int argc, char** argv)
{
  now = time(0);
  int i;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-libs") == 0) {
	if (i == argc - 1) {
	  Usage();
	  return -1;
	}
	gSymbolPath = argv[++i];
      }
      else {
	Usage();
	return -1;
      }
    }
    else
      break;
  }

  if (i < argc) {
    printf("Library              OldBase  OldSize  NewBase  NewSize\n");
    printf("-------              -------  -------  -------  -------\n");
  }
  for (; i < argc; i++) {
    ReBase(argv[i]);
  }
  return 0;
}
