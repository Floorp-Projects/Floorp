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

// Special stuff for the Macintosh implementation of command-line service.

#include "nsCommandLineServiceMac.h"

#include <AppleEvents.h>
#include "nsFileSpec.h"

//----------------------------------------------------------------------------------------
OSErr DoAEOpenOrPrintDoc(
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
	if (err) return err;
	
		// Loop through all items in the list
			// Extract descriptor for the document
			// Coerce descriptor data into a FSSpec
			// Tell Program object to open document
		
	for (int i = 1; i <= numDocs; i++)
	{
		AEKeyword	theKey;
		DescType	theType;
		FSSpec		theFileSpec;
		Size		theSize;
		err = ::AEGetNthPtr(&docList, i, typeFSS, &theKey, &theType,
							(Ptr) &theFileSpec, sizeof(FSSpec), &theSize);
		if (err) return err;
		
//		if (inAENumber == ae_OpenDoc)
//			OpenDocument(&theFileSpec);
//		else
//			PrintDocument(&theFileSpec);
	}
	::AEDisposeDesc(&docList);
	return err;
} // DoAEOpenOrPrintDoc

//----------------------------------------------------------------------------------------
void InitializeMacCommandLine(int& argc, char**& argv)
//----------------------------------------------------------------------------------------
{
#define MAX_BUF 512
#define MAX_TOKENS 20
    static char argBuffer[MAX_BUF];
    static char* args[MAX_TOKENS] = {0};

    argc = 0;
    argv = args;

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

    // Parse the buffer.
	int	rowNumber = 0;
	char* strtokFirstParam = argBuffer;
	while (argc < MAX_TOKENS)
	{
		// Get the next token.  Initialize strtok by passing the string the
		// first time.  Subsequently, pass nil.
		char* nextToken = strtok(strtokFirstParam, " \t\n\r");
		args[argc++] = nextToken;
		if (!nextToken)
			break;
	}
} // InitializeMac
