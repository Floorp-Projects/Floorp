/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Special stuff for the Macintosh implementation of command-line service.

#include "nsCommandLineServiceMac.h"

// Mozilla
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsDebug.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsIDOMWindow.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "jsapi.h"

#include "nsAEEventHandling.h"

// NSPR
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"
#include "pprio.h"	// PR_Init_Log

#include "nsAppShellCIDs.h"
static NS_DEFINE_IID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);

// the static instance
nsMacCommandLine nsMacCommandLine::sMacCommandLine;


//----------------------------------------------------------------------------------------
nsMacCommandLine::nsMacCommandLine()
: mArgsBuffer(NULL)
, mArgs(NULL)
, mStartedUp(PR_FALSE)
//----------------------------------------------------------------------------------------
{
}


//----------------------------------------------------------------------------------------
nsMacCommandLine::~nsMacCommandLine()
//----------------------------------------------------------------------------------------
{
  ShutdownAEHandlerClasses();
  nsMemory::Free(mArgsBuffer);
  mArgsBuffer = NULL;
}


//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::Initialize(int& argc, char**& argv)
//----------------------------------------------------------------------------------------
{
  typedef char* charP;
  mArgs = new charP[kMaxTokens];
  mArgs[0] = nsnull;
  argc = 0;
  argv = mArgs;

  // init the args buffer with the program name
  mTempArgsString.Assign("mozilla");

  // Set up AppleEvent handling.
  OSErr err = CreateAEHandlerClasses(false);
  if (err != noErr) return NS_ERROR_FAILURE;

  // Snarf all the odoc and pdoc apple-events.
  //
  // 1. If they are odoc for 'CMDL' documents, read them into the buffer ready for
  //    parsing (concatenating multiple files).
  //
  // 2. If they are any other kind of document, convert them into -url command-line
  //    parameters or -print parameters, with file URLs.

  EventRecord anEvent;
  for (short i = 1; i < 5; i++)
    ::WaitNextEvent(0, &anEvent, 0, nsnull);

  while (::EventAvail(highLevelEventMask, &anEvent))
  {
    ::WaitNextEvent(highLevelEventMask, &anEvent, 0, nsnull);
    if (anEvent.what == kHighLevelEvent)
    {
      // here we process startup odoc/pdoc events, which can 
      // add items to the command line.
      err = ::AEProcessAppleEvent(&anEvent);
    }
  }
  
  // we've started up now
  mStartedUp = PR_TRUE;

  // Care! We have to ensure that the addresses we pass out in
  // argv continue to point to valid memory. Because we can't guarantee
  // anything about the way that nsCString works, we make a copy
  // of the buffer, which is never freed until the command line handler
  // goes way (and, since it's static, that is at library unload time).
  
  mArgsBuffer = mTempArgsString.ToNewCString();
  mTempArgsString.Truncate();   // it's job is done
  
  // Parse the buffer.
  char* strtokFirstParam = mArgsBuffer;
  while (argc < kMaxTokens)
  {
    // Get the next token.  Initialize strtok by passing the string the
    // first time.  Subsequently, pass nil.
    char* nextToken = strtok(strtokFirstParam, " \t\n\r");
    mArgs[argc] = nextToken;
    if (!nextToken)
      break;
    // Loop
    argc++;
    strtokFirstParam = nsnull;
  }

  // Release the unneeded memory.
  if (argc < kMaxTokens)
  {
    charP* oldArgs = mArgs;
    int arraySize = 1 + argc;
    mArgs = new charP[arraySize];
    memcpy(mArgs, oldArgs, arraySize * sizeof(charP));
    delete [] oldArgs;
    argv = mArgs;
  }

  return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToCommandLine(const char* inArgText)
//----------------------------------------------------------------------------------------
{
  if (*inArgText != ' ')
    mTempArgsString.Append(" ");
  mTempArgsString.Append(inArgText);
  return NS_OK;
}


//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToCommandLine(const char* inOptionString, const FSSpec& inFileSpec)
//----------------------------------------------------------------------------------------
{
  // Convert the filespec to a URL
  nsFileSpec nsspec(inFileSpec);
  nsFileURL fileAsURL(nsspec);
  mTempArgsString.Append(" ");
  AddToCommandLine(inOptionString);
  AddToCommandLine(fileAsURL.GetAsString());
  return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToEnvironmentVars(const char* inArgText)
//----------------------------------------------------------------------------------------
{
  (void)PR_PutEnv(inArgText);
  return NS_OK;
}


//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::HandleOpenOneDoc(const FSSpec& inFileSpec, OSType inFileType)
//----------------------------------------------------------------------------------------
{
  if (!mStartedUp)
  {
    // Is it the right type to be a command-line file?
    if (inFileType == 'TEXT' || inFileType == 'CMDL')
    {
      // Can we open the file?
      nsInputFileStream s(inFileSpec);
      if (s.is_open())
      {
        Boolean foundArgs = false;
        Boolean foundEnv = false;
        char chars[1024];
        const char* kCommandLinePrefix = "ARGS:";
        const char* kEnvVarLinePrefix = "ENV:";
        s.readline(chars, sizeof(chars));

        do
        {       // See if there are any command line or environment var settings
          if (PL_strstr(chars, kCommandLinePrefix) == chars)
          {
            (void)AddToCommandLine(chars + PL_strlen(kCommandLinePrefix));
            foundArgs = true;
          }
          else if (PL_strstr(chars, kEnvVarLinePrefix) == chars)
          {
            (void)AddToEnvironmentVars(chars + PL_strlen(kEnvVarLinePrefix));
            foundEnv = true;
          }

          // Clear the buffer and get the next line from the command line file
          chars[0] = '\0';
          s.readline(chars, sizeof(chars));
        } while (PL_strlen(chars));

        // If we found any environment vars we need to re-init NSPR's logging
        // so that it knows what the new vars are
        if (foundEnv)
          PR_Init_Log();

        // If we found a command line or environment vars we want to return now
        // raather than trying to open the file as a URL
        if (foundArgs || foundEnv)
          return noErr;
      }
    }
    // If it's not a command-line argument, and we are starting up the application,
    // add a command-line "-url" argument to the global list. This means that if
    // the app is opened with documents on the mac, they'll be handled the same
    // way as if they had been typed on the command line in Unix or DOS.
    return AddToCommandLine("-url", inFileSpec);
  }
  // Final case: we're not just starting up. How do we handle this?
  nsFileSpec fileSpec(inFileSpec);
  nsFileURL fileURL(fileSpec);
  nsString urlString; urlString.AssignWithConversion(fileURL.GetURLString());
  nsresult rv;
  rv = OpenWindow( "chrome://navigator/content", urlString.get() );
  if (NS_FAILED(rv))
    return errAEEventNotHandled;
  return noErr;
}



//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::HandlePrintOneDoc(const FSSpec& inFileSpec, OSType fileType)
//----------------------------------------------------------------------------------------
{
  // If  we are starting up the application,
  // add a command-line "-print" argument to the global list. This means that if
  // the app is opened with documents on the mac, they'll be handled the same
  // way as if they had been typed on the command line in Unix or DOS.
  if (!mStartedUp)
    return AddToCommandLine("-print", inFileSpec);
  
  // Final case: we're not just starting up. How do we handle this?
  NS_NOTYETIMPLEMENTED("Write Me");
  return errAEEventNotHandled;
}



//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::OpenWindow(const char *chrome, const PRUnichar *url)
//----------------------------------------------------------------------------------------
{
	nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	nsCOMPtr<nsISupportsWString> urlWrapper(do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID));
	if (!wwatch || !urlWrapper)
		return NS_ERROR_FAILURE;

	urlWrapper->SetData(url);

	nsCOMPtr<nsIDOMWindow> newWindow;
	nsresult rv;
	rv = wwatch->OpenWindow(0, chrome, "_blank",
	             "chrome,dialog=no,all", urlWrapper,
	             getter_AddRefs(newWindow));

	return rv;
}

//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::DispatchURLToNewBrowser(const char* url)
//----------------------------------------------------------------------------------------
{
	OSErr err;
	if (mStartedUp)
	{
		nsresult rv;
		rv = OpenWindow("chrome://navigator/content", NS_ConvertASCIItoUCS2(url).get());
		if (NS_FAILED(rv))
			return errAEEventNotHandled;
	}
	else
		err = AddToCommandLine(url);
	
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::Quit(TAskSave askSave)
//----------------------------------------------------------------------------------------
{
	nsresult rv;
	nsCOMPtr<nsIAppShellService> appShellService = 
	         do_GetService(kAppShellServiceCID, &rv);
	if (NS_FAILED(rv))
		return errAEEventNotHandled;
	
	(void)appShellService->Quit();
	return noErr;
}


//========================================================================================
//      InitializeMacCommandLine
//      The only external entry point to this file.
//========================================================================================

#pragma mark -

//----------------------------------------------------------------------------------------
nsresult InitializeMacCommandLine(int& argc, char**& argv)
//----------------------------------------------------------------------------------------
{

  nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
  return cmdLine.Initialize(argc, argv);
} // InitializeMac
