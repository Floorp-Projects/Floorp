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

// NSPR
#include "prmem.h"
#include "plstr.h"

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

// Global buffers for command-line arguments and parsing.
#define MAX_BUF 512
#define MAX_TOKENS 20
static char* argBuffer = nsnull; // the command line itself
static char** args = nsnull; // array of pointers into argBuffer

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

	OSErr				HandleOpenDoc(const FSSpec&		inFileSpec,
									OSType 				fileType);

	OSErr				HandlePrintDoc(const FSSpec&	inFileSpec,
									OSType 				fileType);

	PRBool				EnsureCommandLine();
	
	OSErr				AddToCommandLine(const char* inArgText);
	
	OSErr				AddToCommandLine(const char*	inOptionString,
									const FSSpec&		inFileSpec);

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
									
	OSErr 				HandleGetWDEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
									
	OSErr 				HandleShowFile(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr 				HandleParseAnchor(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
// Progress

	OSErr 				HandleCancelProgress(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
// Spy window events
	OSErr 				HandleSpyActivate(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr 				HandleSpyListWindows(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr 				HandleSpyGetWindowInfo(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr 				HandleWindowRegistration(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
// Netscape suite
	OSErr				HandleOpenBookmarksEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr				HandleReadHelpFileEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr 				HandleGoEvent( const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);
	OSErr				HandleOpenAddressBookEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);								

	OSErr				HandleOpenComponentEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);								

	OSErr				HandleCommandEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);								

	OSErr 				HandleGetActiveProfileEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);		

	OSErr 				HandleGetProfileImportDataEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									SInt32				inAENumber);		

	static pascal OSErr	AppleEventHandler(
								const AppleEvent*	inAppleEvent,
								AppleEvent*			outAEReply,
								SInt32				inRefCon);

protected:
	KioskEnum			mKioskMode;
	PRBool				mStartedUp;			
	static nsAppleEventHandler*	sAppleEventHandler;		// One and only instance of AEvents

}; // class nsAppleEventHandler

//========================================================================================
class EudoraSuite
//	Tools used to communicate with Eudora
//	The only real use these have is if we are operating in
//	Browser-only mode and the user wishes to use Eudora to
//	handle mail functions.
//========================================================================================
{
	// --------------------------------------------------------------
	//	Some Constants used by the Eudora Suite						
	// --------------------------------------------------------------

	enum
	{	attachDouble		= 0
	,	attachSingle		= 1
	,	attachBinHex		= 2
	,	attachUUencode		= 3
	};

	enum
	{	EU_Norm_Priority	= 0
	,	EU_High_Priority	= 60
	,	EU_Highest_Priority	= 1
	,	EU_Low_Priority		= 160
	,	EU_Lowest_Priority	= 200
	};
public:

	// --------------------------------------------------------------
	/*	This makes a Null AppleEvent descriptor.    
				 */
	// --------------------------------------------------------------
	static void MakeNullDesc(AEDesc *theDesc);

	// --------------------------------------------------------------
	/*	This makes a string AppleEvent descriptor.   
				 */
	// --------------------------------------------------------------
	static OSErr MakeStringDesc(Str255 theStr,AEDesc *theDesc);


	// --------------------------------------------------------------
	/*	This stuffs the required parameters into the AppleEvent. 
				 */
	// --------------------------------------------------------------


	static OSErr CreateObjSpecifier(AEKeyword theClass,AEDesc theContainer,
			AEKeyword theForm,AEDesc theData, Boolean disposeInputs,AEDesc *theSpec);

	// --------------------------------------------------------------
	/*	This creates an AEDesc for the current message.
		(The current message index = 1)     

		In:	Pointer to AEDesc to return
		Out: AEDesc constructed. */
	// --------------------------------------------------------------

	static OSErr MakeCurrentMsgSpec(AEDesc *theSpec);


	// --------------------------------------------------------------
	/*	Send a given Apple Event.  Special case for Eudora, should
		be rewritten, but it works for the moment.
		In:	AppleEvent
		Out: Event sent  */
	// --------------------------------------------------------------
	static OSErr SendEvent(AppleEvent *theEvent);

	// --------------------------------------------------------------
	/*	Create an Apple Event to be sent to Eudora
		In:	Event Class
			Event ID
			Ptr to Apple Event
		Out: Event constructed and returned.  */
	// --------------------------------------------------------------
	static OSErr MakeEvent(AEEventClass eventClass,AEEventID eventID,AppleEvent *theEvent);

	// --------------------------------------------------------------
	/*	This sets the data in a specified field. It operates on the frontmost message 
		in Eudora. It is the equivalent of sending the following AppleScript:
		set field "fieldname" of message 0 to "data"

		Examples for setting up a complete mail message:
			EudoraSuite::SendSetData("\pto",toRecipientPtr);
			EudoraSuite::SendSetData("\pcc",ccRecipientPtr);
			EudoraSuite::SendSetData("\pbcc",bccRecipientPtr);
			EudoraSuite::SendSetData("\psubject",subjectPtr);
			EudoraSuite::SendSetData("\p",bodyPtr);
			
		In:	Field to set the data in (Subject, Address, Content, etc)
			Pointer to text data.
			Size of pointer (allows us to work with XP_Ptrs.
		Out: Apple Event sent to Eudora, setting a given field.  */
	// --------------------------------------------------------------
	static OSErr SendSetData(Str31 theFieldName, Ptr thePtr, SInt32 thePtrSize);
	
	// --------------------------------------------------------------
	/*	Everything you need to tell Eudora to construct a new message
		and send it.
		In:	Pointer to the list of e mail addresses to send TO
			Pointer to the list of e mail addresses to send CC
			Pointer to the list of e mail addresses to send BCC
			Pointer to the Subject text
			Priority level of message.
			char* to the contents of the mail
			Pointer to an FSSpec (or null if none) for an enclosure.
		Out: Apple Events sent to Eudora telling it to construct the
			message and send it. */
	// --------------------------------------------------------------

	static OSErr  SendMessage(
		Ptr		toRecipientPtr, 
		Ptr		ccRecipientPtr,
		Ptr		bccRecipientPtr,
		Ptr		subjectPtr,
		char*	bodyPtr,
		SInt32	thePriority,
		FSSpec	*theEnclosurePtr);

	static OSErr Set_Eudora_Priority(SInt32 thePriority);

}; // class EudoraSuite

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

}; // EudoraSuite

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
		SInt32				inRefCon)
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
		case AE_OpenProfileManager:
												// Required Suite
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
			break;
			
		case ae_Quit:
			NS_NOTYETIMPLEMENTED("Write Me");
			break;
			
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
			
		case AE_CancelProgress:
			err = HandleCancelProgress( inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_FindURL:		// located elsewhere
			NS_NOTYETIMPLEMENTED("Write Me");
			break;

		case AE_ShowFile:
			err = HandleShowFile(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_ParseAnchor:
			err = HandleParseAnchor(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_SpyActivate:
			err = HandleSpyActivate(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_SpyListWindows:
			err = HandleSpyListWindows(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_GetWindowInfo:
			err = HandleSpyGetWindowInfo(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_RegisterWinClose:
		case AE_UnregisterWinClose:
			err = HandleWindowRegistration(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		// --- These Spyglass events are handled elswhere
		case AE_RegisterViewer:
			NS_NOTYETIMPLEMENTED("Write Me");
			//CPrefs::sMimeTypes.HandleRegisterViewerEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_UnregisterViewer:
			NS_NOTYETIMPLEMENTED("Write Me");
			//CPrefs::sMimeTypes.HandleUnregisterViewerEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_RegisterProtocol:
		case AE_UnregisterProtocol:
		case AE_RegisterURLEcho:
		case AE_UnregisterURLEcho:
			NS_NOTYETIMPLEMENTED("Write Me");
			//CNotifierRegistry::HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
			break;

			
		// ### Netscape Experimental Suite
		// -----------------------------------------------------------
		case AE_OpenBookmark:
			err = HandleOpenBookmarksEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_ReadHelpFile:
			err = HandleReadHelpFileEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_Go:
			err = HandleGoEvent (inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_GetWD:	// Return Working Directory
			err = HandleGetWDEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
		
		case AE_OpenProfileManager:
			NS_NOTYETIMPLEMENTED("Write Me");
			//CFrontApp::sApplication->ProperStartup( nsnull, FILE_TYPE_PROFILES );
			break;
				
		case AE_OpenAddressBook:
			err = HandleOpenAddressBookEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
		
		case AE_OpenComponent:
			err = HandleOpenComponentEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;

		case AE_HandleCommand:
			err = HandleCommandEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_GetActiveProfile:
			err = HandleGetActiveProfileEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_GetProfileImportData:
			err = HandleGetProfileImportDataEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		// ### ????  2014 looks like it's a core suite thing, but not defined in the aedt
		// -----------------------------------------------------------
/*		case 2014:		// Display has changed. Maybe in 2.0, should change the pictures
			{
				StAEDescriptor displayDesc;
				OSErr err = AEGetParamDesc(&inAppleEvent,kAEDisplayNotice,typeWildCard,&displayDesc.mDesc);
				if (err == noErr)
					SysBeep(1);
			}
			break;
*/		

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
			err = HandleOpenDoc(spec, fileType);
		else
			err = HandlePrintDoc(spec, fileType);
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
OSErr nsAppleEventHandler::HandleOpenDoc(const FSSpec& inFileSpec, OSType inFileType)
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
				char chars[1024];
				s.readline(chars, sizeof(chars));
				// Does the text in it have the right prefix?
				const char* kCommandLinePrefix = "ARGS:";
				if (PL_strstr(chars, kCommandLinePrefix) == chars)
				{
					return AddToCommandLine(chars + PL_strlen(kCommandLinePrefix));
					 // That's all we have to do.
				}
			}
		}
		// If it's not a command-line argument, and we are starting up the application,
		// add a command-line "-url" argument to the global list. This means that if
		// the app is opened with documents on the mac, they'll be handled the same
		// way as if they had been typed on the command line in Unix or DOS.
		return AddToCommandLine("-url", inFileSpec);
	}
	// Final case: we're not just starting up. How do we handle this?
	NS_NOTYETIMPLEMENTED("Write Me");
	return errAEEventNotHandled;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandlePrintDoc(const FSSpec& inFileSpec, OSType fileType)
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
	// We need to make sure
	
//	CFrontApp::sApplication->ProperStartup( nsnull, FILE_TYPE_GETURL );

	// Get the window, and let it handle the event
	StAEDescriptor	keyData;
	OSErr err = ::AEGetKeyDesc(&inAppleEvent, AE_www_typeWindow, typeWildCard,&keyData.mDesc);
	
	// TODO: handle dispatching to storage
	
//	if (CFrontApp::GetApplication()->HasProperlyStartedUp())
	if (1)
	{
		// A window was specified, open the url in that window
		if ((err == noErr) && (keyData.mDesc.descriptorType == typeObjectSpecifier))
		{
			NS_NOTYETIMPLEMENTED("Write Me");
#if 0
			StAEDescriptor	theToken;
			err = ::AEResolve(&keyData.mDesc, kAEIDoMinimum, &theToken.mDesc);
			ThrowIfOSErr_(err);
			LModelObject * model = CFrontApp::sApplication->GetModelFromToken(theToken.mDesc);
			ThrowIfNil_(model);
			
			CBrowserWindow* win = dynamic_cast<CBrowserWindow*>(model);
			ThrowIfNil_(win);
			
			CBrowserWindow::HandleGetURLEvent(inAppleEvent, outAEReply, outResult, win);		
#endif
		}
		// No window or file specified, we will open the url in the frontmost
		// browser window or a new browser window if there are no open browser
		// windows.
		// Or no browser window if one is not appropriate.  Good grief! jrm.
		else
		{
			NS_NOTYETIMPLEMENTED("Write Me");
#if 0
			try
			{
				// 97-09-18 pchen -- first check to see if this is a NetHelp URL, and dispatch
				// it directly
				// 97/10/24 jrm -- Why can't we always dispatch directly?  CURLDispatcher
				// already has the logic to decide what's right!  Trying this.
				MoreExtractFromAEDesc::DispatchURLDirectly(inAppleEvent);
			}
			catch (...)
			{
				// Get a window
				
				CBrowserWindow* win = CBrowserWindow::FindAndShow(true);
				ThrowIfNil_(win);
					
				// Let the window handle the event
				
				CBrowserWindow::HandleGetURLEvent(inAppleEvent, outAEReply, outResult, win);
			}
#endif
		}
	}
	else
	{
		NS_NOTYETIMPLEMENTED("Write Me");
//		CBrowserWindow::HandleGetURLEvent(inAppleEvent, outAEReply, outResult);		
	}
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
	// We need to make sure
//	CFrontApp::sApplication->ProperStartup( nsnull, FILE_TYPE_GETURL );
	
	// Get the Flags to see if we want an editor
//	try
	{
		SInt32 flags;
		DescType realType;
		Size actualSize;
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_flag, typeLongInteger,
						 &realType, &flags, sizeof(flags), &actualSize);
		ThrowIfOSErr_(err);
#ifdef EDITOR
		if (flags & 0x8)
		{
			// Get the url (in case we want an editor)
			char* url = nsnull;
			MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
			ThrowIfNil_(url);
			// MAJOR CRASHING BUG!!!  if building Akbar 3.0 we should NOT CALL MakeEditWindow!!!
			URL_Struct * request = NET_CreateURLStruct(url, NET_DONT_RELOAD);
			XP_FREE (url);
			ThrowIfNil_(request);
			CEditorWindow::MakeEditWindow( nsnull, request);
			return;
		}
#endif // EDITOR
	}
//	catch(...)
//	{
//	}

//	if (CFrontApp::GetApplication()->HasProperlyStartedUp())
	if (1)
	{
		// Get the window, and let it handle the event
//		try
		{
			Int32 windowID;
			Size realSize;
			OSType realType;
			
			OSErr err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_wind, typeLongInteger,
							 &realType, &windowID, sizeof(windowID), &realSize);
			ThrowIfOSErr_(err);
			
			NS_NOTYETIMPLEMENTED("Write Me");
//			// If 0 then create a new window,  
//			//	If FFFF set to front most window
//			CBrowserWindow* win = nsnull;
//			if (windowID == 0)
//			{
//				win = CURLDispatcher::CreateNewBrowserWindow();
//			}
//			else if (windowID == 0xFFFFFFFF)	// Frontmost  window
//				win = CBrowserWindow::FindAndShow();
//			else
//				win = CBrowserWindow::GetWindowByID(windowID);
//			
//			ThrowIfNil_(win);
//			CBrowserWindow::HandleOpenURLEvent(inAppleEvent, outAEReply, outResult, win);		
		}
//		catch(...)
//		{
//			// 97-09-18 pchen -- first check to see if this is a NetHelp URL, and dispatch
//			// it directly
//			// 97/10/24 jrm -- Why can't we always dispatch directly?  CURLDispatcher
//			// already has the logic to decide what's right!  Trying this.
//			MoreExtractFromAEDesc::DispatchURLDirectly(inAppleEvent);
//			return;
//		}			
	}
	else
	{
		NS_NOTYETIMPLEMENTED("Write Me");
//		CBrowserWindow::HandleOpenURLEvent(inAppleEvent, outAEReply, outResult);		
	}
	return err;
} // nsAppleEventHandler::HandleOpenURLEvent

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleShowFile(const AppleEvent	&inAppleEvent, 
		AppleEvent& /*outAEReply*/, AEDesc& /*outResult*/, SInt32 /*inAENumber*/)
//	ShowFile
//	Similar to odoc, except that it takes a few extra arguments
//	keyDirectObject	file spec
//	AE_spy_showFile_mime MIME type (optional)
//	AE_spy_showFile_win window ID (optional)
//	AE_spy_showFile_url url (optional)
//
//	ShowFile: Similar to OpenDocuments, except that it specifies the parent 
//		URL, and MIME type of the file
//		
//	ShowFile  
//		alias  					-- File to open
//		[MIME type  string]  	-- MIME type
//		[Window ID  integer]  	-- Window to open the file in
//		[URL  string]  			-- Use this as a base URL
//		
//	Result:   
//		integer  				-- Window ID of the loaded window. 
//			0 means ShowFile failed, 
//			FFFFFFF means that data was not appropriate type to display in the browser.
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	char * url = nsnull;
	char * mimeType = nsnull;
	FSSpec fileSpec;
	Boolean hasFileSpec = FALSE;
	Size	realSize;
	OSType	realType;
	
	// Get the file specs
	err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeFSS,
						&realType, &fileSpec, sizeof(fileSpec), &realSize);
	if (err)
		return err;

	// Get the window
//	try
	{
		Int32 windowID;
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_wind, typeLongInteger,
						 &realType, &windowID, sizeof(windowID), &realSize);
		NS_NOTYETIMPLEMENTED("Write Me");
		if (err)
		    return err;	
//		win = CBrowserWindow::GetWindowByID(windowID);
//		ThrowIfNil_(win);
	}
