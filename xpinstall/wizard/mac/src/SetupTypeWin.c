/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "MacInstallWizard.h"


/*-----------------------------------------------------------*
 *   Setup Type Window
 *-----------------------------------------------------------*/

void 
ShowSetupTypeWin(void)
{
	Str255 				next;
	Str255 				back;
	MenuHandle 			popupMenu;
	PopupPrivateData ** pvtDataHdl;
	unsigned char *		currMenuItem;
	short 				i;
/*	
	short 				numVols;
	unsigned char **	volName;
*/
	Rect 				viewRect;
	long				txtSize;
	Str255				instLocTitle, selectFolder;
	GrafPtr				oldPort;
	GetPort(&oldPort);
	
	if (gWPtr != NULL)
	{
		SetPort(gWPtr);
	
		gCurrWin = kSetupTypeID; 
		/* gControls->stw = (SetupTypeWin *) NewPtrClear(sizeof(SetupTypeWin));	*/

		GetIndString(next, rStringList, sNextBtn);
		GetIndString(back, rStringList, sBackBtn);
	
		// malloc and get controls
		gControls->stw->instType = GetNewControl( rInstType, gWPtr);
		gControls->stw->instDescBox = GetNewControl( rInstDescBox, gWPtr);
		gControls->stw->destLocBox = GetNewControl( rDestLocBox, gWPtr);
		gControls->stw->destLoc = GetNewControl(rDestLoc, gWPtr);
		if (!gControls->stw->instType || !gControls->stw->instDescBox || 
			!gControls->stw->destLocBox || !gControls->stw->destLoc)
		{
			ErrorHandler();
			return;
		}

		// populate popup button menus
		HLock((Handle)gControls->stw->instType);
		pvtDataHdl = (PopupPrivateData **) (*(gControls->stw->instType))->contrlData;
		popupMenu = (MenuHandle) (**pvtDataHdl).mHandle;
		for (i=0; i<gControls->cfg->numSetupTypes; i++)
		{
			HLock(gControls->cfg->st[i].shortDesc);
			currMenuItem = CToPascal(*gControls->cfg->st[i].shortDesc);		
			HUnlock(gControls->cfg->st[i].shortDesc);
			InsertMenuItem( popupMenu, currMenuItem, i);
		}
		HUnlock((Handle)gControls->stw->instType);
		SetControlMaximum(gControls->stw->instType, gControls->cfg->numSetupTypes);
		SetControlValue(gControls->stw->instType, gControls->opt->instChoice);
		//Draw1Control(gControls->stw->instType);
	
		// setup type desc TE init and default item desc display
		HLockHi((Handle)gControls->stw->instDescBox);
		viewRect = (*(gControls->stw->instDescBox))->contrlRect;
		HUnlock((Handle)gControls->stw->instDescBox);	
		InsetRect(&viewRect, kTxtRectPad, kTxtRectPad);

		gControls->stw->instDescTxt = (TEHandle) NewPtrClear(sizeof(TEPtr));
		TextFont(systemFont);
		TextFace(normal);
		TextSize(12);	
		gControls->stw->instDescTxt = TENew( &viewRect, &viewRect);
		HLockHi(gControls->cfg->st[gControls->opt->instChoice - 1].longDesc);
		txtSize = strlen(*gControls->cfg->st[gControls->opt->instChoice - 1].longDesc);
		TEInsert( *gControls->cfg->st[gControls->opt->instChoice - 1].longDesc, txtSize, gControls->stw->instDescTxt);
		TESetAlignment( teFlushDefault, gControls->stw->instDescTxt);
		HUnlock(gControls->cfg->st[gControls->opt->instChoice - 1].longDesc);

/*	
	volName = (unsigned char **)NewPtrClear(sizeof(unsigned char *));
	GetAllVInfo(volName, &numVols);	
	gControls->stw->numVols = numVols;
	HLockHi((Handle)gControls->stw->destLoc);
	pvtDataHdl = (PopupPrivateData **) (*(gControls->stw->destLoc))->contrlData;
	popupMenu = (MenuHandle) (**pvtDataHdl).mHandle;
	for (i=0; i<numVols; i++)
	{
		InsertMenuItem( popupMenu, volName[i], i);
	}
	InsertMenuItem( popupMenu, "\p-", i);
	GetIndString(selectFolder, rStringList, sSelectFolder);
	InsertMenuItem( popupMenu, selectFolder, i+1);	
	HUnlock((Handle)gControls->stw->destLoc);
	
	SetControlMaximum(gControls->stw->destLoc, numVols+2); // 2 extra for divider and "Select Folder..." item
	SetControlValue(gControls->stw->destLoc, 1);
*/
		GetIndString(selectFolder, rStringList, sSelectFolder);
		SetControlTitle(gControls->stw->destLoc, selectFolder);
		GetIndString(instLocTitle, rStringList, sInstLocTitle);
		SetControlTitle(gControls->stw->destLocBox, instLocTitle);	
	
		// show controls
		ShowControl(gControls->stw->instType);
		ShowControl(gControls->stw->destLoc);
		ShowNavButtons( back, next );
	
		DrawDiskNFolder(gControls->opt->vRefNum, gControls->opt->folder);
	}
		
	SetPort(oldPort);
}

