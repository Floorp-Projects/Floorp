/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// Special stuff for the Macintosh implementation of command-line service.

#include "nsCommandLineServiceMac.h"

// Mozilla
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsAppleEvents.h"
#include "nsDebug.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsIDOMToolkitCore.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIXULWindowCallbacks.h"

// NSPR
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"
#include "prlog.h"

// Universal
#include <AppleEvents.h>
#include <AEObjects.h>
#include <AERegistry.h>
#include <Processes.h>
#include <Displays.h>		// For the display notice event
#include <Balloons.h>
#include <Errors.h>
#include <EPPC.h>

// PowerPlant
#include <UAppleEventsMgr.h>
#include <UExtractFromAEDesc.h>

#include "nsAppCoresCIDs.h"
static NS_DEFINE_IID(kToolkitCoreCID, NS_TOOLKITCORE_CID);

#include "nsAppShellCIDs.h"
static NS_DEFINE_IID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);

// Global buffers for command-line arguments and parsing.
#define MAX_BUF 512
#define MAX_TOKENS 20
static char* argBuffer = nsnull; // the command line itself
static char** args = nsnull; // array of pointers into argBuffer

static nsIWebShellWindow* FindWebShellWindow(nsIXULWindowCallbacks* cb);

//========================================================================================
class nsAppleEventHandler
//========================================================================================
{
	

public:
	enum KioskEnum
	{
		KioskOff = 0
	,	KioskOn = 1
	};
	
						nsAppleEventHandler();
	virtual				~nsAppleEventHandler();

	static nsAppleEventHandler* Get()
						{
							if (!sAppleEventHandler)
								new nsAppleEventHandler();
							return sAppleEventHandler;
						}

	void				SetStartedUp(PRBool inStarted) { mStartedUp = inStarted; }

	// --- Top Level Apple Event Handling
	
	virtual OSErr 		HandleAppleEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	virtual OSErr		GetAEProperty(DescType inProperty,
									const AEDesc	&inRequestedType,
									AEDesc			&outPropertyDesc) const;
	virtual OSErr		SetAEProperty(DescType inProperty,
							const AEDesc	&inRequestedType,
							AEDesc			&outPropertyDesc);

	// ---  AEOM support
	OSErr				GetSubModelByUniqueID(DescType		inModelID,
									const AEDesc	&inKeyData,
									AEDesc			&outToken) const;

	static KioskEnum 	GetKioskMode(){return sAppleEventHandler->mKioskMode;}

private:

	
	OSErr				HandleAEOpenOrPrintDoc(
									const AppleEvent	&inAppleEvent,
									AppleEvent&			outAEReply,
									SInt32				inAENumber);

	OSErr				HandleOpen1Doc(const FSSpec&		inFileSpec,
									OSType 				fileType);

	OSErr				HandlePrint1Doc(const FSSpec&	inFileSpec,
									OSType 				fileType);

	PRBool				EnsureCommandLine();
	
	OSErr				AddToCommandLine(const char* inArgText);
	
	OSErr				AddToCommandLine(const char*	inOptionString,
									const FSSpec&		inFileSpec);

	void				AddToEnvironmentVars(const char* inArgText);
	
	OSErr				DispatchURLToNewBrowser(const char* url);
	
	OSErr 				HandleOpenURLEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
// spy Apple Event suite
// file/URL opening + misc
	OSErr 				HandleGetURLEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
									
// Netscape suite
	OSErr				HandleDoJavascriptEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);

	static pascal OSErr	AppleEventHandler(
								const AppleEvent*	inAppleEvent,
								AppleEvent*			outAEReply,
								UInt32				inRefCon);

protected:
	KioskEnum			mKioskMode;
	PRBool				mStartedUp;			
	static nsAppleEventHandler*	sAppleEventHandler;		// One and only instance of AEvents

}; // class nsAppleEventHandler

//========================================================================================
class MoreExtractFromAEDesc
//	Apple event helpers -- extension of UExtractFromAEDesc.h
//	All the miscellaneous AppleEvent helper routines.
//========================================================================================
{
public:

	// --------------------------------------------------------------
	/*	Given an AppleEvent, locate a string given a keyword and
		return the string
		In:	Event to search
			AEKeyword assocaated with the string
			C string ptr
		Out: Pointer to a newly created C string returned */
	// --------------------------------------------------------------
	static void GetCString(const AppleEvent	&inAppleEvent, AEKeyword keyword, 
			char * & s, Boolean inThrowIfError = true);

	// --------------------------------------------------------------
	/*	Given an AEDesc of type typeChar, return it's string.
		In:	AEDesc containing a string
			C string ptr
		Out: Pointer to a newly created C string returned */
	// --------------------------------------------------------------
	static void TheCString(const AEDesc &inDesc, char * & outPtr);
	
	// --------------------------------------------------------------
	/*	Add an error string and error code to an AppleEvent.
		Typically used when constructing the return event when an
		error occured
		In:	Apple Event to append to
			Error string
			Error code
		Out: keyErrorNum and keyErrorSting AEDescs are added to the Event. */
	// --------------------------------------------------------------
	static void MakeErrorReturn(AppleEvent &event, ConstStr255Param errorString, 
			OSErr errorCode);

	// --------------------------------------------------------------
	/*	Display an error dialog if the given AppleEvent contains
		a keyErrorNumber.  a keyErrorString is optional and will be
		displayed if present
		In:	Apple Event
		Out: Error dialog displayed if error data present. */
	// --------------------------------------------------------------
	static Boolean DisplayErrorReply(AppleEvent &reply);

	// --------------------------------------------------------------
	/*	Return the process serial number of the sending process.
		In:	Apple Event send by some app.
		Out: ProcessSerialNumber of the sending app. */
	// --------------------------------------------------------------
	static ProcessSerialNumber ExtractAESender(const AppleEvent &inAppleEvent);

	static void DispatchURLDirectly(const AppleEvent &inAppleEvent);
}; // class MoreExtractFromAEDesc