//	catch (...)
//	{
//		win = CBrowserWindow::FindAndShow();
//		ThrowIfNil_(win);
//	}
	
	// Get the url
//	try
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_showFile_url, url);
	}
//	catch(...)
//	{
//	}
		
	// Get the mime type
//	try
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_showFile_mime, mimeType);
	}
//	catch(...)
//	{
//	}
	
	NS_NOTYETIMPLEMENTED("Write Me");
//	CFrontApp::sApplication->OpenLocalURL(&fileSpec, win, mimeType);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleParseAnchor(const AppleEvent	
		&inAppleEvent, AppleEvent &outAEReply, AEDesc& /*outResult*/, SInt32 /*inAENumber*/)
//	ParseAnchor AppleEvent
//	Gets the URL, the relative url, and returns the full url of the relative
//
//	arse anchor: Resolves the relative URL
//	parse anchor  
//		string  				-- Main URL
//		relative to  string  	-- Relative URL
//	Result:   
//		string  				-- Parsed  URL
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	char * url = nil;
	char * relative = nil;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
	MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_parse_rel, relative);
	NS_NOTYETIMPLEMENTED("Write Me");
	char* parsed = nsnull;//= NET_MakeAbsoluteURL(url, relative);
//	ThrowIfNil_(parsed);
	
	err = ::AEPutParamPtr(&outAEReply, keyAEResult, typeChar, parsed, strlen(parsed));
	if (parsed)
		PR_DELETE(parsed);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleCancelProgress(const AppleEvent	&inAppleEvent,
									AppleEvent			& /* outAEReply */,
									AEDesc&				 /* outResult */,
									SInt32				/* inAENumber */)