void
ShowSetupDescTxt(void)
{
	Rect r;
	
	if (gControls->stw->instDescTxt)
	{
		r = (**(gControls->stw->instDescTxt)).viewRect;
		TEUpdate( &r, gControls->stw->instDescTxt);
	}
	
	DrawDiskNFolder(gControls->opt->vRefNum, gControls->opt->folder);
}

pascal void
OurNavEventFunction(NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms,
					NavCallBackUserData callBackUD)
{
	WindowPtr  windowPtr;
                     
	windowPtr = (WindowPtr) callBackParms->eventData.eventDataParms.event->message;
	if (!windowPtr)
	{
		ErrorHandler();
		return;
	}
	
	switch(callBackSelector)
	{
		case kNavCBEvent:
			switch(callBackParms->eventData.eventDataParms.event->what)
			{
				case updateEvt:
					if(((WindowPeek) windowPtr)->windowKind != kDialogWindowKind)
						HandleUpdateEvt((EventRecord *) callBackParms->eventData.eventDataParms.event);
					break;
			}
			break;
	} 
}

void 
InSetupTypeContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 				localPt;
	Rect				r;
	ControlPartCode		part;	
	short				cntlVal;
	long 				len;
	ControlHandle 		currCntl;
	
	/* NavChooseFolder vars */
	NavReplyRecord		reply;	
	NavDialogOptions	dlgOpts;
	NavEventUPP			eventProc;
	AEDesc				resultDesc, initDesc;
	FSSpec				folderSpec, tmp;
	OSErr				err;

	GrafPtr				oldPort;
	GetPort(&oldPort);
	SetPort(wCurrPtr);
	
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	HLockHi((Handle)gControls->stw->instType);
	r = (**(gControls->stw->instType)).contrlRect;
	HUnlock((Handle)gControls->stw->instType);
	if (PtInRect(localPt, &r))
	{
		part = FindControl(localPt, gWPtr, &currCntl);
		part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
		
		gControls->opt->instChoice = GetControlValue(currCntl);
		
		r = (**(gControls->stw->instDescTxt)).viewRect;
		HLockHi(gControls->cfg->st[gControls->opt->instChoice-1].longDesc);
		len = strlen(*gControls->cfg->st[gControls->opt->instChoice-1].longDesc);
		TESetText( *gControls->cfg->st[gControls->opt->instChoice-1].longDesc, len, gControls->stw->instDescTxt);
		HUnlock(gControls->cfg->st[gControls->opt->instChoice-1].longDesc);
		EraseRect( &r );
		TEUpdate( &r, gControls->stw->instDescTxt);
		return;
	}
	
	HLockHi((Handle)gControls->stw->destLoc);
	r = (**(gControls->stw->destLoc)).contrlRect;
	HUnlock((Handle)gControls->stw->destLoc);
	if (PtInRect(localPt, &r))
	{
		part = FindControl(localPt, gWPtr, &currCntl);
		part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
		cntlVal = GetControlValue(currCntl);
		
		err = NavLoad();
		
		if (err==noErr) {
		
			err = NavGetDefaultDialogOptions(&dlgOpts);
			GetIndString( dlgOpts.message, rStringList, sFolderDlgMsg );
			eventProc = NewNavEventProc( (ProcPtr) OurNavEventFunction );
			ERR_CHECK(FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, NULL, &tmp));
			ERR_CHECK(AECreateDesc(typeFSS, (void*) &tmp, sizeof(FSSpec), &initDesc));
			err = NavChooseFolder( &initDesc, &reply, &dlgOpts, eventProc, NULL, NULL );
		
			AEDisposeDesc(&initDesc);
			DisposeRoutineDescriptor(eventProc);
		
			if((reply.validRecord) && (err == noErr))
			{
				if((err = AECoerceDesc(&(reply.selection),typeFSS,&resultDesc)) == noErr)
				{
					BlockMoveData(*resultDesc.dataHandle,&tmp,sizeof(FSSpec));
					/* forces name to get filled */
					FSMakeFSSpec(tmp.vRefNum, tmp.parID, tmp.name, &folderSpec); 
				
					/* NOTE: 
					** ----
					** gControls->opt->parID and gControls->opt->folder refer to the
					** same folder. The -dirID- is used by Install() when passing params to
					** the SDINST_DLL through its prescribed interface that just takes the
					** vRefNum and parID of the FSSpec that describes the folder.
					** Whilst, the -folder- string is used by DrawDiskNFolder() in repainting.
					*/
					pstrcpy(gControls->opt->folder, folderSpec.name);
					DrawDiskNFolder(folderSpec.vRefNum, folderSpec.name);
					gControls->opt->vRefNum = tmp.vRefNum;
					gControls->opt->dirID = tmp.parID;
				}
            
				AEDisposeDesc(&resultDesc);
				NavDisposeReply(&reply);
			}
		
			//err = NavUnload();
			//if (err!=noErr) SysBeep(10); // DEBUG
		}
		
		return;
	}
	
	HLock((Handle)gControls->backB);
	r = (**(gControls->backB)).contrlRect;
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
		{
			ClearDiskSpaceMsgs();
			KillControls(gWPtr);
			ShowWelcomeWin();
			return;
		}
	}
			
	HLock((Handle)gControls->nextB);			
	r = (**(gControls->nextB)).contrlRect;
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{
			ClearDiskSpaceMsgs();
			KillControls(gWPtr);
			/* treat last setup type selection as custom */
			if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
				ShowComponentsWin();
			else
				ShowTerminalWin();
			return;
		}
	}
	SetPort(oldPort);
}