//========================================================================================
class AEUtilities
//	Some more simple Apple Event utility routines.
//========================================================================================
{
public:

	// --------------------------------------------------------------
	/*	CreateAppleEvent
		Create a new Apple Event from scratch.
		In:	Apple Event suite
			Apple Event ID
			Ptr to return Apple Event
			ProcessSerialNumber of the target app to send event to.
		Out:A new Apple Event is created.  More data may be added to
			the event simply by calling AEPutParamDesc or AEPutParamPtr */
	// --------------------------------------------------------------
	static	OSErr CreateAppleEvent(OSType suite, OSType id, 
		AppleEvent &event, ProcessSerialNumber targetPSN);

	// --------------------------------------------------------------
	/*	Check to see if there is an error in the given AEvent.
		We simply return an OSError equiv to the error value
		in the event.  If none exists (or an error took place
		during access) we return 0.
		In:	Apple Event to test
		Out:Error value returned */
	// --------------------------------------------------------------
	static	OSErr EventHasErrorReply(AppleEvent & reply);

}; // AEUtilities

//========================================================================================
//					Implementation of nsAppleEventHandler
//========================================================================================

#pragma mark -

nsAppleEventHandler* nsAppleEventHandler::sAppleEventHandler = nsnull;

//----------------------------------------------------------------------------------------
nsAppleEventHandler::nsAppleEventHandler()
//----------------------------------------------------------------------------------------
:	mKioskMode(KioskOff) // We assume not operating in kiosk mode unless told otherwise
,	mStartedUp(PR_FALSE)
{
	// We're a singleton class
	sAppleEventHandler = this;
	
	// We must not throw out of here.
	try
	{
		UAppleEventsMgr::InstallAEHandlers(
			NewAEEventHandlerProc(nsAppleEventHandler::AppleEventHandler));
	}
	catch(...)
	{
	}
}

//----------------------------------------------------------------------------------------
nsAppleEventHandler::~nsAppleEventHandler()
//----------------------------------------------------------------------------------------
{
	// We're a singleton class
	sAppleEventHandler = nsnull;
}

//----------------------------------------------------------------------------------------
pascal OSErr nsAppleEventHandler::AppleEventHandler(
		const AppleEvent*	inAppleEvent,
		AppleEvent*			outAEReply,
		UInt32				inRefCon)
// This is the function that gets installed as the AE callback.
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	// We must not throw out of here.
	try
	{
		StAEDescriptor resultDesc;
		err = sAppleEventHandler->HandleAppleEvent(
				*inAppleEvent, *outAEReply, resultDesc, inRefCon);
	}
	catch(...)
	{
		err = errAEEventNotHandled;
	}
	return err;
} // nsAppleEventHandler::AppleEventHandler

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleAppleEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&outResult,
	SInt32				inAENumber)
//	nsAppleEventHandler::HandleAppleEvent
//	AppleEvent Dispach Here
//	In: & AppleEvent	Incoming apple event
//		& ReplyEvent	Reply event we can attach result info to
//		& Descriptor	OUtgoing descriptor
//		  AppleEvent ID	The Apple Event ID we were called with.
//	Out: Event Handled by CAppleEvent
//----------------------------------------------------------------------------------------
{
	OSErr err = errAEEventNotHandled;
	switch (inAENumber)
	{
		case AE_GetURL:
		case AE_OpenURL:
		case ae_OpenApp:
		case ae_OpenDoc:
		case ae_PrintDoc:
		case ae_Quit:
			break;
			
		default:
			// Ignore all apple events handled by this handler except get url, open
			// url and the required suite until we have properly started up.
			if (!mStartedUp)
				return errAEEventNotHandled;			
			break;
	}

	switch (inAENumber)
	{	
	
		case ae_OpenApp:
			err = noErr;
			break;
			
		case ae_Quit:
		{
			nsresult rv;
			NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &rv);
			if (NS_FAILED(rv))
				return errAEEventNotHandled;
			appShellService->Shutdown();
			break;
		}	
		case ae_OpenDoc:
		case ae_PrintDoc:
			err = HandleAEOpenOrPrintDoc(inAppleEvent, outAEReply, inAENumber);
			break;
			
		// ### Mac URL standard Suite
		// -----------------------------------------------------------
		case AE_GetURL:	// Direct object - url, cwin - HyperWindow
			err = HandleGetURLEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		// ### Spyglass Suite
		// -----------------------------------------------------------
		case AE_OpenURL:
			err = HandleOpenURLEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
			
		// ### Netscape Experimental Suite
		// -----------------------------------------------------------
		case AE_DoJavascript:
			err = HandleDoJavascriptEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		// ### If all else fails behave like a word processor
		// -----------------------------------------------------------
		default:
			NS_NOTYETIMPLEMENTED("Write Me");
			//CFrontApp::sApplication->LDocApplication::HandleAppleEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
	}
	return noErr;
} // nsAppleEventHandler::HandleAppleEvent

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleAEOpenOrPrintDoc(
	const AppleEvent	&inAppleEvent,
	AppleEvent&			/* outAEReply */,
	SInt32				inAENumber)
