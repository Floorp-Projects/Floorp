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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
 
#ifndef _NS_FILESELECTOR_H_
	#include "nsFileSelector.h"
#endif

#include "nsAppleSingleDecoder.h"

nsFileSelector::nsFileSelector()
{
	mFile = NULL;
}

nsFileSelector::~nsFileSelector()
{

}

pascal void
OurNavEventFunction(NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms,
					NavCallBackUserData callBackUD)
{
	WindowPtr  windowPtr;
                     
	windowPtr = (WindowPtr) callBackParms->eventData.eventDataParms.event->message;
	if (!windowPtr)
		return;
		
	switch(callBackSelector)
	{
		case kNavCBEvent:
			switch(callBackParms->eventData.eventDataParms.event->what)
			{
				case updateEvt:
					if(((WindowPeek) windowPtr)->windowKind != kDialogWindowKind)
						// XXX irrelevant
						// HandleUpdateEvt((EventRecord *) callBackParms->eventData.eventDataParms.event); 
					break;
			}
			break;
	} 
}

OSErr
nsFileSelector::SelectFile(FSSpecPtr aOutFile)
{
	OSErr	err = noErr;
	NavReplyRecord		reply;	
	NavDialogOptions	dlgOpts;
	NavEventUPP			eventProc;
	AEDesc				resultDesc, initDesc;
	FSSpec				tmp;
	short				cwdVRefNum;
	long				cwdDirID, len;
	
	mFile = aOutFile;
	
	err = NavGetDefaultDialogOptions(&dlgOpts);
	len = strlen("Please select a file");
	nsAppleSingleDecoder::PLstrncpy(dlgOpts.message, "\pPlease select a file", len);
	eventProc = NewNavEventProc( (ProcPtr) OurNavEventFunction );
	
	ERR_CHECK( GetCWD(&cwdDirID, &cwdVRefNum) );
	ERR_CHECK( FSMakeFSSpec(cwdVRefNum, cwdDirID, NULL, &tmp) );
	ERR_CHECK( AECreateDesc(typeFSS, (void*) &tmp, sizeof(FSSpec), &initDesc) );
	
	err = NavChooseFile( &initDesc, &reply, &dlgOpts, eventProc, NULL, NULL, NULL, NULL );	
		
	AEDisposeDesc(&initDesc);
	DisposeRoutineDescriptor(eventProc);
		
	if((reply.validRecord) && (err == noErr))
	{
		if((err = AECoerceDesc(&(reply.selection),typeFSS,&resultDesc)) == noErr)
		{
			BlockMoveData(*resultDesc.dataHandle,&tmp,sizeof(FSSpec));
			/* forces name to get filled */
			FSMakeFSSpec(tmp.vRefNum, tmp.parID, tmp.name, aOutFile); 
		}
            
		AEDisposeDesc(&resultDesc);
		NavDisposeReply(&reply);
	}
	
	return err;
}

OSErr
nsFileSelector::SelectFolder(FSSpecPtr aOutFolder)
{
	OSErr	err = noErr;
	NavReplyRecord		reply;	
	NavDialogOptions	dlgOpts;
	NavEventUPP			eventProc;
	AEDesc				resultDesc, initDesc;
	FSSpec				tmp;
	short				cwdVRefNum;
	long				cwdDirID, len;
	
	mFile = aOutFolder;
	
	err = NavGetDefaultDialogOptions(&dlgOpts);
	len = strlen("Please select a folder");
	nsAppleSingleDecoder::PLstrncpy(dlgOpts.message, "\pPlease select a folder", len);
	eventProc = NewNavEventProc( (ProcPtr) OurNavEventFunction );
	
	ERR_CHECK( GetCWD(&cwdDirID, &cwdVRefNum) );
	ERR_CHECK( FSMakeFSSpec(cwdVRefNum, cwdDirID, NULL, &tmp) );
	ERR_CHECK( AECreateDesc(typeFSS, (void*) &tmp, sizeof(FSSpec), &initDesc) );
	
	err = NavChooseFolder( &initDesc, &reply, &dlgOpts, eventProc, NULL, NULL );
		
	AEDisposeDesc(&initDesc);
	DisposeRoutineDescriptor(eventProc);
		
	if((reply.validRecord) && (err == noErr))
	{
		if((err = AECoerceDesc(&(reply.selection),typeFSS,&resultDesc)) == noErr)
		{
			BlockMoveData(*resultDesc.dataHandle,&tmp,sizeof(FSSpec));
			/* forces name to get filled */
			FSMakeFSSpec(tmp.vRefNum, tmp.parID, tmp.name, aOutFolder); 
		}
            
		AEDisposeDesc(&resultDesc);
		NavDisposeReply(&reply);
	}
	
	return err;
}

OSErr
nsFileSelector::GetCWD(long *aOutDirID, short *aOutVRefNum)
{
	OSErr 				err = noErr;
	ProcessSerialNumber	psn;
	ProcessInfoRec		pInfo;
	FSSpec				tmp;
	
	/* get cwd based on curr ps info */
	if (!(err = GetCurrentProcess(&psn))) 
	{
		pInfo.processName = nil;
		pInfo.processAppSpec = &tmp;
		pInfo.processInfoLength = (sizeof(ProcessInfoRec));
		
		if(!(err = GetProcessInformation(&psn, &pInfo)))
		{
			*aOutDirID = pInfo.processAppSpec->parID;
			*aOutVRefNum = pInfo.processAppSpec->vRefNum;
		}
	}
	
	return err;
}
