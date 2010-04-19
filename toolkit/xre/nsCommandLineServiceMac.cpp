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

// Special stuff for the Macintosh implementation of command-line service.

#include "nsCommandLineServiceMac.h"

#include "nsDebug.h"
#include "nsILocalFileMac.h"
#include "nsDebug.h"
#include "nsNetUtil.h"
#include "nsIAppStartup.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsIDOMWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "jsapi.h"
#include "nsReadableUtils.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsICommandLineRunner.h"
#include "nsDirectoryServiceDefs.h"

#include "prmem.h"
#include "plstr.h"
#include "prenv.h"

// the static instance
nsMacCommandLine nsMacCommandLine::sMacCommandLine;

/*
 * ReadLine --
 *
 * Read in a line of text, terminated by CR or LF, from inStream into buf.
 * The terminating CR or LF is not included.  The text in buf is terminated
 * by a null byte.
 * Returns the number of bytes in buf.  If EOF and zero bytes were read, returns -1.
 */

static PRInt32 ReadLine(FILE* inStream, char* buf, PRInt32 bufSize)
{
  PRInt32 charsRead = 0;
  int c;
  
  if (bufSize < 2)
    return -1;

  while (charsRead < (bufSize-1)) {
    c = getc(inStream);
    if (c == EOF || c == '\n' || c == '\r')
      break;
    buf[charsRead++] = c;
  }
  buf[charsRead] = '\0';
  
  return (c == EOF && !charsRead) ? -1 : charsRead; 
}

nsMacCommandLine::nsMacCommandLine()
: mArgs(NULL)
, mArgsAllocated(0)
, mArgsUsed(0)
, mStartedUp(PR_FALSE)
{
}

nsMacCommandLine::~nsMacCommandLine()
{
  if (mArgs) {
    for (PRUint32 i = 0; i < mArgsUsed; i++)
      free(mArgs[i]);
    free(mArgs);
  }
}

nsresult nsMacCommandLine::Initialize(int& argc, char**& argv)
{
  mArgs = static_cast<char **>(malloc(kArgsGrowSize * sizeof(char *)));
  if (!mArgs)
    return NS_ERROR_FAILURE;
  mArgs[0] = nsnull;
  mArgsAllocated = kArgsGrowSize;
  mArgsUsed = 0;
  
  // Here, we may actually get useful args.
  // Copy them first to mArgv.
  for (int arg = 0; arg < argc; arg++) {
    char* flag = argv[arg];
    // don't pass on the psn (Process Serial Number) flag from the OS
    if (strncmp(flag, "-psn_", 5) != 0)
      AddToCommandLine(flag);
  }

  // we've started up now
  mStartedUp = PR_TRUE;
  
  argc = mArgsUsed;
  argv = mArgs;
  
  return NS_OK;
}

void nsMacCommandLine::SetupCommandLine(int& argc, char**& argv, PRBool forRestart)
{
  // Initializes the command line from Apple Events and other sources,
  // as appropriate for OS X.
  //
  // IMPORTANT: This must be done before XPCOM shutdown if the app is to
  // relaunch (i.e. before the ScopedXPCOMStartup object goes out of scope).
  // XPCOM shutdown can cause other things to process native events, and
  // native event processing can cause the waiting Apple Events to be
  // discarded.

  // Process Apple Events and put them into the arguments.
  Initialize(argc, argv);

  if (forRestart) {
    Boolean isForeground = PR_FALSE;
    ProcessSerialNumber psnSelf, psnFront;

    // If the process will be relaunched, the child should be in the foreground
    // if the parent is in the foreground.  This will be communicated in a
    // command-line argument to the child.
    if (::GetCurrentProcess(&psnSelf) == noErr &&
        ::GetFrontProcess(&psnFront) == noErr &&
        ::SameProcess(&psnSelf, &psnFront, &isForeground) == noErr &&
        isForeground) {
      // The process is currently in the foreground.  The relaunched
      // process should come to the front, too.
      AddToCommandLine("-foreground");
    }
  }

  argc = mArgsUsed;
  argv = mArgs;
}

nsresult nsMacCommandLine::AddToCommandLine(const char* inArgText)
{
  if (mArgsUsed >= mArgsAllocated - 1) {
    // realloc does not free the given pointer if allocation fails.
    char **temp = static_cast<char **>(realloc(mArgs, (mArgsAllocated + kArgsGrowSize) * sizeof(char *)));
    if (!temp)
      return NS_ERROR_OUT_OF_MEMORY;
    mArgs = temp;
    mArgsAllocated += kArgsGrowSize;
  }
  char *temp2 = strdup(inArgText);
  if (!temp2)
    return NS_ERROR_OUT_OF_MEMORY;
  mArgs[mArgsUsed++] = temp2;
  mArgs[mArgsUsed] = nsnull;
  return NS_OK;
}