//	Find the context with this transaction ID, and cancel it
//	In: Incoming apple event, reply event, outtgoing descriptor, eventID
//	Out: OpenURLEvent handled
//	
//	cancel progress: 
//		Interrupts the download of the document in the given window
//		
//	cancel progress  
//		integer  				-- progress ID, obtained from the progress app
//		[in window  integer] 	-- window ID of the progress to cancel
//----------------------------------------------------------------------------------------
{
	// Find the window with this transaction, and let it handle the event
	OSErr err = noErr;
//	try
	{
		Int32 transactionID;
		StAEDescriptor	transDesc;
		Boolean hasTransactionID = FALSE;
		Boolean hasWindowID = FALSE;
//		try
		{
			err = ::AEGetKeyDesc(&inAppleEvent, keyDirectObject, typeWildCard, &transDesc.mDesc);
			if (err)
			    return err;	
			UExtractFromAEDesc::TheInt32(transDesc.mDesc, transactionID);
			hasTransactionID = TRUE;
		}
//		catch(...)
//		{
//		}
//		try
		{
			err = ::AEGetKeyDesc(&inAppleEvent, AE_spy_CancelProgress_win, typeWildCard, &transDesc.mDesc);
			if (err)
			    return err;	
			UExtractFromAEDesc::TheInt32(transDesc.mDesc, transactionID);
			hasWindowID = TRUE;
		}
//		catch(...)
//		{
//		}
		
		NS_NOTYETIMPLEMENTED("Write Me");
#if 0
		CBrowserWindow* window;
		if (!hasTransactionID && !hasWindowID)	// No arguments, interrupt frontmost by default
		{
			window = CBrowserWindow::FindAndShow();
			if (window && window->GetWindowContext())
				NET_InterruptWindow(*(window->GetWindowContext()));
		}
		else
		{
			CMediatedWindow* theWindow;
			CWindowIterator iter(WindowType_Browser);
			while (iter.Next(theWindow))
			{
				CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theWindow);
				CNSContext* nsContext = theBrowserWindow->GetWindowContext();
				if (((nsContext->GetTransactionID() == transactionID) && hasTransactionID) 
					|| ((nsContext->GetContextUniqueID() == transactionID) && hasWindowID))
					NET_InterruptWindow(*nsContext);
			}
		}
#endif
	}
