/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser   <sfraser@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

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
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsIDOMWindowInternal.h"
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
: mArgBuffer(NULL)
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
      err = ::AEProcessAppleEvent(&anEvent);
  }

  
  // we've started up now
  mStartedUp = PR_TRUE;
  
  if (!mArgBuffer)      // no arguments from anywhere
    return NS_OK;

  // Release some unneeded memory
  char* oldBuffer = mArgBuffer;
  mArgBuffer = new char[PL_strlen(mArgBuffer) + 1];
  PL_strcpy(mArgBuffer, oldBuffer);
  delete [] oldBuffer;

  // Parse the buffer.
  int     rowNumber = 0;
  char* strtokFirstParam = mArgBuffer;
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
PRBool nsMacCommandLine::EnsureCommandLine()
//----------------------------------------------------------------------------------------
{
  if (mArgBuffer)
    return PR_TRUE;

  mArgBuffer = new char[kMaxBufferSize];
  if (!mArgBuffer)
    return PR_FALSE;

  PL_strcpy(mArgBuffer, "mozilla"); // argv[0]
  return PR_TRUE;
}


//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToCommandLine(const char* inArgText)
//----------------------------------------------------------------------------------------
{
  if (!EnsureCommandLine())
    return NS_ERROR_OUT_OF_MEMORY;

  if (*inArgText != ' ')
    PL_strcat(mArgBuffer, " ");
  PL_strcat(mArgBuffer, inArgText);
  return NS_OK;
}


//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToCommandLine(const char* inOptionString, const FSSpec& inFileSpec)
//----------------------------------------------------------------------------------------
{
  if (!EnsureCommandLine())
    return NS_ERROR_OUT_OF_MEMORY;

  // Convert the filespec to a URL
  nsFileSpec nsspec(inFileSpec);
  nsFileURL fileAsURL(nsspec);
  PL_strcat(mArgBuffer, " ");
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
  rv = OpenWindow( "chrome://navigator/content", urlString.GetUnicode() );
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
	nsresult rv;
    NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &rv)
    if (NS_SUCCEEDED(rv))
    {
    	nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
	    JSContext *jsContext;
    	rv = appShellService->GetHiddenWindowAndJSContext( getter_AddRefs( hiddenWindow ),
                                                           &jsContext );
	    if (NS_SUCCEEDED(rv))
	    {
		    void *stackPtr;
		    jsval *argv = JS_PushArguments( jsContext,
                		                    &stackPtr,
                		                    "sssW",
                        		            chrome,
                                		    "_blank",
		                                    "chrome,dialog=no,all",
        		                            url );
		    if(argv)
		    {
			    nsCOMPtr<nsIDOMWindowInternal> newWindow;
			    rv = hiddenWindow->OpenDialog( jsContext,
			                                   argv,
			                                   4,
			                                   getter_AddRefs( newWindow ) );
			    JS_PopArguments( jsContext, stackPtr );
			}
		}
	}
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
		rv = OpenWindow("chrome://navigator/content", NS_ConvertASCIItoUCS2(url).GetUnicode());
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
	NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &rv);
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
