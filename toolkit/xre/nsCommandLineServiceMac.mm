/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandLineServiceMac.h"

#include "nsString.h"
#include "nsTArray.h"
#include "MacApplicationDelegate.h"
#include <cstring>
#include <Cocoa/Cocoa.h>

namespace CommandLineServiceMac {

static const int kArgsGrowSize = 20;

static char** sArgs = nullptr;
static int sArgsAllocated = 0;
static int sArgsUsed = 0;

void AddToCommandLine(const char* inArgText) {
  if (sArgsUsed >= sArgsAllocated - 1) {
    // realloc does not free the given pointer if allocation fails
    char** temp = static_cast<char**>(
        realloc(sArgs, (sArgsAllocated + kArgsGrowSize) * sizeof(char*)));
    if (!temp) return;
    sArgs = temp;
    sArgsAllocated += kArgsGrowSize;
  }

  char* temp2 = strdup(inArgText);
  if (!temp2) return;

  sArgs[sArgsUsed++] = temp2;
  sArgs[sArgsUsed] = nullptr;

  return;
}

// Caller has ownership of argv and is responsible for freeing the allocated
// memory.
void SetupMacCommandLine(int& argc, char**& argv, bool forRestart) {
  sArgs = static_cast<char**>(malloc(kArgsGrowSize * sizeof(char*)));
  if (!sArgs) return;
  sArgsAllocated = kArgsGrowSize;
  sArgs[0] = nullptr;
  sArgsUsed = 0;

  // Copy args, stripping anything we don't want.
  for (int arg = 0; arg < argc; arg++) {
    char* flag = argv[arg];
    // Don't pass on the psn (Process Serial Number) flag from the OS, or
    // the "-foreground" flag since it will be set below if necessary.
    if (strncmp(flag, "-psn_", 5) != 0 && strncmp(flag, "-foreground", 11) != 0)
      AddToCommandLine(flag);
  }

  // Process the URLs we captured when the NSApp was first run and add them to
  // the command line.
  nsTArray<nsCString> startupURLs = TakeStartupURLs();
  for (const nsCString& url : startupURLs) {
    AddToCommandLine("-url");
    AddToCommandLine(url.get());
  }

  // If the process will be relaunched, the child should be in the foreground
  // if the parent is in the foreground.  This will be communicated in a
  // command-line argument to the child.
  if (forRestart) {
    NSRunningApplication* frontApp =
        [[NSWorkspace sharedWorkspace] frontmostApplication];
    if ([frontApp isEqual:[NSRunningApplication currentApplication]]) {
      AddToCommandLine("-foreground");
    }
  }

  free(argv);
  argc = sArgsUsed;
  argv = sArgs;
}

}  // namespace CommandLineServiceMac