void
DrawDiskNFolder(short vRefNum, unsigned char *folder)
{
	Str255			inFolderMsg, onDiskMsg, volName;
	char			*cstr;
	Rect			viewRect, dlb;
	TEHandle		pathInfo;
	short			bCmp;
	OSErr			err = noErr;
	
	/* get vol and folder name */
	if ((err = GetVol(volName, &vRefNum)) == noErr)
	{				
		dlb = (*gControls->stw->destLocBox)->contrlRect;
		SetRect(&viewRect, dlb.left+10, dlb.top+15, dlb.left+220, dlb.bottom-5);
		
		/* format and draw vol and disk name strings */
		TextFace(normal);
		TextSize(9);
		TextFont(applFont);
		EraseRect(&viewRect);
		pathInfo = TENew(&viewRect, &viewRect);
	
		if ( (bCmp = pstrcmp(folder, volName)) == 0)
		{
				cstr = "\t\t\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
			GetIndString( inFolderMsg, rStringList, sInFolder);
			cstr = PascalToC(inFolderMsg);
			TEInsert(cstr, strlen(cstr), pathInfo); 
			DisposePtr(cstr);	
				cstr = "\r\"\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
	
			cstr = PascalToC(folder);
			TEInsert(cstr, strlen(cstr), pathInfo);
			DisposePtr(cstr);
				cstr = "\"\r\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
		}
		
			cstr = "\t\t\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
		GetIndString( onDiskMsg,   rStringList, sOnDisk);
		cstr = PascalToC(onDiskMsg);
		TEInsert(cstr, strlen(cstr), pathInfo);
		DisposePtr(cstr);
			cstr = "\r\"\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
			
		cstr = PascalToC(volName);
		TEInsert(cstr, strlen(cstr), pathInfo);
		DisposePtr(cstr);
			cstr = "\"\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
			
		TEUpdate(&viewRect, pathInfo);
		
		TextFont(systemFont);
		TextSize(12);
	}
	
	/* free mem blocks */
	TEDispose(pathInfo);

	DrawDiskSpaceMsgs(vRefNum);
}

