/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser   <sfraser@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mark Mentovai <mark@moxienet.com>
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

#include "nsCommandLineServiceMac.h"
#include "MacApplicationDelegate.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

namespace CommandLineServiceMac {

static const int kArgsGrowSize = 20;

static char** sArgs = NULL;
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
  sArgs[sArgsUsed] = NULL;

  return;
}

void SetupMacCommandLine(int& argc, char**& argv, bool forRestart)
{
  sArgs = static_cast<char **>(malloc(kArgsGrowSize * sizeof(char*)));
  if (!sArgs)
    return;
  sArgsAllocated = kArgsGrowSize;
  sArgs[0] = NULL;
  sArgsUsed = 0;

  sBuildingCommandLine = PR_TRUE;

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

  sBuildingCommandLine = PR_FALSE;

  argc = sArgsUsed;
  argv = sArgs;
}

bool AddURLToCurrentCommandLine(const char* aURL)
{
  if (!sBuildingCommandLine) {
    return PR_FALSE;
  }

  AddToCommandLine("-url");
  AddToCommandLine(aURL);

  return PR_TRUE;
}

} // namespace CommandLineServiceMac