//----------------------------------------------------------------------------------------
{
	AEDescList docList;
	OSErr err = ::AEGetParamDesc(&inAppleEvent, keyDirectObject,
							typeAEList, &docList);
	if (err) return err;
	
	SInt32	numDocs;
	err = ::AECountItems(&docList, &numDocs);
	if (err)
			goto Clean;
	
	// Loop through all items in the list
	// Extract descriptor for the document
	// Coerce descriptor data into a FSSpec
	// Dispatch the print or open to the appropriate routine
	for (int i = 1; i <= numDocs; i++)
	{
		AEKeyword keyWord;
		DescType descType;
		FSSpec spec;
		Size theSize;
		err = ::AEGetNthPtr(&docList, i, typeFSS, &keyWord, &descType,
							(Ptr) &spec, sizeof(FSSpec), &theSize);
		if (err)
			goto Clean;
		
		nsFileSpec nsspec(spec);
		CInfoPBRec info;
		nsspec.GetCatInfo(info);
		OSType fileType = info.hFileInfo.ioFlFndrInfo.fdType;
		if (inAENumber == ae_OpenDoc)
			err = HandleOpen1Doc(spec, fileType);
		else
			err = HandlePrint1Doc(spec, fileType);
	}
Clean:
	::AEDisposeDesc(&docList);
	return err;
} // nsAppleEventHandler::HandleAEOpenOrPrintDoc

//----------------------------------------------------------------------------------------
PRBool nsAppleEventHandler::EnsureCommandLine()
//----------------------------------------------------------------------------------------
{
    if (argBuffer)
    	return PR_TRUE;
    argBuffer = new char[MAX_BUF];
    if (!argBuffer)
    	return PR_FALSE;
	PL_strcpy(argBuffer, "mozilla"); // argv[0]
	return PR_TRUE;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::AddToCommandLine(const char* inOptionString, const FSSpec& inFileSpec)
//----------------------------------------------------------------------------------------
{
	// Convert the filespec to a URL
	nsFileSpec nsspec(inFileSpec);
	nsFileURL fileAsURL(nsspec);
	PL_strcat(argBuffer, " ");
	AddToCommandLine(inOptionString);
	AddToCommandLine(fileAsURL.GetAsString());
	return noErr;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::AddToCommandLine(const char* inArgText)
//----------------------------------------------------------------------------------------
{
	if (!EnsureCommandLine())
		return memFullErr;
	if (*inArgText != ' ')
		PL_strcat(argBuffer, " ");
	PL_strcat(argBuffer, inArgText);
	return noErr;
}

//----------------------------------------------------------------------------------------
void nsAppleEventHandler::AddToEnvironmentVars(const char* inArgText)
//----------------------------------------------------------------------------------------
{
	(void)PR_PutEnv(inArgText);
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleOpen1Doc(const FSSpec& inFileSpec, OSType inFileType)
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
				{	// See if there are any command line or environment var settings
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
	nsString urlString(fileURL.GetURLString());
	nsresult rv;
	NS_WITH_SERVICE(nsIDOMToolkitCore, toolkitCore, kToolkitCoreCID, &rv)
	if (NS_FAILED(rv))
		return errAEEventNotHandled;
	rv = toolkitCore->ShowWindowWithArgs(
		"chrome://navigator/content",
		nsnull,
		urlString);
	if (NS_FAILED(rv))
		return errAEEventNotHandled;
#if 0
	// Another way, but this way loads the URL as the "chrome" - no toolbar, etc.
	// Personally, I like this better, but it's not "right" :-)

	nsCOMPtr<nsIWebShellWindow> aWindow = getter_AddRefs(FindWebShellWindow(nsnull));
	if (!aWindow)
		return errAEEventNotHandled;

	nsCOMPtr<nsIWebShell> webShell;
	rv = aWindow->GetWebShell(*getter_AddRefs(webShell));
	if (NS_FAILED(rv) || !webShell)
		return errAEEventNotHandled;

	rv = webShell->LoadURL(
		urlString.GetUnicode(),
		/*nsIPostData* aPostData=*/nsnull,
		/*PRBool aModifyHistory=*/PR_TRUE,
		/*nsURLReloadType aType=*/nsURLReload,
		/*const PRUint32 aLocalIP=*/0);
	if (NS_FAILED(rv))
		return errAEEventNotHandled;
	aWindow->Show(PR_TRUE);
#endif
	return noErr;
} // nsAppleEventHandler::HandleOpen1Doc

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandlePrint1Doc(const FSSpec& inFileSpec, OSType fileType)
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

/*##########################################################################*/	
/*			----  Mac URL standard, supported by many apps   ----			*/
/*##########################################################################*/	

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::DispatchURLToNewBrowser(const char* url)
//----------------------------------------------------------------------------------------
{
	OSErr err;
	if (mStartedUp)
	{
		nsresult rv;
		NS_WITH_SERVICE(nsIDOMToolkitCore, toolkitCore, kToolkitCoreCID, &rv)
		if (NS_FAILED(rv))
			return errAEEventNotHandled;
		rv = toolkitCore->ShowWindowWithArgs(
			"chrome://navigator/content",
			nsnull,
			url);
		if (NS_FAILED(rv))
			return errAEEventNotHandled;
	}
	else
		err = AddToCommandLine(url);
	return err;
} // nsAppleEventHandler::DispatchURLToNewBrowser

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleGetURLEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc&				outResult,
	SInt32				/*inAENumber*/)
//	nsAppleEventHandler::HandleGetURLEvent			Suite:URL
//	Dispach the URL event to the correct window and let the
//	window handle the event.
//
//GetURL: Loads the URL (optionaly to disk)
//
//	GetURL  string  -- The url 
//		[to  file specification] 	-- file the URL should be loaded into  
//		[inside  reference]  		-- Window the URL should be loaded to
//		[from  string] 				-- Refererer, to be sent with the HTTP request
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	char* url = nsnull;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
	if (!url)
		return errAEParamMissed;
	err = DispatchURLToNewBrowser(url);
	PR_DELETE(url);
	return err;
} // nsAppleEventHandler::HandleGetURLEvent

/*##########################################################################*/	
/*			 		----   The Spyglass Suite   ----						*/
/*##########################################################################*/	

#define errMakeNewWindow 100

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleOpenURLEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc&				outResult,
	SInt32				/*inAENumber*/)