nsresult nsMacCommandLine::AddToCommandLine(const char* inOptionString, const CFURLRef file)
{
  CFStringRef string = ::CFURLGetString(file);
  if (!string)
    return NS_ERROR_FAILURE;

  CFIndex length = ::CFStringGetLength(string);
  CFIndex bufLen = 0;
  ::CFStringGetBytes(string, CFRangeMake(0, length), kCFStringEncodingUTF8,
                     0, PR_FALSE, nsnull, 0, &bufLen);

  UInt8 buffer[bufLen + 1];
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;

  ::CFStringGetBytes(string, CFRangeMake(0, length), kCFStringEncodingUTF8,
                     0, PR_FALSE, buffer, bufLen, nsnull);
  buffer[bufLen] = 0;

  AddToCommandLine(inOptionString);  
  AddToCommandLine((char*)buffer);

  return NS_OK;
}

nsresult nsMacCommandLine::AddToEnvironmentVars(const char* inArgText)
{
  (void)PR_SetEnv(inArgText);
  return NS_OK;
}

nsresult nsMacCommandLine::HandleOpenOneDoc(const CFURLRef file, OSType inFileType)
{
  nsCOMPtr<nsILocalFileMac> inFile;
  nsresult rv = NS_NewLocalFileWithCFURL(file, PR_TRUE, getter_AddRefs(inFile));
  if (NS_FAILED(rv))
    return rv;

  if (!mStartedUp) {
    // Is it the right type to be a command-line file?
    if (inFileType == 'TEXT' || inFileType == 'CMDL') {
      // Can we open the file?
      FILE *fp = 0;
      rv = inFile->OpenANSIFileDesc("r", &fp);
      if (NS_SUCCEEDED(rv)) {
        Boolean foundArgs = false;
        Boolean foundEnv = false;
        char chars[1024];
        static const char kCommandLinePrefix[] = "ARGS:";
        static const char kEnvVarLinePrefix[] = "ENV:";

        while (ReadLine(fp, chars, sizeof(chars)) != -1) {
          // See if there are any command line or environment var settings
          if (PL_strstr(chars, kCommandLinePrefix) == chars) {
            AddToCommandLine(chars + sizeof(kCommandLinePrefix) - 1);
            foundArgs = true;
          }
          else if (PL_strstr(chars, kEnvVarLinePrefix) == chars) {
            AddToEnvironmentVars(chars + sizeof(kEnvVarLinePrefix) - 1);
            foundEnv = true;
          }
        }

        fclose(fp);
        // If we found a command line or environment vars we want to return now
        // rather than trying to open the file as a URL
        if (foundArgs || foundEnv)
          return NS_OK;
      }
    }
    // If it's not a command-line argument, and we are starting up the application,
    // add a command-line "-url" argument to the global list. This means that if
    // the app is opened with documents on the mac, they'll be handled the same
    // way as if they had been typed on the command line in Unix or DOS.
    return AddToCommandLine("-url", file);
  }

  // Final case: we're not just starting up, use the arg as a -file <arg>
  nsCOMPtr<nsICommandLineRunner> cmdLine
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
  if (!cmdLine) {
    NS_ERROR("Couldn't create command line!");
    return NS_ERROR_FAILURE;
  }
  nsCString filePath;
  rv = inFile->GetNativePath(filePath);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> workingDir;
  rv = NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(workingDir));
  if (NS_FAILED(rv))
    return rv;

  const char *argv[3] = {nsnull, "-file", filePath.get()};
  rv = cmdLine->Init(3, const_cast<char**>(argv), workingDir, nsICommandLine::STATE_REMOTE_EXPLICIT);
  if (NS_FAILED(rv))
    return rv;
  rv = cmdLine->Run();
  return rv;
}

nsresult nsMacCommandLine::HandlePrintOneDoc(const CFURLRef file, OSType fileType)
{
  // If  we are starting up the application,
  // add a command-line "-print" argument to the global list. This means that if
  // the app is opened with documents on the mac, they'll be handled the same
  // way as if they had been typed on the command line in Unix or DOS.
  if (!mStartedUp)
    return AddToCommandLine("-print", file);

  // Final case: we're not just starting up. How do we handle this?
  NS_NOTYETIMPLEMENTED("Write Me");
  return NS_ERROR_FAILURE;
}

nsresult nsMacCommandLine::DispatchURLToNewBrowser(const char* url)
{
  nsresult rv = AddToCommandLine("-url");
  if (NS_SUCCEEDED(rv))
    rv = AddToCommandLine(url);

  return rv;
}

#pragma mark -

void SetupMacCommandLine(int& argc, char**& argv, PRBool forRestart)
{
  nsMacCommandLine& cmdLine = nsMacCommandLine::GetMacCommandLine();
  return cmdLine.SetupCommandLine(argc, argv, forRestart);
}