//	catch(...)
//	{
//	}
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleSpyActivate(const AppleEvent	&inAppleEvent, AppleEvent& /*outAEReply*/,
							AEDesc& /*outResult*/, SInt32 /*inAENumber*/)
//	webActivate: Makes Netscape the frontmost application, and selects a given 
//		window. This event is here for suite completeness/ cross-platform 
//		compatibility only, you should use standard AppleEvents instead.
//		
//	webActivate  
//		integer 			 -- window to bring to front
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	Size realSize;
	OSType realType;
	CBrowserWindow * win = nsnull;
	
	MakeFrontApplication();
	// Get the window, and select it
	try
	{
		Int32 windowID;
		
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_wind, typeLongInteger,
						 &realType, &windowID, sizeof(windowID), &realSize);
		if (err)
		    return err;	

		win = CBrowserWindow::GetWindowByID(windowID);
		if (win == nil)
			Throw_(errAENoSuchObject);
		UDesktop::SelectDeskWindow(win);
	}
	catch(...){}
#endif
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleSpyListWindows(
	const AppleEvent&	inAppleEvent,
	AppleEvent&			outAEReply,
	AEDesc&				outResult,
	SInt32				inAENumber)
//	list windows: Lists the IDs of all the hypertext windows
//	
//	list windows
//	Result:   
//		list  		-- List of unique IDs of all the hypertext windows
//----------------------------------------------------------------------------------------
{
#pragma unused(inAppleEvent, outResult, inAENumber) 

	NS_NOTYETIMPLEMENTED("Write Me");
	OSErr err = noErr;
	AEDescList windowList;
#if 0
	windowList.descriptorType = typeNull;
	
	try
	{
		// Create a descriptor list, and add a windowID for each browser window to the list

		CMediatedWindow* 	theWindow = nsnull;
		DataIDT				windowType = WindowType_Browser;
		CWindowIterator		iter(windowType);
		
		err = ::AECreateList(nil, 0, FALSE, &windowList);
		if (err)
		    goto Clean;	

		while (iter.Next(theWindow))
		{
			CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theWindow);
			ThrowIfNil_(theBrowserWindow);
			
			CBrowserContext* theBrowserContext = (CBrowserContext*)theBrowserWindow->GetWindowContext();
			ThrowIfNil_(theBrowserContext);
			
			StAEDescriptor	theIDDesc(theBrowserContext->GetContextUniqueID());
			err = ::AEPutDesc(&windowList, 0, &theIDDesc.mDesc);
		if (err)
		    goto Clean;	
		}
	}
	catch (...)
	{
		if (windowList.descriptorType != typeNull)
			::AEDisposeDesc(&windowList);
		throw;
	}

	err = ::AEPutParamDesc(&outAEReply, keyAEResult, &windowList);
#endif
Clean:
	::AEDisposeDesc(&windowList);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleSpyGetWindowInfo(const AppleEvent	&inAppleEvent, AppleEvent &outAEReply,
							AEDesc& /*outResult*/, SInt32 /*inAENumber*/)
//	Gets the window information, a list containing URL and title
//	## Another redundant event that has an equivalent event already 
//	specified by Apple
//
//	get window info: 
//	Returns the information about the window as a list. 
//	Currently the list contains the window title and the URL. 
//	## You can get the same information using standard Apple Event GetProperty.
//	
//	get window info  
//		integer  			-- window ID
//	Result:   
//		list				-- contains URL and title strings
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	AEDescList propertyList;
	propertyList.descriptorType = typeNull;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	Size realSize;
	OSType realType;
	CBrowserWindow * win = nsnull;			// Window
	
	try
	{
			// Get the window, and list its properties
		Int32 windowID;
		
		err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeLongInteger,
						 &realType, &windowID, sizeof(windowID), &realSize);
		if (err)
		    goto Clean;	
		win = CBrowserWindow::GetWindowByID(windowID);
		if (win == nil)
			Throw_(errAENoSuchObject);
		
			// Create the list
		err = ::AECreateList(nil, 0, FALSE, &propertyList);
		if (err)
		    goto Clean;	
		
			// Put the URL as the first item in the list
		cstring url = win->GetWindowContext()->GetCurrentURL();
		err = ::AEPutPtr(&propertyList, 1, typeChar, (char*)url, url.length());
		if (err)
		    goto Clean;	
		
			// Put the title as the second item in the list
		CStr255 ptitle;
		GetWTitle(win->GetMacPort(), ptitle);
		cstring ctitle((char*)ptitle);
		err = ::AEPutPtr(&propertyList, 2, typeChar, (char*)ctitle, ctitle.length());
		if (err)
		    goto Clean;	
		
			// Put the list as the reply
		err = ::AEPutParamDesc(&outAEReply, keyAEResult, &propertyList);
		::AEDisposeDesc(&propertyList);
	}
	catch(...)
	{
		throw;		
	}
#endif
Clean:
	if (propertyList.descriptorType != typeNull)
		::AEDisposeDesc(&propertyList);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleWindowRegistration(const AppleEvent	&inAppleEvent, AppleEvent &outAEReply,
							AEDesc &outResult, SInt32 inAENumber)
//	register window close: 
//	Netscape will notify registered application when this window closes
//	
//	register window close  
//		'sign'  		-- Application signature for window  
//		integer 		 -- window ID
//		
//	[Result:   boolean]  -- true if successful
//
//	unregister window close: 
//	Undo for register window close unregister window close  
//	
//		'sign'  		-- Application signature for window  
//		integer 		 -- window ID
//		
//	[Result:   boolean]  -- true if successful
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	Size realSize;
	OSType realType;
	CBrowserWindow * win = nsnull;

	// Get the window, and select it
	Int32 windowID;
	
	err = ::AEGetParamPtr(&inAppleEvent, AE_spy_registerWinClose_win, typeLongInteger,
					 &realType, &windowID, sizeof(windowID), &realSize);
	ThrowIfOSErr_(err);
	
	win = CBrowserWindow::GetWindowByID(windowID);
	if (win == nil)
		Throw_(errAENoSuchObject);
	win->HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