//	nsAppleEventHandler::HandleOpenURLEvent		Suite:Spyglass
//	Open to a URL
//	In: Incoming apple event, reply event, outtgoing descriptor, eventID
//
//		Flavors of OpenURL
//		#define AE_spy_openURL		'OURL'	// typeChar OpenURL
//			AE_spy_openURL_into			// typeFSS into
//			AE_spy_openURL_wind			// typeLongInteger windowID
//			AE_spy_openURL_flag			// typeLongInteger flags
//				4 bits used.
//				1 = 
//				2 = 
//				4 = 
//				8 = Open into editor window
//			AE_spy_openURL_post			// typeWildCard post data
//			AE_spy_openURL_mime			// typeChar MIME type
//			AE_spy_openURL_prog			// typePSN Progress app
//			
//	Just like with GetURL, figure out the window, and let it handle the load
//	Arguments we care about: AE_spy_openURL_wind
//	
//	Out: OpenURLEvent handled
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	char* url = nsnull;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
	if (!url)
		return errAEParamMissed;
	err = DispatchURLToNewBrowser(url);
	PR_DELETE(url);
	return err;
} // nsAppleEventHandler::HandleOpenURLEvent

/*##########################################################################*/	
/*				----   Experimental Netscape Suite    ----					*/
/*##########################################################################*/	

//----------------------------------------------------------------------------------------
struct nsJavascriptCallbacks : public nsIXULWindowCallbacks
//----------------------------------------------------------------------------------------
{
    nsJavascriptCallbacks(const char* scriptText)
    	: mText(scriptText)
    {
    		NS_INIT_REFCNT();
    }
    nsJavascriptCallbacks()
    {
    		NS_INIT_REFCNT();
    }
    virtual ~nsJavascriptCallbacks() {}

    NS_DECL_ISUPPORTS

    NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell)
    {
    	return NS_OK;
    }
    NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell)
    {
		// Get the webshell's script owner
		nsCOMPtr<nsIScriptContextOwner> scriptContextOwner(do_QueryInterface(aWebShell));
		if ( !scriptContextOwner )
			return errAEEventNotHandled;

		// Get the script owner's script context
		nsCOMPtr<nsIScriptContext> scriptContext;
		nsresult rv = scriptContextOwner->GetScriptContext(getter_AddRefs(scriptContext));
		if (NS_FAILED(rv))
			return errAEEventNotHandled;
		
		// Ask the script context to evalute the javascript string
		PRBool isUndefined = PR_FALSE;
		nsString rVal("xxx");
	    const char* url = "";
		scriptContext->EvaluateString(mText, url, 0, rVal, &isUndefined);
    	return NS_OK;
    }

private:
    nsString mText;
}; // nsJavascriptCallbacks