void
DrawDiskSpaceMsgs(short vRefNum)
{
	HVolumeParam	pb;
	OSErr			err;
	long			freeSpace;
	TEHandle		diskSpaceMsgH;
	Rect			instDescBox, viewRect;
	Handle			instDescRectH;
	Str255			msg;
	Str15			kb;
	char 			*cmsg, *ckb, *cfreeSpace, *cSpaceNeeded;
	
	pb.ioCompletion = NULL;
	pb.ioVolIndex = 0;
	pb.ioNamePtr = NULL;
	pb.ioVRefNum = vRefNum;
	
	ERR_CHECK( PBHGetVInfoSync((HParmBlkPtr)&pb) );
	freeSpace = pb.ioVFrBlk * pb.ioVAlBlkSiz;	// in bytes
	freeSpace /= 1024;							// translate to kilobytes

	instDescRectH = Get1Resource('RECT', rCompListBox);
	HLockHi(instDescRectH);
	instDescBox = (Rect) **((Rect**)instDescRectH);
	SetRect( &viewRect, instDescBox.left, instDescBox.bottom + 2, 
						instDescBox.right, instDescBox.bottom + 12 );
	HUnlock(instDescRectH);	
	DetachResource(instDescRectH);
	DisposeHandle(instDescRectH); 
							
	TextFace(normal);
	TextSize(9);
	TextFont(applFont);
	EraseRect(&viewRect);	
	diskSpaceMsgH = TENew(&viewRect, &viewRect);
	
	/* Get the "Disk Space Available: " string */
	GetIndString( msg, rStringList, sDiskSpcAvail );
	cmsg = PascalToC(msg);
	
	/* tack on the actual disk space in KB */
	cfreeSpace = ltoa(freeSpace);
	strcat( cmsg, cfreeSpace );
	
	/* tack on the "kilobyte" string: generally "K" but is rsrc'd */
	GetIndString( kb, rStringList, sKilobytes );
	ckb = PascalToC(kb);
	strcat( cmsg, ckb );
	
	/* draw the disk space available string */
	TEInsert( cmsg, strlen(cmsg), diskSpaceMsgH );
	TEUpdate( &viewRect, diskSpaceMsgH );
	
	/* recycle msg pointer and handle */
	TEDispose(diskSpaceMsgH);
	DisposePtr((char*)cmsg);
	
	SetRect( &viewRect, instDescBox.right - 150, instDescBox.bottom + 2,
						instDescBox.right, instDescBox.bottom + 12 );
	diskSpaceMsgH = TENew( &viewRect, &viewRect );
	
	/* Get the "Disk Space Needed: " string */
	GetIndString( msg, rStringList, sDiskSpcNeeded );
	cmsg = PascalToC(msg);
	
	/* tack on space needed in KB */
	cSpaceNeeded = DiskSpaceNeeded();
	strcat( cmsg, cSpaceNeeded );
	
	/* tack on the "kilobyte" string: generally "K" but is rsrc'd */
	GetIndString( kb, rStringList, sKilobytes );
	ckb = PascalToC(kb);
	strcat( cmsg, ckb );
	
	/* draw the disk space available string */
	TEInsert( cmsg, strlen(cmsg), diskSpaceMsgH );
	TEUpdate( &viewRect, diskSpaceMsgH );
	
	DisposePtr((char*)ckb);
	free(cSpaceNeeded);		// malloc'd, not NewPtrClear'd
	free(cfreeSpace);		// malloc'd, not NewPtrClear'd
	DisposePtr((char*)cmsg);
	TEDispose(diskSpaceMsgH);
	TextFont(systemFont);
	TextSize(12);
}