#endif
	return err;
}

/*##########################################################################*/	
/*				----   Experimental Netscape Suite    ----					*/
/*##########################################################################*/	

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleOpenBookmarksEvent(
	const AppleEvent&	inAppleEvent,
	AppleEvent& 		/*outAEReply*/,
	AEDesc&				/*outResult*/,
	SInt32				/*inAENumber*/)
//	Open bookmark: Reads in a bookmark file
//	
//	Open bookmark  
//		alias  -- If not available, reloads the current bookmark file
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	FSSpec fileSpec;
	Boolean hasFileSpec = FALSE;
	Size	realSize;
	OSType	realType;

	char * path = nsnull;
	
	// Get the file specs
	err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeFSS,
							&realType, &fileSpec, sizeof(fileSpec), &realSize);

	// If none, use the default file
	//		I hate this glue to uapp.  Will change as soon as all AE stuff
	//		is working.
	if (err != noErr)
		path = GetBookmarksPath( fileSpec, TRUE );
	else
		path = GetBookmarksPath( fileSpec, FALSE );
	
	ThrowIfNil_(path);
	
	DebugStr("\pNot implemented");
//¥¥¥	CBookmarksContext::Initialize();
//¥¥¥	BM_ReadBookmarksFromDisk( *CBookmarksContext::GetInstance(), path, nsnull );
#endif
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleOpenAddressBookEvent( 
									const AppleEvent	&/*inAppleEvent*/,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									SInt32				/*inAENumber*/)
//	Open AddressBook: Opens the address book
//	
//	Open AddressBook  
//----------------------------------------------------------------------------------------
{
#ifdef MOZ_MAIL_NEWS
	CAddressBookManager::ShowAddressBookWindow();
#endif // MOZ_MAIL_NEWS
	return noErr;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleOpenComponentEvent( 
									const AppleEvent	&inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									SInt32				/*inAENumber*/)
//	Open Component: Opens the component specified in the parameter
//	to the Apple Event.
//----------------------------------------------------------------------------------------
{	
	OSType		componentType;
	DescType	returnedType;
	Size		gotSize = 0;
	
	OSErr err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeEnumerated, &returnedType, &componentType,
			sizeof(OSType), &gotSize);

	if (err == noErr && gotSize == sizeof(OSType))
	{					  
		switch (componentType)
		{
			case AE_www_comp_navigator:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_BrowserWindow);
				break;

#ifdef MOZ_MAIL_NEWS
			case AE_www_comp_inbox:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_Inbox);
				break;
						
			case AE_www_comp_collabra:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_MailNewsFolderWindow);
				break;
#endif // MOZ_MAIL_NEWS
#ifdef EDITOR
			case AE_www_comp_composer:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_NewWindowEditor);
				break;
#endif // EDITOR
#if !defined(MOZ_LITE) && !defined(MOZ_MEDIUM)
			case AE_www_comp_conference:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_LaunchConference);
				break;
				
			case AE_www_comp_calendar:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_LaunchCalendar);
				break;
				
			case AE_www_comp_ibmHostOnDemand:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_Launch3270);
				break;
#endif
#ifdef MOZ_NETCAST
			case AE_www_comp_netcaster:
				NS_NOTYETIMPLEMENTED("Write Me");
//				CFrontApp::sApplication->ObeyCommand(cmd_LaunchNetcaster);
				break;
#endif // MOZ_NETCAST
			default:
				; //do nothing
		}
	}
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleCommandEvent( 
									const AppleEvent	& inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									SInt32				/*inAENumber*/)
//----------------------------------------------------------------------------------------
{	

	SInt32		commandID;
	DescType	returnedType;
	Size		gotSize = 0;
	
	OSErr err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeEnumerated, &returnedType,
			&commandID, sizeof(SInt32), &gotSize);

	if (err == noErr && gotSize == sizeof(SInt32))
	{					  
// Is this really only for mail?
#ifdef MOZ_MAIL_NEWS
	CFrontApp::sApplication->ObeyCommand(commandID);
#endif // MOZ_MAIL_NEWS

	}
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleReadHelpFileEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									SInt32				/*inAENumber*/)
//	Read help file: 
//	Reads in the help file (file should be in the help file format)
//	## Questions about the new help file system, ask montulli
//		
//	Read help file  
//		alias
//		[with index  string]  	-- Index to the help file. Defaults to  ÔDEFAULTÕ)
//		[search text  string]  	-- Optional text to search for
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	FSSpec fileSpec;
	char * map_file_url = nsnull;
	char * help_id = nsnull;
	char * search_text = nsnull;
	Size	realSize;
	OSType	realType;

//	CFrontApp::sApplication->ProperStartup(nsnull, FILE_TYPE_NONE);

	try
	{
		// Get the file specs
		err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeFSS,
								&realType, &fileSpec, sizeof(fileSpec), &realSize);
		ThrowIfOSErr_(err);
		
		map_file_url = CFileMgr::GetURLFromFileSpec( fileSpec );
		ThrowIfNil_(map_file_url);

		// Get the help text
		
		try
		{
			MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_www_ReadHelpFileID, help_id);
		}
		catch(...)
		{
		}
			
		// Get the search text (optional)
		try
		{
			MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_www_ReadHelpFileSearchText, search_text);
		}
		catch(...)
		{}
	}
	catch(...)
	{
		throw;
	}
	
		// Will using the bookmark context as a dummy work?
	DebugStr("\pNot implemented");
//¥¥¥	CBookmarksContext::Initialize();
//¥¥¥	NET_GetHTMLHelpFileFromMapFile(*CBookmarksContext::GetInstance(),
//							  map_file_url, help_id, search_text);
	
	FREEIF(map_file_url);
	FREEIF(help_id);
	FREEIF(search_text);
#endif
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleGoEvent( 
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&outResult,
	SInt32				inAENumber)
//	Go: navigate a window: back, forward, again(reload), home)
//	Go  
//		reference  		-- window
//		direction  		again/home/backward/forward
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	// We need to make sure
	