NS_IMPL_ISUPPORTS( nsJavascriptCallbacks, nsIXULWindowCallbacks::GetIID() );

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleDoJavascriptEvent(
	const AppleEvent&	inAppleEvent,
	AppleEvent& 		/*outAEReply*/,
	AEDesc&				/*outResult*/,
	SInt32				/*inAENumber*/)
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	// Get the direct object - the string to execute.
	char *scriptText = nsnull;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, scriptText);
	if (!scriptText)
		return errAEParamMissed;
	const char* kJavascriptPrefix = "javascript:";
	char* url = (char*)PR_MALLOC(PL_strlen(scriptText) + PL_strlen(kJavascriptPrefix) + 2);
	if (!url)
	{
		PR_FREEIF(scriptText);
		return memFullErr;
	}
	PL_strcpy(url, kJavascriptPrefix);
	PL_strcat(url, scriptText);
	PR_FREEIF(scriptText);
	err = DispatchURLToNewBrowser(url);
Clean:
	PR_FREEIF(url);
	return err;
#if 0
	// Find (or make) a webshell window. At the moment, we're using a new window, which
	// is why I'm using the callback mechanism to ensure that we don't try to
	// evaluate the javascript till after javascript is initialized.
//	nsCOMPtr<nsJavascriptCallbacks> cb(
//		nsDontQueryInterface<nsJavascriptCallbacks>(new nsJavascriptCallbacks(scriptText)));
	nsCOMPtr<nsIWebShellWindow> aWindow = getter_AddRefs(FindWebShellWindow(nsnull));
	if (!aWindow)
		return errAEEventNotHandled;
	
	PRBool usingExistingWindow = PR_FALSE; // For now.
	
	if (1)
	{
		// Get the window's webshell
		nsCOMPtr<nsIWebShell> webShell;
		nsresult rv = aWindow->GetWebShell(*getter_AddRefs(webShell));
		if (NS_FAILED(rv) || !webShell)
			return errAEEventNotHandled;
		
		// Get the webshell's script owner
		nsCOMPtr<nsIScriptContextOwner> scriptContextOwner(do_QueryInterface(webShell));
		if ( !scriptContextOwner )
			return errAEEventNotHandled;

		// Get the script owner's script context
		nsCOMPtr<nsIScriptContext> scriptContext;
		rv = scriptContextOwner->GetScriptContext(getter_AddRefs(scriptContext));
		if (NS_FAILED(rv))
			return errAEEventNotHandled;
		
		// Ask the script context to evalute the javascript string
		PRBool isUndefined = PR_FALSE;
		nsString rVal("xxx");
	    const char* url = "";
		scriptContext->EvaluateString(scriptText, url, 0, rVal, &isUndefined);
	}
	return noErr;
#endif // 0
}

/*##########################################################################*/	
/*				----   Apple Event Object Model support   ----				*/
/*##########################################################################*/	

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::GetSubModelByUniqueID(DescType inModelID, const AEDesc	&inKeyData, AEDesc &outToken) const
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	switch (inModelID)
	{
	case cWindow:		// The hyperwindows have unique IDs that can be resolved
						// FFFFFFFFF is the front window, 0 is a new window
		Int32 windowID;
		UExtractFromAEDesc::TheInt32(inKeyData, windowID);
		LWindow* foundWindow = CBrowserWindow::GetWindowByID(windowID);
		if (foundWindow == nsnull)
			ThrowOSErr_(errAENoSuchObject);
		else
		{
			NS_NOTYETIMPLEMENTED("Write Me");
//			CFrontApp::sApplication->PutInToken(foundWindow, outToken);
		}	
		break;
	default:
		NS_NOTYETIMPLEMENTED("Write Me");
//		CFrontApp::sApplication->LDocApplication::GetSubModelByUniqueID(inModelID, inKeyData, outToken);
		break;
	}
#endif // 0
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::GetAEProperty(DescType inProperty,
							const AEDesc	&inRequestedType,
							AEDesc			&outPropertyDesc) const
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	
	switch (inProperty)
	{
		case AE_www_typeApplicationAlert:	// application that handles alerts
			err = ::AECreateDesc(typeType, &ErrorManager::sAlertApp,
								sizeof(ErrorManager::sAlertApp), &outPropertyDesc);
			ThrowIfOSErr_(err);
			break;
			
		case AE_www_typeKioskMode:
			err = ::AECreateDesc(typeLongInteger, (const void *)mKioskMode,
								sizeof(mKioskMode), &outPropertyDesc);
			break;
			
		default:
			NS_NOTYETIMPLEMENTED("Write Me");
//			CFrontApp::sApplication->LApplication::GetAEProperty(inProperty, inRequestedType, outPropertyDesc);
			break;
	}
