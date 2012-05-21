/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "TestySupport.h"

FILE* gLogFile = NULL;

int Testy_LogInit(const char* fileName)
{
  gLogFile = fopen(fileName, "w+b");
  if (!gLogFile) return -1;
  return 0;
}

void Testy_LogShutdown()
{
  if (gLogFile)
    fclose(gLogFile);
}


void Testy_LogStart(const char* name)
{
  PR_ASSERT(gLogFile);
  fprintf(gLogFile, "Test Case: %s", name);
  fflush(gLogFile);
}

void Testy_LogComment(const char* name, const char* comment)
{
  PR_ASSERT(gLogFile);
  fprintf(gLogFile, "Test Case: %s\n\t%s", name, comment);
  fflush(gLogFile);
}

void Testy_LogEnd(const char* name, bool passed)
{
  PR_ASSERT(gLogFile);
  fprintf(gLogFile, "Test Case: %s (%s)", name, passed ? "Passed" : "Failed");
  fflush(gLogFile);
}

void Testy_GenericStartup()
{
  
}

void Testy_GenericShutdown()
{
  
}