char *
DiskSpaceNeeded(void)
{
	char *cSpaceNeeded;
	short i;
	long spaceNeeded = 0;
	
	/* loop through all components cause they may be scattered through 
	 * the array for this particular setup type 
	 */
	for (i=0; i<kMaxComponents; i++)
	{	
		if (gControls->opt->compSelected[i] == kSelected)
		{
			spaceNeeded += gControls->cfg->comp[i].size;
		}
	}
	
	cSpaceNeeded = ltoa(spaceNeeded);
	
	return cSpaceNeeded;
}

void
ClearDiskSpaceMsgs(void)
{
	Rect instDescBox, viewRect;
	Handle instDescRectH;
	
	instDescRectH = Get1Resource('RECT', rCompListBox);
	HLockHi(instDescRectH);
	instDescBox = (Rect) **((Rect**)instDescRectH);
	SetRect( &viewRect, instDescBox.left, instDescBox.bottom + 2, 
						instDescBox.right, instDescBox.bottom + 12 );
	HUnlock(instDescRectH);	
	DetachResource(instDescRectH);
	DisposeHandle(instDescRectH);
						
	EraseRect( &viewRect );
	InvalRect( &viewRect );
}

/*
** ltoa -- long to ascii
**
** Converts a long to a C string. We allocate 
** a string of the appropriate size and the caller
** should assume ownership of the returned pointer.
*/
char *
ltoa(long n)
{
	char *s;
	int i, j, sign, tmp;
	
	/* check sign and convert to positive to stringify numbers */
	if ( (sign = n) < 0)
		n = -n;
	i = 0;
	s = (char*) malloc(sizeof(char));
	
	/* grow string as needed to add numbers from powers of 10 down till none left */
	do
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = n % 10 + '0';  /* '0' or 30 is where ASCII numbers start from */
		s[i] = '\0';
	}
	while( (n /= 10) > 0);	
	
	/* tack on minus sign if we found earlier that this was negative */
	if (sign < 0)
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = '-';
	}
	s[i] = '\0';
	
	/* pop numbers (and sign) off of string to push back into right direction */
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}
	
	return s;
}
	
short
pstrcmp(unsigned char* s1, unsigned char* s2)
{
	long len;
	register short i;
	
	if ( *s1 != *s2)  /* different lengths */
		return false;
	
	len = *s1;
	for (i=0; i<=len; i++)
	{
		s1++;
		s2++;
		if (*s1 != *s2)
			return false;
	}
	
	return true;
}

unsigned char*
pstrcpy(unsigned char* dest, unsigned char* src)
{
	long len;
	register short i;
	unsigned char* origdest;
	
	origdest = dest;
	len = *src;
	for (i=0; i<=len; i++)
	{
		*dest = *src;
		dest++;
		src++;
	}
		
	return origdest;
}
	
void
GetAllVInfo( unsigned char **volName, short *count)
{
	QHdrPtr				vcbQ;
	VCB *				currVCB;
	register short		i;
	
	vcbQ = GetVCBQHdr();
	currVCB = (VCB *)vcbQ->qHead;
	i = 0;
	while(1)
	{
		volName[i] = currVCB->vcbVN;
		/* *vRefNum[i] = currVCB->vcbVRefNum; */
		
		i++;  // since i gets incremented pre-break, count is accurate
		if (currVCB == (VCB *) vcbQ->qTail)
			break;
		currVCB = (VCB *)currVCB->qLink;
	}
	*count = i;
}

void
EnableSetupTypeWin(void)
{
	EnableNavButtons();

	// TO DO
}

void
DisableSetupTypeWin(void)
{
	DisableNavButtons();
	
	// TO DO
}