#endif // 0
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::SetAEProperty(DescType inProperty,
							const AEDesc	&inValue,
							AEDesc			&outPropertyDesc)
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	switch (inProperty)
	{
	case AE_www_typeApplicationAlert:	// application that handles alerts
		try
		{
			OSType newSig;
			UExtractFromAEDesc::TheType(inValue, newSig);
			ErrorManager::sAlertApp = newSig;
		}
		catch(...)	// In case of error, revert to self
		{
			ErrorManager::sAlertApp = emSignature;
		}
		break;
	case AE_www_typeKioskMode:
		try
		{
			Boolean menuBarModeChangeBroadcasted = false;
			Int32 kMode;
			UExtractFromAEDesc::TheInt32(inValue, kMode);
			
			if ((kMode == KioskOn) && (mKioskMode != KioskOn))
			{
				mKioskMode = KioskOn;
				// SetMenubar(KIOSK_MENUBAR);		еее this is currently handled by the chrome structure, below
				//										BUT, they want three states for menu bars, and the field is only a boolean
								
				CMediatedWindow*	theIterWindow = nsnull;
				DataIDT				windowType = WindowType_Browser;
				CWindowIterator		theWindowIterator(windowType);
				
				while (theWindowIterator.Next(theIterWindow))
				{
					CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
					if (theBrowserWindow != nil)
					{
						Chrome			aChrome;
						theBrowserWindow->GetChromeInfo(&aChrome);
						aChrome.show_button_bar = 0;
						aChrome.show_url_bar = 0;
						aChrome.show_directory_buttons = 0;
						aChrome.show_security_bar = 0;
						aChrome.show_menu = 0;		// еее this isn't designed correctly!	deeje 97-03-13
						
						// Make sure we only broadcast the menubar mode change once!
						theBrowserWindow->SetChromeInfo(&aChrome, !menuBarModeChangeBroadcasted);
						if (!menuBarModeChangeBroadcasted)
						{
							menuBarModeChangeBroadcasted = true;
						}
					}
				}
			}
			else if ((kMode == KioskOff) && (mKioskMode != KioskOff))
			{
				mKioskMode = KioskOff;
				// SetMenubar(MAIN_MENUBAR);		еее this is currently handled by the chrome structure, below
				//										BUT, they want three states for menu bars, and the field is only a boolean
								
				CMediatedWindow*	theIterWindow = nsnull;
				DataIDT				windowType = WindowType_Browser;
				CWindowIterator		theWindowIterator(windowType);
				
				while (theWindowIterator.Next(theIterWindow))
				{
					CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
					if (theBrowserWindow != nil)
					{
						Chrome			aChrome;
						theBrowserWindow->GetChromeInfo(&aChrome);
						aChrome.show_button_bar = CPrefs::GetBoolean(CPrefs::ShowToolbar);
						aChrome.show_url_bar = CPrefs::GetBoolean(CPrefs::ShowURL);
						aChrome.show_directory_buttons = CPrefs::GetBoolean(CPrefs::ShowDirectory);
						aChrome.show_security_bar = CPrefs::GetBoolean(CPrefs::ShowToolbar) || CPrefs::GetBoolean(CPrefs::ShowURL);
						aChrome.show_menu = 1;		// еее this isn't designed correctly!	deeje 97-03-13
						
						// Make sure we only broadcast the menubar mode change once!
						theBrowserWindow->SetChromeInfo(&aChrome, !menuBarModeChangeBroadcasted);
						if (!menuBarModeChangeBroadcasted)
						{
							menuBarModeChangeBroadcasted = true;
						}
					}
				}
			}
			
		}
		catch(...)
		{}
		break;
	default:
		NS_NOTYETIMPLEMENTED("Write Me");
//		CFrontApp::sApplication->LApplication::SetAEProperty(inProperty, inValue, outPropertyDesc);
		break;
	}
#endif // 0
	return err;
}

//----------------------------------------------------------------------------------------
// The return value has been AddRefed. Callers should release it.
nsIWebShellWindow* FindWebShellWindow(nsIXULWindowCallbacks* inCallbacks)
//----------------------------------------------------------------------------------------
{
	// Temporary - we'll create a brand new window EVERY time.
	nsresult rv;
	NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &rv);
	if (NS_FAILED(rv))
		return nsnull;

	const PRInt32 windowWidth  = 615;
	const PRInt32 windowHeight = 650;
	nsCOMPtr<nsIURI> urlObj;
    char * urlStr = "chrome://navigator/content";
#ifndef NECKO
	rv = NS_NewURL(getter_AddRefs(urlObj), urlStr);
#else
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return nsnull;

    nsIURI *uri = nsnull;
    rv = service->NewURI(urlStr, nsnull, &uri);
    if (NS_FAILED(rv)) return nsnull;

    rv = uri->QueryInterface(nsIURI::GetIID(), (void**)&urlObj);
    NS_RELEASE(uri);
#endif // NECKO
	if (NS_FAILED(rv))
		return nsnull;

	nsIWebShellWindow *aWindow = nsnull;			// we will return the window AddRefed
	rv = appShellService->CreateTopLevelWindow(
    	nsnull,
    	urlObj, // nsIURI* of chrome
    	PR_TRUE, PR_TRUE,
        NS_CHROME_ALL_CHROME,
        inCallbacks, // callbacks
        NS_SIZETOCONTENT, NS_SIZETOCONTENT,
    	&aWindow);
	if (NS_FAILED(rv))
		return nsnull;
	return aWindow;
}

//========================================================================================
//					Implementation of MoreExtractFromAEDesc
//	Apple event helpers -- extension of UExtractFromAEDesc.h
//	All the miscelaneous AppleEvent helper routines.
//========================================================================================

#pragma mark -