//	CFrontApp::sApplication->ProperStartup( nsnull, FILE_TYPE_GETURL );
	CBrowserWindow * win = nsnull;

	// Get the window.  If it is a Browser window then
	//	decode the AE and send a message.
	
	// The keyData.mDesc should be a ref to a browser window 
	// If we get it successfulle (ie the user included the "in window 1" 
	// then we can get a hold of the actual BrowserWindow object by using
	// AEResolve and GetModelFromToken somehow
	StAEDescriptor	keyData;
	err = ::AEGetKeyDesc(&inAppleEvent, AE_www_typeWindow, typeWildCard, &keyData.mDesc);
	if ((err == noErr) && (keyData.mDesc.descriptorType == typeObjectSpecifier))
	{
				NS_NOTYETIMPLEMENTED("Write Me");
#if 0
		StAEDescriptor	theToken;
		err = ::AEResolve(&keyData.mDesc, kAEIDoMinimum, &theToken.mDesc);
		ThrowIfOSErr_(err);
		LModelObject * model = CFrontApp::sApplication->GetModelFromToken(theToken.mDesc);
		ThrowIfNil_(model);
		model->HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
#endif
	// else the user did not provide an inside reference "inside window 1"
	// so send back an error	
	}
	else
	{
		
	}
#endif
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleGetWDEvent( 
	const AppleEvent	& /*inAppleEvent */,
	AppleEvent			& outAEReply,
	AEDesc				& /*outResult */,
	SInt32				/*inAENumber */)
//	Get workingURL: 
//		Get the path to the running application in URL format.  
//		This will allow a script to construct a relative URL
//		
//	Get workingURL
//	Result:   
//		'tTXT'  -- Will return text of the from ÒFILE://foo/applicationnameÓ
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	char		*pathBlock = nsnull;
	Size		pathLength;
	OSErr		err;

	// Grab the URL of the running application
	pathBlock = PathURLFromProcessSignature(emSignature, 'APPL');
	
	if (outAEReply.dataHandle != nsnull && pathBlock != nsnull)
	{
		pathLength = PL_strlen(pathBlock);
		
		err = AEPutParamPtr( &outAEReply, 
			keyDirectObject, 
			typeChar, 
			pathBlock, pathLength);
	}
	
	if (pathBlock)
		PR_DELETE(pathBlock);
#endif
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleGetActiveProfileEvent( 
	const AppleEvent	& /*inAppleEvent */,
	AppleEvent			&outAEReply,
	AEDesc				& /*outResult */,
	SInt32				/*inAENumber */)
//	HandleGetActiveProfileEvent: 
//		Get the name of the active profile from the prefs.
//	
//	Result:   
//		'tTXT'  -- Will return text of the profile name
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
	char 	profileName[255];
	int 	len = 255;
	OSErr	err = noErr;
	
	err =  PREF_GetCharPref("profile.name", profileName, &len);
	
	if (err == PREF_NOERROR && outAEReply.dataHandle != nil) {
	
			err = AEPutParamPtr( &outAEReply, 
				keyDirectObject, 
				typeChar, 
				profileName, strlen(profileName));
	}
#endif
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsAppleEventHandler::HandleGetProfileImportDataEvent( 
	const AppleEvent	& /*inAppleEvent */,
	AppleEvent			&outAEReply,
	AEDesc				& /*outResult */,
	SInt32				/*inAENumber */)
//
//		Returns useful data for the external import module.
//	
//	Result:   
//		'tTXT'  -- (Though binary data - see ProfileImportData struct below)
//----------------------------------------------------------------------------------------
{
	OSErr err = noErr;
	NS_NOTYETIMPLEMENTED("Write Me");
#if 0
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#else
#error "There'll be a big bug here"
#endif
	struct ProfileImportData
	{
		Int16		profileVRefNum;
		SInt32		profileDirID;
		Int16		mailFolderVRefNum;
		SInt32		mailFolderDirID;
		DataIDT		frontWindowKind;
	} data;
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif
	Assert_(16==sizeof(ProfileImportData));
	FSSpec spec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
	data.profileVRefNum = spec.vRefNum;
	data.profileDirID = spec.parID;
	spec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
	data.mailFolderVRefNum = spec.vRefNum;
	data.mailFolderDirID = spec.parID;
	CWindowIterator iter(WindowType_Any, false);
	CMediatedWindow* frontWindow = nil;
	data.frontWindowKind = iter.Next(frontWindow) ? frontWindow->GetWindowType() : 0;
	/* err =*/ AEPutParamPtr( &outAEReply, 
		keyDirectObject, 
		typeChar, 
		&data, sizeof(data));
#endif // 0
	return err;
} // nsAppleEventHandler::HandleGetProfileImportDataEvent

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
				// SetMenubar(KIOSK_MENUBAR);		¥¥¥ this is currently handled by the chrome structure, below
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
						aChrome.show_menu = 0;		// ¥¥¥ this isn't designed correctly!	deeje 97-03-13
						
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
				// SetMenubar(MAIN_MENUBAR);		¥¥¥ this is currently handled by the chrome structure, below
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
						aChrome.show_menu = 1;		// ¥¥¥ this isn't designed correctly!	deeje 97-03-13
						
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

//========================================================================================
//					Implementation of EudoraSuite
//========================================================================================

#pragma mark -

/*-------------------------------------------------------------*/
//	 class EudoraSuite		This supports sending to Eudora
//	Tools used to communicate with Eudora
//	The only real use these have is if we are operating in
//	Browser-only mode and the user wishes to use Eudora to
//	handle mail functions.
//
/*-------------------------------------------------------------*/

//----------------------------------------------------------------------------------------
void EudoraSuite::MakeNullDesc(AEDesc *theDesc)
//----------------------------------------------------------------------------------------
{
	theDesc->descriptorType = typeNull;
	theDesc->dataHandle = nil;
}

//----------------------------------------------------------------------------------------
OSErr EudoraSuite::MakeStringDesc(Str255 theStr,AEDesc *theDesc)
//	This makes a string AppleEvent descriptor.   
//	In: A pascal string
//		Pointer to an AEDesc.
//	Out: AEDesc of type TEXT created and returned
//----------------------------------------------------------------------------------------
{
	return AECreateDesc(kAETextSuite, &theStr[1], StrLength(theStr), theDesc);
}

//----------------------------------------------------------------------------------------
OSErr EudoraSuite::CreateObjSpecifier(AEKeyword theClass,AEDesc theContainer,
		AEKeyword theForm,AEDesc theData, Boolean /*disposeInputs*/,AEDesc *theSpec)
