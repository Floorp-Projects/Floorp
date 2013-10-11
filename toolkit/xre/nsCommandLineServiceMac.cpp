/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandLineServiceMac.h"
#include "MacApplicationDelegate.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

namespace CommandLineServiceMac {

static const int kArgsGrowSize = 20;

static char** sArgs = nullptr;
static int sArgsAllocated = 0;
static int sArgsUsed = 0;

static bool sBuildingCommandLine = false;

void AddToCommandLine(const char* inArgText)
{
  if (sArgsUsed >= sArgsAllocated - 1) {
    // realloc does not free the given pointer if allocation fails
    char **temp = static_cast<char**>(realloc(sArgs, (sArgsAllocated + kArgsGrowSize) * sizeof(char*)));
    if (!temp)
      return;
    sArgs = temp;
    sArgsAllocated += kArgsGrowSize;
  }

  char *temp2 = strdup(inArgText);
  if (!temp2)
    return;

  sArgs[sArgsUsed++] = temp2;
  sArgs[sArgsUsed] = nullptr;

  return;
}

void SetupMacCommandLine(int& argc, char**& argv, bool forRestart)
{
  sArgs = static_cast<char **>(malloc(kArgsGrowSize * sizeof(char*)));
  if (!sArgs)
    return;
  sArgsAllocated = kArgsGrowSize;
  sArgs[0] = nullptr;
  sArgsUsed = 0;

  sBuildingCommandLine = true;

  // Copy args, stripping anything we don't want.
  for (int arg = 0; arg < argc; arg++) {
    char* flag = argv[arg];
    // Don't pass on the psn (Process Serial Number) flag from the OS.
    if (strncmp(flag, "-psn_", 5) != 0)
      AddToCommandLine(flag);
  }

  // Force processing of any pending Apple GetURL Events while we're building
  // the command line. The handlers will append to the command line rather than
  // act directly so there is no chance we'll process them during a XUL window
  // load and accidentally open unnecessary windows and home pages.
  ProcessPendingGetURLAppleEvents();

  // If the process will be relaunched, the child should be in the foreground
  // if the parent is in the foreground.  This will be communicated in a
  // command-line argument to the child.
  if (forRestart) {
    Boolean isForeground = false;
    ProcessSerialNumber psnSelf, psnFront;
    if (::GetCurrentProcess(&psnSelf) == noErr &&
        ::GetFrontProcess(&psnFront) == noErr &&
        ::SameProcess(&psnSelf, &psnFront, &isForeground) == noErr &&
        isForeground) {
      AddToCommandLine("-foreground");
    }
  }

  sBuildingCommandLine = false;

  argc = sArgsUsed;
  argv = sArgs;
}

bool AddURLToCurrentCommandLine(const char* aURL)
{
  if (!sBuildingCommandLine) {
    return false;
  }

  AddToCommandLine("-url");
  AddToCommandLine(aURL);

  return true;
}

} // namespace CommandLineServiceMac