//----------------------------------------------------------------------------------------
void MoreExtractFromAEDesc::GetCString(const AppleEvent	&inAppleEvent, 
						AEKeyword keyword, char * & s, Boolean inThrowIfError)
//	Given an AppleEvent, locate a string given a keyword and
//	return the string
//	In:	Event to search
//		AEKeyword assocaated with the string
//		C string ptr
//	Out: Pointer to a newly created C string returned
//----------------------------------------------------------------------------------------
{
	s = nil;
	StAEDescriptor desc;
	OSErr err = ::AEGetParamDesc(&inAppleEvent,keyword,typeWildCard,&desc.mDesc);
	
	if (err) {
		if (inThrowIfError)
			ThrowIfOSErr_(err);
		else
			return;
	}
	TheCString(desc, s);
}

//----------------------------------------------------------------------------------------
void MoreExtractFromAEDesc::TheCString(const AEDesc &inDesc, char * & outPtr)
//	Given an AEDesc of type typeChar, return its string.
//	In:	AEDesc containing a string
//		C string ptr
//	Out: Pointer to a newly created C string returned
//----------------------------------------------------------------------------------------
{
	outPtr = nil;
	Handle	dataH;
	AEDesc	coerceDesc = {typeNull, nil};
	if (inDesc.descriptorType == typeChar)
	{
#if TARGET_CARBON
		DebugStr("\pAppleEvent support needs fixing under Carbon");
#else
		dataH = inDesc.dataHandle;		// Descriptor is the type we want#
#endif
	}
	else
	{
		// Try to coerce to the desired type
		if (AECoerceDesc(&inDesc, typeChar, &coerceDesc) == noErr)
		{
										// Coercion succeeded
#if TARGET_CARBON
			DebugStr("\pAppleEvent support needs fixing under Carbon");
#else
			dataH = coerceDesc.dataHandle;
#endif

		}
		else
			return;
	}
	
	Int32 strLength = GetHandleSize(dataH);
	outPtr = (char *)PR_MALLOC(strLength+1);	// +1 for nsnull ending
	if (!outPtr)
		return;
	// Terminate the string
	BlockMoveData(*dataH, outPtr, strLength);
	outPtr[strLength] = 0;
	
	if (coerceDesc.dataHandle != nil)
		AEDisposeDesc(&coerceDesc);
}

//----------------------------------------------------------------------------------------
void MoreExtractFromAEDesc::MakeErrorReturn(
	AppleEvent &event,
	ConstStr255Param errorString,
	OSErr errorCode)
//	Add an error string and error code to an AppleEvent.
//	Typically used when constructing the return event when an
//	error occured
//	In:	Apple Event to append to
//		Error string
//		Error code
//	Out: keyErrorNum and keyErrorSting AEDescs are added to the Event.
//----------------------------------------------------------------------------------------
{
	StAEDescriptor	errorNum(errorCode);
	StAEDescriptor	errorText((ConstStringPtr)errorString);
	// We can ignore the errors. If error occurs, it only means that the reply is not handled
	OSErr err = ::AEPutParamDesc(&event, keyErrorNumber, &errorNum.mDesc);
	err = ::AEPutParamDesc(&event, keyErrorString, &errorText.mDesc);	
}

//----------------------------------------------------------------------------------------
Boolean MoreExtractFromAEDesc::DisplayErrorReply(AppleEvent &reply)
//	Display an error dialog if the given AppleEvent contains
//	a keyErrorNumber.  a keyErrorString is optional and will be
//	displayed if present
//	In:	Apple Event
//	Out: Error dialog displayed if error data present.
//----------------------------------------------------------------------------------------
{
	DescType realType;
	Size actualSize;
	OSErr	errNumber;
	Str255 errorString;
	// First check for errors
	errNumber = AEUtilities::EventHasErrorReply(reply);
	if (errNumber == noErr)
		return false;
	
	// server returned an error, so get error string
	OSErr err = ::AEGetParamPtr(&reply, keyErrorString, typeChar, 
						&realType, &errorString[1], sizeof(errorString)-1, 
						&actualSize);
	if (err == noErr)
	{
		errorString[0] = actualSize > 255 ? 255 : actualSize;
		NS_NOTYETIMPLEMENTED("Write Me");
//		ErrorManager::ErrorNotify(errNumber, errorString);
	}
	else
	{
		NS_NOTYETIMPLEMENTED("Write Me");
//		ErrorManager::ErrorNotify(errNumber, (unsigned char *)*GetString(AE_ERR_RESID));
	}
	return TRUE;
}

//----------------------------------------------------------------------------------------
ProcessSerialNumber	MoreExtractFromAEDesc::ExtractAESender(const AppleEvent &inAppleEvent)
//	Return the process serial number of the sending process.
//	In:	Apple Event send by some app.
//	Out: ProcessSerialNumber of the sending app.
//----------------------------------------------------------------------------------------
{
	Size realSize;
	DescType realType;
	TargetID target;
	ProcessSerialNumber psn;

	OSErr err = AEGetAttributePtr(&inAppleEvent, keyAddressAttr, typeTargetID, &realType, 
									&target, sizeof(target), &realSize);
	ThrowIfOSErr_(err);
	err = ::GetProcessSerialNumberFromPortName(&target.name,&psn);
	ThrowIfOSErr_(err);
	return psn;
}