//This stuffs the required parameters into the AppleEvent
//----------------------------------------------------------------------------------------
{
	AEDesc theRec;
	OSErr	err;
	
	err = AECreateList(nil,0,true,&theRec);
	if (!err)
		err = AEPutKeyPtr(&theRec,keyAEKeyForm,typeEnumeration,&theForm,sizeof(theForm));
	if (!err)
		err = AEPutKeyPtr(&theRec,keyAEDesiredClass,cType,&theClass,sizeof(theClass));
	if (!err)
		err = AEPutKeyDesc(&theRec,keyAEKeyData,&theData);
	if (!err)
		err = AEPutKeyDesc(&theRec,keyAEContainer,&theContainer);
	if (!err)
		err = AECoerceDesc(&theRec,cObjectSpecifier,theSpec);
	AEDisposeDesc(&theRec);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr EudoraSuite::MakeCurrentMsgSpec(AEDesc *theSpec)
//	This creates an AEDesc for the current message.
//	(The current message index = 1)     
//
//	In:	Pointer to AEDesc to return
//	Out: AEDesc constructed.
//----------------------------------------------------------------------------------------
{
	AEDesc theContainer,theIndex;
	OSErr	err;
	SInt32	msgIndex = 1;

	EudoraSuite::MakeNullDesc (&theContainer);
	err = AECreateDesc(cLongInteger, &msgIndex, sizeof(msgIndex), &theIndex);
	if (!err)
		err = EudoraSuite::CreateObjSpecifier(cEuMessage, theContainer,
				formAbsolutePosition, theIndex, true, theSpec);
				
	AEDisposeDesc(&theContainer);
	AEDisposeDesc(&theIndex);
	
	return err;
}

//----------------------------------------------------------------------------------------
OSErr EudoraSuite::SendEvent(AppleEvent *theEvent)
//	Send a given Apple Event.  Special case for Eudora, should
//	be rewritten, but it works for the moment.
//	In:	AppleEvent
//	Out: Event sent
//----------------------------------------------------------------------------------------
{
	AppleEvent theReply;
	OSErr err;
	
	EudoraSuite::MakeNullDesc(&theReply);
	err = AESend(theEvent,&theReply,kAENoReply + kAENeverInteract 
			+ kAECanSwitchLayer+kAEDontRecord,kAENormalPriority,-1,nil,nil);

	AEDisposeDesc(&theReply);
	AEDisposeDesc(theEvent);

	return err;
}

//----------------------------------------------------------------------------------------
OSErr EudoraSuite::MakeEvent(AEEventClass eventClass,AEEventID eventID,AppleEvent *theEvent)
//	Create an Apple Event to be sent to Eudora
//	In:	Event Class
//		Event ID
//		Ptr to Apple Event
//	Out: Event constructed and returned.
//----------------------------------------------------------------------------------------
{
	AEAddressDesc theTarget;
	OSType theSignature;
	OSErr	err;
	
	theSignature = kEudoraSuite;
	err = AECreateDesc(typeApplSignature,&theSignature,sizeof(theSignature),&theTarget);
	if (!err)
		err = AECreateAppleEvent(eventClass,eventID,&theTarget,0,0,theEvent);
	AEDisposeDesc(&theTarget);
	return err;
}

/*	This sets the data in a specified field. It operates on the frontmost message 
	in Eudora. It is the equivalent of sending the following AppleScript:
	set field "fieldname" of message 0 to "data"

	Examples for setting up a complete mail message:
		EudoraSuite::SendSetData("\pto",toRecipientPtr);
		EudoraSuite::SendSetData("\pcc",ccRecipientPtr);
		EudoraSuite::SendSetData("\pbcc",bccRecipientPtr);
		EudoraSuite::SendSetData("\psubject",subjectPtr);
		EudoraSuite::SendSetData("\p",bodyPtr);
		
	In:	Field to set the data in (Subject, Address, Content, etc)
		Pointer to text data.
		Size of pointer (allows us to work with XP_Ptrs).
	Out: Apple Event sent to Eudora, setting a given field.  */
//----------------------------------------------------------------------------------------
OSErr EudoraSuite::SendSetData(Str31 theFieldName, Ptr thePtr, SInt32 thePtrSize)
//----------------------------------------------------------------------------------------
{
	AEDesc theMsg,theName,theFieldSpec,theText;
	AppleEvent theEvent;
	OSErr err;
	Handle theData;
	
	theData = NewHandle(thePtrSize);
	BlockMove((Ptr)thePtr,*theData,thePtrSize);

	if (theData != nil)
		{

			err = EudoraSuite::MakeCurrentMsgSpec(&theMsg);
			if (!err)
				err = EudoraSuite::MakeStringDesc(theFieldName,&theName);
			if (!err)
				err = EudoraSuite::CreateObjSpecifier(cEuField,theMsg,formName,theName,true,&theFieldSpec);
			if (!err)
				err = EudoraSuite::MakeEvent(kAECoreSuite,kAESetData,&theEvent);
			if (!err)
				err = AEPutParamDesc(&theEvent,keyAEResult,&theFieldSpec);
			AEDisposeDesc(&theFieldSpec);
				
			theText.descriptorType = typeChar;
			theText.dataHandle = theData;
			if (!err)
				err = AEPutParamDesc(&theEvent,keyAEData,&theText);
			if (!err)
				err = EudoraSuite::SendEvent(&theEvent);

			DisposeHandle(theText.dataHandle);
			AEDisposeDesc(&theText);
			AEDisposeDesc(&theMsg);
			AEDisposeDesc(&theName);
		}
	DisposeHandle(theData);
	return err;
	
}

//----------------------------------------------------------------------------------------
OSErr  EudoraSuite::SendMessage(
	Ptr		toRecipientPtr, 
	Ptr		ccRecipientPtr,
	Ptr		bccRecipientPtr,
	Ptr		subjectPtr,
	char*	bodyPtr,
	SInt32	thePriority,
	FSSpec	*theEnclosurePtr)
//	Everything you need to tell Eudora to construct a new message
//	and send it.
//	In:	Pointer to the list of e mail addresses to send TO
//		Pointer to the list of e mail addresses to send CC
//		Pointer to the list of e mail addresses to send BCC
//		Pointer to the Subject text
//		Priority level of message.
//		Pointer to the contents of the mail
//		Pointer to an FSSpec (or null if none) for an enclosure.
//	Out: Apple Events sent to Eudora telling it to construct the
//		message and send it.
//----------------------------------------------------------------------------------------
{	
	AEDesc nullSpec,theName,theFolder,theMailbox,theInsertRec,theInsl,msgSpec,theEnclList;
	OSType thePos,theClass;
	AppleEvent theEvent;
	OSErr 	err;
	
		
/* This section creates a new message and places it at the end of the out mailbox.
   It is equivalent to  the following AppleScript:
		make message at end of mailbox "out" of mail folder ""
*/


	MakeNullDesc(&nullSpec);
	err = MakeStringDesc("\p",&theName);
	if (!err)
		err = EudoraSuite::CreateObjSpecifier(cEuMailfolder,nullSpec,formName,theName,true,&theFolder);


	if (!err)
		err = MakeStringDesc("\pout",&theName);
	if (!err)
		err = EudoraSuite::CreateObjSpecifier(cEuMailbox,theFolder,formName,theName,true,&theMailbox);
	if (!err)
		err = AECreateList(nil,0,true,&theInsertRec);
	if (!err)
		err = AEPutKeyDesc(&theInsertRec,keyAEObject,&theMailbox);


	thePos=kAEEnd;
	if (!err)
		err = AEPutKeyPtr(&theInsertRec,keyAEPosition,typeEnumeration,&thePos,sizeof(thePos));

	if (!err)
		err = AECoerceDesc(&theInsertRec,typeInsertionLoc,&theInsl);

	if (!err)
		err = EudoraSuite::MakeEvent(kAECoreSuite,kAECreateElement,&theEvent);
	if (!err)
		err = AEPutParamDesc(&theEvent,keyAEInsertHere,&theInsl);

	AEDisposeDesc(&nullSpec);
	AEDisposeDesc(&theName);
	AEDisposeDesc(&theFolder);
	AEDisposeDesc(&theMailbox);
	AEDisposeDesc(&theInsertRec);
	AEDisposeDesc(&theInsl);

	theClass=cEuMessage;
	if (!err)
		err = AEPutParamPtr(&theEvent,keyAEObjectClass,cType,&theClass,sizeof(theClass));
	if (!err)
		err = EudoraSuite::SendEvent(&theEvent);


/* This section fills in various fields.
   It is equivalent to  the following AppleScript:
		set field "to" of message 0 to "data"
*/

	if (!err) {
		if ( toRecipientPtr )
		err = SendSetData("\pto",toRecipientPtr, GetPtrSize(toRecipientPtr));
	}
	
	if (!err) {
		if ( ccRecipientPtr )
		err = SendSetData("\pcc",ccRecipientPtr, GetPtrSize(ccRecipientPtr));
	}
	if (!err) {
		if ( bccRecipientPtr ) 
		err = SendSetData("\pbcc",bccRecipientPtr, GetPtrSize(bccRecipientPtr));
	}
	
	if (!err)
		err = SendSetData("\psubject",subjectPtr, GetPtrSize(subjectPtr));
	if (!err)
		err = SendSetData("\p",bodyPtr, PL_strlen(bodyPtr) );

/* This sets the priority of the message. See the constants defined above for the legal
   values.
*/
	
	err = Set_Eudora_Priority(thePriority);		
		

/* This attaches a file to the Eudora message provided it is a proper FSSpec. */
	if (StrLength(theEnclosurePtr->name)>0)
		{
			if (!err)
				err = MakeCurrentMsgSpec(&msgSpec);
			if (!err)
				err = EudoraSuite::MakeEvent(kEudoraSuite,kEuAttach,&theEvent);
			if (!err)
				err = AEPutParamDesc(&theEvent,keyAEResult,&msgSpec);
				if (!err)
					err = AECreateList(nil,0,false,&theEnclList);
				if (!err)
					err = AEPutPtr(&theEnclList,0,typeFSS,&theEnclosurePtr->name,
						sizeof(theEnclosurePtr->name) );
				if (!err)
					err = AEPutParamDesc(&theEvent,keyEuDocumentList,&theEnclList);
				if (!err)
					err = EudoraSuite::SendEvent(&theEvent);
				AEDisposeDesc(&msgSpec);
				AEDisposeDesc(&theEnclList);
			}


/* This tells Eudora to queue the current message. */
	if (!err)
		err = EudoraSuite::MakeCurrentMsgSpec(&msgSpec);
	if (!err)
		err = EudoraSuite::MakeEvent(kEudoraSuite,kEuQueue,&theEvent);
	if (!err)
		err = AEPutParamDesc(&theEvent,keyAEResult,&msgSpec);
	AEDisposeDesc(&msgSpec);
	if (!err)
		err = EudoraSuite::SendEvent(&theEvent);
		
	return err;

}

//----------------------------------------------------------------------------------------
OSErr EudoraSuite::Set_Eudora_Priority(SInt32 thePriority)
//	Given the priority of a message, this sets the priority in the message.
//	This same type of procedure can be used for many of the AppleScript commands in
//	the form of:     set <item> of message 0 to <data>
//----------------------------------------------------------------------------------------
{
  AEDesc theMsg,theData,thePropSpec,thePriorityDesc;
  AppleEvent theEvent;
  OSErr theErr;
  AEKeyword theProperty;
  Handle h;
 
  theErr = MakeCurrentMsgSpec(&theMsg);
 
  theProperty = pEuPriority;
  AECreateDesc(typeType,&theProperty,sizeof (theProperty),&thePriorityDesc);
 
  if (!theErr)
 	theErr = CreateObjSpecifier(typeProperty,theMsg,typeProperty,thePriorityDesc,true,&thePropSpec);
  if (!theErr)
    theErr = MakeEvent(kAECoreSuite,kAESetData,&theEvent);
 
  if (!theErr)
    theErr = AEPutKeyDesc(&theEvent, keyDirectObject, &thePropSpec);
  if (!theErr)
    theErr = AEDisposeDesc(&thePropSpec);
 
  h = NewHandle (sizeof(thePriority));
  BlockMove ((Ptr)&thePriority,*h,sizeof(thePriority));
  theData.descriptorType = typeInteger;
  theData.dataHandle = h;
  if (!theErr)
    theErr=AEPutParamDesc(&theEvent,keyAEData,&theData);
  if (!theErr)
    theErr = SendEvent(&theEvent);
	DisposeHandle(h);
    AEDisposeDesc(&theMsg);
    AEDisposeDesc(&theData);
    AEDisposeDesc(&thePriorityDesc);

   return theErr;
 
}

//========================================================================================
//					Implementation of EudoraSuite
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
	if (inDesc.descriptorType == typeChar) {
		dataH = inDesc.dataHandle;		// Descriptor is the type we want
	
	} else {							// Try to coerce to the desired type
		if (AECoerceDesc(&inDesc, typeChar, &coerceDesc) == noErr) {
										// Coercion succeeded
			dataH = coerceDesc.dataHandle;

		} else {						// Coercion failed
			ThrowOSErr_(errAETypeError);
		}
	}
	
	Int32	strLength = GetHandleSize(dataH);
	outPtr = (char *)PR_MALLOC(strLength+1);	// +1 for nsnull ending
	ThrowIfNil_( outPtr );
	// Terminate the string
	BlockMoveData(*dataH, outPtr, strLength);
	outPtr[strLength] = 0;
	
	if (coerceDesc.dataHandle != nil) {
		AEDisposeDesc(&coerceDesc);
	}
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
    while (::EventAvail(highLevelEventMask, &anEvent))
    {
    	::WaitNextEvent(highLevelEventMask, &anEvent, 0, 0);
		if (anEvent.what == kHighLevelEvent)
		    OSErr err = ::AEProcessAppleEvent(&anEvent);
    }

	handler->SetStartedUp(PR_TRUE);
	if (!argBuffer)
		return;
		
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
} // InitializeMac