//----------------------------------------------------------------------------------------
void MoreExtractFromAEDesc::DispatchURLDirectly(const AppleEvent &inAppleEvent)
//----------------------------------------------------------------------------------------
{
	char *url = nsnull;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
	ThrowIfNil_(url);
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	URL_Struct * request = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	PR_DELETE(url);
	ThrowIfNil_(request);
	request->internal_url = TRUE; // for attachments in mailto: urls.
	CURLDispatcher::DispatchURL(request, nsnull);
#endif
}

//========================================================================================
//					Implementation of AEUtilities
//	Some more simple Apple Event utility routines.
//========================================================================================

#pragma mark -

//----------------------------------------------------------------------------------------
OSErr AEUtilities::CreateAppleEvent(OSType suite, OSType id, 
		AppleEvent &event, ProcessSerialNumber targetPSN)
//	CreateAppleEvent
//	Create a new Apple Event from scratch.
//	In:	Apple Event suite
//		Apple Event ID
//		Ptr to return Apple Event
//		ProcessSerialNumber of the target app to send event to.
//	Out:A new Apple Event is created.  More data may be added to
//		the event simply by calling AEPutParamDesc or AEPutParamPtr
//----------------------------------------------------------------------------------------
{
	AEAddressDesc progressApp;
	OSErr err = ::AECreateDesc(typeProcessSerialNumber, &targetPSN, 
							 sizeof(targetPSN), &progressApp);
	if (err)
		return err;
	err = ::AECreateAppleEvent(suite, id,
									&progressApp,
									kAutoGenerateReturnID,
									kAnyTransactionID,
									&event);
	AEDisposeDesc(&progressApp);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr AEUtilities::EventHasErrorReply(AppleEvent & reply)
//	Check to see if there is an error in the given AEvent.
//	We simply return an OSError equiv to the error value
//	in the event.  If none exists (or an error took place
//	during access) we return 0.
//	In:	Apple Event to test
//	Out:Error value returned
//----------------------------------------------------------------------------------------
{
	if (reply.descriptorType == typeNull)
		return noErr;
		
	SInt32	errNumber;
	Size realSize;
	DescType realType;
	OSErr err = ::AEGetParamPtr(&reply, keyErrorNumber, typeLongInteger, 
						&realType, &errNumber, sizeof(errNumber), 
						&realSize);
	if (err == noErr)
		return errNumber;
	else
		return noErr; 
}

//========================================================================================
//					InitializeMacCommandLine
//	The only external entry point to this file.
//========================================================================================

#pragma mark -

//----------------------------------------------------------------------------------------
void InitializeMacCommandLine(int& argc, char**& argv)
//----------------------------------------------------------------------------------------
{
    typedef char* charP;
    args = new charP[MAX_TOKENS];
    args[0] = nsnull;
    argc = 0;
    argv = args;
    
    // Set up AppleEvent handling.
    
    nsAppleEventHandler* handler = nsAppleEventHandler::Get();
    if (!handler)
    	return;
    	
    // Snarf all the odoc and pdoc apple-events.
    //
    // 1. If they are odoc for 'CMDL' documents, read them into the buffer ready for
    //    parsing (concatenating multiple files).
    //
    // 2. If they are any other kind of document, convert them into -url command-line
    //    parameters or -print parameters, with file URLs.
    
    EventRecord anEvent;
	for (int i = 1; i < 5; i++)
		::WaitNextEvent(0, &anEvent, 0, nsnull);
		
    while (::EventAvail(highLevelEventMask, &anEvent))
    {
    	::WaitNextEvent(highLevelEventMask, &anEvent, 0, nsnull);
		if (anEvent.what == kHighLevelEvent)
		    OSErr err = ::AEProcessAppleEvent(&anEvent);
    }

	// Now we've grabbed all the initial high-level events, and written them into
	// the command-line buffer as url commands, parse the buffer.
	
	handler->SetStartedUp(PR_TRUE);
	if (!argBuffer)
		return;
		
    // Release some unneeded memory
    char* oldBuffer = argBuffer;
    argBuffer = new char[PL_strlen(argBuffer) + 1];
    PL_strcpy(argBuffer, oldBuffer);
    delete [] oldBuffer;
    
    // Parse the buffer.
	int	rowNumber = 0;
	char* strtokFirstParam = argBuffer;
	while (argc < MAX_TOKENS)
	{
		// Get the next token.  Initialize strtok by passing the string the
		// first time.  Subsequently, pass nil.
		char* nextToken = strtok(strtokFirstParam, " \t\n\r");
		args[argc] = nextToken;
		if (!nextToken)
			break;
		// Loop
		argc++;
		strtokFirstParam = nsnull;
	}

    // Release the unneeded memory.
	if (argc < MAX_TOKENS)
	{
	    charP* oldArgs = args;
	    int arraySize = 1 + argc;
	    args = new charP[arraySize];
	    memcpy(args, oldArgs, arraySize * sizeof(charP));
	    delete [] oldArgs;
	    argv = args;
	}
} // InitializeMac
