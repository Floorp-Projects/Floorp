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


#include <Math64.h>


/*-----------------------------------------------------------*
 *   Setup Type Window
 *-----------------------------------------------------------*/

static long sDSNeededK = 0;  /* disk space needed in KB */
static long sDSAvailK = 0;   /* disk space available in KB */

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

		GetResourcedString(next, rInstList, sNextBtn);
		GetResourcedString(back, rInstList, sBackBtn);
	
		// malloc and get controls
		gControls->stw->instType = GetNewControl( rInstType, gWPtr);
		gControls->stw->instDescBox = GetNewControl( rInstDescBox, gWPtr);
		gControls->stw->destLocBox = GetNewControl( rDestLocBox, gWPtr);
		gControls->stw->destLoc = GetNewControl(rDestLoc, gWPtr);
		if (!gControls->stw->instType || !gControls->stw->instDescBox || 
			!gControls->stw->destLocBox || !gControls->stw->destLoc)
		{
			ErrorHandler(eMem, nil);
			return;
		}

		// populate popup button menus
		HLock((Handle)gControls->stw->instType);
		pvtDataHdl = (PopupPrivateData **) (*(gControls->stw->instType))->contrlData;
		HLock((Handle)pvtDataHdl);
		popupMenu = (MenuHandle) (**pvtDataHdl).mHandle;
		for (i=0; i<gControls->cfg->numSetupTypes; i++)
		{
			HLock(gControls->cfg->st[i].shortDesc);
			currMenuItem = CToPascal(*gControls->cfg->st[i].shortDesc);		
			HUnlock(gControls->cfg->st[i].shortDesc);
			InsertMenuItem( popupMenu, currMenuItem, i);
		}
		HUnlock((Handle)pvtDataHdl);
		HUnlock((Handle)gControls->stw->instType);
		SetControlMaximum(gControls->stw->instType, gControls->cfg->numSetupTypes);
		SetControlValue(gControls->stw->instType, gControls->opt->instChoice);
	
		// setup type desc TE init and default item desc display
		HLock((Handle)gControls->stw->instDescBox);
		SetRect(&viewRect,  (*(gControls->stw->instDescBox))->contrlRect.left,
							(*(gControls->stw->instDescBox))->contrlRect.top,
							(*(gControls->stw->instDescBox))->contrlRect.right,
							(*(gControls->stw->instDescBox))->contrlRect.bottom);
		HUnlock((Handle)gControls->stw->instDescBox);	
		InsetRect(&viewRect, kTxtRectPad, kTxtRectPad);

		TextFont(systemFont);
		TextFace(normal);
		TextSize(12);	
		gControls->stw->instDescTxt = TENew( &viewRect, &viewRect);
		HLock(gControls->cfg->st[gControls->opt->instChoice - 1].longDesc);
		txtSize = strlen(*gControls->cfg->st[gControls->opt->instChoice - 1].longDesc);
		TEInsert( *gControls->cfg->st[gControls->opt->instChoice - 1].longDesc, txtSize, gControls->stw->instDescTxt);
		TESetAlignment( teFlushDefault, gControls->stw->instDescTxt);
		HUnlock(gControls->cfg->st[gControls->opt->instChoice - 1].longDesc);

/*
    volName = (unsigned char **)NewPtrClear(sizeof(unsigned char *));
	GetAllVInfo(volName, &numVols);	
	gControls->stw->numVols = numVols;
	HLock((Handle)gControls->stw->destLoc);
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
		GetResourcedString(selectFolder, rInstList, sSelectFolder);
		SetControlTitle(gControls->stw->destLoc, selectFolder);
		GetResourcedString(instLocTitle, rInstList, sInstLocTitle);
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
	Rect teRect; /* TextEdit rect */
	
	if (gControls->stw->instDescTxt)
	{
		teRect = (**(gControls->stw->instDescTxt)).viewRect;
		TEUpdate(&teRect, gControls->stw->instDescTxt);
	}
	
	DrawDiskNFolder(gControls->opt->vRefNum, gControls->opt->folder);
}

pascal void
OurNavEventFunction(NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms,
					NavCallBackUserData callBackUD)
{
	WindowPtr  windowPtr;
  
  if (!callBackParms || !callBackParms->eventData.eventDataParms.event)
  	return;

	windowPtr = (WindowPtr)callBackParms->eventData.eventDataParms.event->message;
	if (!windowPtr)
		return;
		
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

static Boolean bFirstFolderSelection = true;

void 
InSetupTypeContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 				localPt;
	Rect				r;
	ControlPartCode		part;	
	short				cntlVal;
	long 				len;
	ControlHandle 		currCntl;
	int					instChoice;
	
	/* NavChooseFolder vars */
	NavReplyRecord		reply;	
	NavDialogOptions	dlgOpts;
	NavEventUPP			eventProc;
	AEDesc				resultDesc, initDesc;
	FSSpec				folderSpec, tmp;
	OSErr				err;
	long				realDirID;

	GrafPtr				oldPort;
	GetPort(&oldPort);
	SetPort(wCurrPtr);
	
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	HLock((Handle)gControls->stw->instType);
	SetRect(&r, (**(gControls->stw->instType)).contrlRect.left,
				(**(gControls->stw->instType)).contrlRect.top,
				(**(gControls->stw->instType)).contrlRect.right,
				(**(gControls->stw->instType)).contrlRect.bottom);
	HUnlock((Handle)gControls->stw->instType);
	if (PtInRect(localPt, &r))
	{
		part = FindControl(localPt, gWPtr, &currCntl);
		part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
		
		gControls->opt->instChoice = GetControlValue(currCntl);
		instChoice = gControls->opt->instChoice - 1;
		
		SetRect(&r, (**(gControls->stw->instDescTxt)).viewRect.left,
					(**(gControls->stw->instDescTxt)).viewRect.top,
					(**(gControls->stw->instDescTxt)).viewRect.right,
					(**(gControls->stw->instDescTxt)).viewRect.bottom);
					
		HLock(gControls->cfg->st[instChoice].longDesc);
		len = strlen(*gControls->cfg->st[instChoice].longDesc);
		TESetText( *gControls->cfg->st[instChoice].longDesc, len, gControls->stw->instDescTxt);
		HUnlock(gControls->cfg->st[instChoice].longDesc);

		EraseRect( &r );
		TEUpdate( &r, gControls->stw->instDescTxt);
		
		ClearDiskSpaceMsgs();
		DrawDiskSpaceMsgs(gControls->opt->vRefNum);
		return;
	}
	
	HLockHi((Handle)gControls->stw->destLoc);
	SetRect(&r, (**(gControls->stw->destLoc)).contrlRect.left,
				(**(gControls->stw->destLoc)).contrlRect.top,
				(**(gControls->stw->destLoc)).contrlRect.right,
				(**(gControls->stw->destLoc)).contrlRect.bottom);
	HUnlock((Handle)gControls->stw->destLoc);
	if (PtInRect(localPt, &r))
	{
		part = FindControl(localPt, gWPtr, &currCntl);
		part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
		cntlVal = GetControlValue(currCntl);
		
		err = NavGetDefaultDialogOptions(&dlgOpts);
		GetResourcedString( dlgOpts.message, rInstList, sFolderDlgMsg );
		eventProc = NewNavEventUPP( OurNavEventFunction );
		
		if (!bFirstFolderSelection)
			GetParentID(gControls->opt->vRefNum, gControls->opt->dirID, "\p", &realDirID);
		else
		{
			realDirID = gControls->opt->dirID;
			bFirstFolderSelection = false;
		}
		FSMakeFSSpec(gControls->opt->vRefNum, realDirID, "\p", &tmp);
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
				gControls->opt->vRefNum = tmp.vRefNum;
				gControls->opt->dirID = tmp.parID;
				DrawDiskNFolder(folderSpec.vRefNum, folderSpec.name);
			}
            
			AEDisposeDesc(&resultDesc);
			NavDisposeReply(&reply);
		}
		
		return;
	}
			
	HLock((Handle)gControls->nextB);			
	r = (**(gControls->nextB)).contrlRect;
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{
			/* check if folder location contains legacy apps */
			if (gControls->cfg->numLegacyChecks > 0)
				if (LegacyFileCheck(gControls->opt->vRefNum, gControls->opt->dirID))
				{
					/* user cancelled so don't do anything */
					return;
				}
			
			/* if not custom setup type then perform disk space check */
			if (gControls->opt->instChoice < gControls->cfg->numSetupTypes)
			{
    			if (!VerifyDiskSpace())
    			    return;
            }
            			    			
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
InsertCompList(int instChoice)
{
	int compsDone, i, len;
	InstComp currComp;
	char compName[128];
	Boolean didOneComp = false;
	
	// if not cutsom setup type show components list
	if (gControls->opt->instChoice < gControls->cfg->numSetupTypes)
	{
		compsDone = 0;
		TEInsert("\r", 1, gControls->stw->instDescTxt);
		for(i=0; i<kMaxComponents; i++)
		{
			if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
				 (!gControls->cfg->comp[i].invisible) &&
				 (compsDone < gControls->cfg->st[instChoice].numComps) )
			{
				currComp = gControls->cfg->comp[i];
				HLock(currComp.shortDesc);
				len = strlen(*currComp.shortDesc) + 4;
				memset(compName, 0, 128);
				if (didOneComp)
				    sprintf(compName, ", %s", *currComp.shortDesc);
				else
				{
				    sprintf(compName, "%s", *currComp.shortDesc);
				    didOneComp = true;
				}
				TEInsert(compName, len, gControls->stw->instDescTxt);
				HUnlock(currComp.shortDesc);
			}
			compsDone++;
		}
	}
}

void
DrawDiskNFolder(short vRefNum, unsigned char *folder)
{
	Str255			inFolderMsg, onDiskMsg, volName;
	char			*cstr;
	Rect			viewRect, dlb;
	TEHandle		pathInfo;
	short			bCmp, outVRefNum;
	OSErr			err = noErr;
	FSSpec			fsTarget;
	IconRef			icon;
	SInt16			label;
	unsigned long   free, total;
	
#define ICON_DIM 32

    dlb = (*gControls->stw->destLocBox)->contrlRect;
    SetRect(&viewRect, dlb.left+10, dlb.top+15, dlb.left+10+ICON_DIM, dlb.top+15+ICON_DIM);
   
	/* draw folder/volume icon */
	FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, "\p", &fsTarget);
	err = GetIconRefFromFile(&fsTarget, &icon, &label);
	if (err==noErr)
	{
		EraseRect(&viewRect);
		PlotIconRef(&viewRect, kAlignNone, kTransformNone, kIconServicesNormalUsageFlag, icon);
	}
	ReleaseIconRef(icon);	
	
	/* set up rect for disk and folder strings */
    SetRect(&viewRect, dlb.left+10+ICON_DIM+12, dlb.top+15, dlb.left+220, dlb.bottom-5);
    
	/* get vol and folder name */
	if ((err = HGetVInfo(vRefNum, volName, &outVRefNum, &free, &total)) == noErr)
	{	
		/* format and draw vol and disk name strings */
		TextFace(normal);
		TextSize(9);
		TextFont(applFont);
		EraseRect(&viewRect);
		pathInfo = TENew(&viewRect, &viewRect);
	
		if ( (bCmp = pstrcmp(folder, volName)) == 0)
		{
			GetResourcedString( inFolderMsg, rInstList, sInFolder);
			cstr = PascalToC(inFolderMsg);
			TEInsert(cstr, strlen(cstr), pathInfo); 
			DisposePtr(cstr);	
				cstr = "\r\"\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
	
			cstr = PascalToC(folder);
			TEInsert(cstr, strlen(cstr), pathInfo);
			DisposePtr(cstr);
				cstr = "\"\r\0"; 	TEInsert(cstr, strlen(cstr), pathInfo);
		}
		
		GetResourcedString( onDiskMsg,   rInstList, sOnDisk);
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
	XVolumeParam	pb;
	OSErr			err, reserr;
	short			msglen = 0;
	TEHandle		dsAvailH, dsNeededH;
	Rect			instDescBox, viewRect;
	Handle			instDescRectH;
	Str255			msg;
	Str15			kb;
	char 			*cstr, *cmsg, *ckb, *cfreeSpace, *cSpaceNeeded;
	
	pb.ioCompletion = NULL;
	pb.ioVolIndex = 0;
	pb.ioNamePtr = NULL;
	pb.ioVRefNum = vRefNum;
	
	ERR_CHECK( PBXGetVolInfoSync(&pb) );
	sDSAvailK = U32SetU(U64Divide(pb.ioVFreeBytes, U64SetU(1024L), nil));
    
	instDescRectH = NULL;
	instDescRectH = Get1Resource('RECT', rCompListBox);
	reserr = ResError();
	if (reserr!=noErr || !instDescRectH)
	{
		ErrorHandler(reserr, nil);
		return;
	}
	
	HLock(instDescRectH);
	instDescBox = (Rect) **((Rect**)instDescRectH);
	SetRect( &viewRect, instDescBox.left, instDescBox.bottom + 2, 
						instDescBox.right, instDescBox.bottom + 14 );
	HUnlock(instDescRectH);	
	DetachResource(instDescRectH);
	DisposeHandle(instDescRectH); 
							
	TextFace(normal);
	TextSize(9);
	TextFont(applFont);
	EraseRect(&viewRect);	
	dsAvailH = NULL;
	dsAvailH = TENew(&viewRect, &viewRect);
	if (!dsAvailH)
	{
		ErrorHandler(eMem, nil);
		return;
	}
	
	/* Get the "Disk Space Available: " string */
	GetResourcedString( msg, rInstList, sDiskSpcAvail );
	cstr = PascalToC(msg);
	msglen = strlen(cstr);
	cmsg = (char*)malloc(msglen+255);
	strncpy(cmsg, cstr, msglen);
	cmsg[msglen] = '\0';
	
	/* tack on the actual disk space in KB */
	cfreeSpace = ltoa(sDSAvailK);
	msglen += strlen(cfreeSpace);
	strcat( cmsg, cfreeSpace );
	cmsg[msglen] = '\0';
	
	/* tack on the "kilobyte" string: generally "K" but is rsrc'd */
	GetResourcedString( kb, rInstList, sKilobytes );
	ckb = PascalToC(kb);
	msglen += strlen(ckb);
	strcat( cmsg, ckb );
	cmsg[msglen] = '\0';
	
	/* draw the disk space available string */
	TEInsert( cmsg, strlen(cmsg), dsAvailH );
	TEUpdate( &viewRect, dsAvailH );
	
	/* recycle pointers */
	if (cstr)
		DisposePtr((Ptr)cstr);
	if (cmsg)
		free(cmsg);
	if (ckb)
		DisposePtr((Ptr)ckb);
	
	SetRect( &viewRect, instDescBox.right - 150, instDescBox.bottom + 2,
						instDescBox.right, instDescBox.bottom + 14 );
	dsNeededH = NULL;
	dsNeededH = TENew( &viewRect, &viewRect );
	if (!dsNeededH)
	{
		ErrorHandler(eMem, nil);
		return;
	}
	
	/* Get the "Disk Space Needed: " string */
	GetResourcedString( msg, rInstList, sDiskSpcNeeded );
	cstr = PascalToC(msg);
	msglen = strlen(cstr);
	cmsg = (char*)malloc(msglen+255);
	strncpy(cmsg, cstr, msglen);
	cmsg[msglen] = '\0';
	
	/* tack on space needed in KB */
	cSpaceNeeded = DiskSpaceNeeded();
	msglen += strlen(cSpaceNeeded);
	strcat( cmsg, cSpaceNeeded );
	cmsg[msglen] = '\0';
	
	/* tack on the "kilobyte" string: generally "K" but is rsrc'd */
	GetResourcedString( kb, rInstList, sKilobytes );
	ckb = PascalToC(kb);
	msglen += strlen(ckb);
	strcat( cmsg, ckb );
	cmsg[msglen] = '\0';
	
	/* draw the disk space available string */
	TEInsert( cmsg, strlen(cmsg), dsNeededH );
	TEUpdate( &viewRect, dsNeededH );
	
	if (dsAvailH)
		TEDispose(dsAvailH);
	if (dsNeededH)
		TEDispose(dsNeededH);
	
	if (ckb)
		DisposePtr((Ptr)ckb);
	if (cSpaceNeeded)
		free(cSpaceNeeded);		// malloc'd, not NewPtrClear'd
	if (cfreeSpace)
		free(cfreeSpace);
	if (cstr)
		DisposePtr((Ptr)cstr);
	if (cmsg)
		free(cmsg);
	TextFont(systemFont);
	TextSize(12);
}

char *
DiskSpaceNeeded(void)
{
	char *cSpaceNeeded;
	short i;
	long spaceNeeded = 0;
	int instChoice = gControls->opt->instChoice - 1;
	
	/* loop through all components cause they may be scattered through 
	 * the array for this particular setup type 
	 */
	for (i=0; i<kMaxComponents; i++)
	{	
		/* if "Custom Install" and it's selected... */
		if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
		{
			if ((gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
				(gControls->cfg->comp[i].selected == true))
				spaceNeeded += gControls->cfg->comp[i].size;
		}
		
		/* or not custom install but in current setup type... */
		else if (gControls->cfg->st[instChoice].comp[i] == kInSetupType)
		{
			spaceNeeded += gControls->cfg->comp[i].size;
		}
	}
	
	cSpaceNeeded = ltoa(spaceNeeded);
	sDSNeededK = spaceNeeded;
	
	return cSpaceNeeded;
}

void
ClearDiskSpaceMsgs(void)
{
	Rect instDescBox, viewRect;
	Handle instDescRectH;
	OSErr	reserr;
	GrafPtr	oldPort;
	
	GetPort(&oldPort);
	if (gWPtr)
		SetPort(gWPtr);
	
	instDescRectH = NULL;
	instDescRectH = Get1Resource('RECT', rCompListBox);
	reserr = ResError();
	if (reserr!=noErr || !instDescRectH)
	{
		ErrorHandler(reserr, nil);
		return;
	}

	HLock(instDescRectH);
	instDescBox = (Rect) **((Rect**)instDescRectH);
	SetRect( &viewRect, instDescBox.left, instDescBox.top, 
						instDescBox.right, instDescBox.bottom + 14 );
	HUnlock(instDescRectH);	
	DetachResource(instDescRectH);
	DisposeHandle(instDescRectH);
						
	EraseRect( &viewRect );
	InvalRect( &viewRect );
	
	SetPort(oldPort);
}

/*
** ltoa -- long to ascii
**
** Converts a long to a C string. We allocate 
** a string of the appropriate size and the caller
** should assume ownership of the returned pointer.
*/
#define kMaxLongLen	12
char *
ltoa(long n)
{
	char	s[kMaxLongLen] = "";
	char *returnBuf;
	int i, j, sign;
	
	/* check sign and convert to positive to stringify numbers */
	if ( (sign = n) < 0)
		n = -n;
	i = 0;
	
	/* grow string as needed to add numbers from powers of 10 down till none left */
	do
	{
		s[i++] = n % 10 + '0';  /* '0' or 30 is where ASCII numbers start from */
	}
	while( (n /= 10) > 0);	
	
	/* tack on minus sign if we found earlier that this was negative */
	if (sign < 0)
	{
		s[i++] = '-';
	}

	s[i] = '\0';
	
	/* pop numbers (and sign) off of string to push back into right direction */
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		char tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}

	returnBuf = (char *)malloc(strlen(s) + 1);
	strcpy(returnBuf, s);
	return returnBuf;
}

short
pstrcmp(unsigned char* s1, unsigned char* s2)
{
	long len;
	register short i;
	
	if ( *s1 != *s2)  /* different lengths */
		return false;
	
	len = *s1;
	for (i=0; i<len; i++)
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
	
	if (!dest || !src)
		return nil;
	
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
	
unsigned char*
pstrcat(unsigned char* dst, unsigned char* src)
{
	unsigned char 	*origdst;
	long			dlen, slen;
	register short	i;
	
	if (!dst || !src)
		return nil;
		
	origdst = dst;
	dlen = *dst;
	slen = *src;
	*dst = dlen+slen; 
	
	for (i=1; i<=slen; i++)
	{
		*(dst+dlen+i) = *(src+i);
	}
	
	return origdst;
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

Boolean
LegacyFileCheck(short vRefNum, long dirID)
{
	Boolean 	bRetry = false;
	int			i, diffLevel;
	StringPtr	pFilepath = 0, pMessage = 0, pSubfolder = 0;
	FSSpec		legacy, fsDest;
	OSErr		err = noErr;
	short		dlgRV = 0;
	char		cFilepath[1024];
	AlertStdAlertParamRec *alertdlg;
	
	for (i = 0; i < gControls->cfg->numLegacyChecks; i++)
	{
		/* construct legacy files' FSSpecs in program dir */
		HLock(gControls->cfg->checks[i].filename);
		if (!**gControls->cfg->checks[i].filename)
		{
			HUnlock(gControls->cfg->checks[i].filename);
			continue;
		}
		HLock(gControls->cfg->checks[i].subfolder);
		memset(cFilepath, 0, 1024);
		strcpy(cFilepath, ":");
		strcat(cFilepath, *gControls->cfg->checks[i].subfolder);
		strcat(cFilepath, ":");
		strcat(cFilepath, *gControls->cfg->checks[i].filename);
		HUnlock(gControls->cfg->checks[i].filename);
		pSubfolder = CToPascal(*gControls->cfg->checks[i].subfolder);
		HUnlock(gControls->cfg->checks[i].subfolder);
		pFilepath = CToPascal(cFilepath);
		
		err = FSMakeFSSpec(vRefNum, dirID, pFilepath, &legacy);
		if (pFilepath)
			DisposePtr((Ptr)pFilepath);
			
		/* if legacy file exists */
		if (err == noErr)
		{
			/* if new version is greater than old version */
			diffLevel = CompareVersion( gControls->cfg->checks[i].version, 
										&legacy );
			if (diffLevel > 0)
			{
				/* set up message dlg */
				if (!gControls->cfg->checks[i].message || !(*gControls->cfg->checks[i].message))
					continue;
				HLock(gControls->cfg->checks[i].message);
				pMessage = CToPascal(*gControls->cfg->checks[i].message);
				HUnlock(gControls->cfg->checks[i].message);
				if (!pMessage)
					continue;
				
				/* set bRetry to retval of show message dlg */
				alertdlg = (AlertStdAlertParamRec *)NewPtrClear(sizeof(AlertStdAlertParamRec));
				alertdlg->defaultButton = kAlertStdAlertOKButton;
				alertdlg->defaultText = (ConstStringPtr)NewPtrClear(kKeyMaxLen);
				alertdlg->cancelText = (ConstStringPtr)NewPtrClear(kKeyMaxLen);
			    GetResourcedString((unsigned char *)alertdlg->defaultText, rInstList, sDeleteBtn);
			    GetResourcedString((unsigned char *)alertdlg->cancelText, rInstList, sCancel);
				StandardAlert(kAlertCautionAlert, pMessage, nil, alertdlg, &dlgRV);
				if (dlgRV == 1) /* default button id  ("Delete") */
				{			
					/* delete and move on to next screen */
					err = FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, pSubfolder, &fsDest);
					if (err == noErr) /* don't care if this fails */
						DeleteDirectoryContents(fsDest.vRefNum, fsDest.parID, fsDest.name);
				}
				else
					bRetry = true;
					
				if (pMessage)
					DisposePtr((Ptr) pMessage);
			}
		}
	}
	
	if (pSubfolder)
		DisposePtr((Ptr) pSubfolder);
		
	return bRetry;
}

int
CompareVersion(Handle newVersion, FSSpecPtr file)
{
	int			diffLevel = 0, intVal;
	OSErr		err = noErr;
	short		fileRef;
	Handle		versRsrc = nil;
	char		oldRel, oldRev, oldFix, oldInternalStage, oldDevStage, oldRev_n_Fix;
	char		*newRel, *newRev, newFix[2], *newInternalStage, newDevStage, *newFix_n_DevStage;
	Ptr			newVerCopy;
	
	/* no version supplied means show check always */
	if (!newVersion || !(*newVersion))
		return 6;
	
	/* if no valid filename then error so don't show message */
	if (!file)
		return -6;	
		
	/* get version from 'vers' res ID = 1 */		
	fileRef = FSpOpenResFile(file, fsRdPerm);
	if (fileRef == -1)
		return -9;
		
	versRsrc = Get1Resource('vers', 1);
	if (versRsrc == nil)
	{
		CloseResFile(fileRef);
		return -10;
	}
	
	// rel, rev, fix, internalStage, devStage
	HLock(versRsrc);
	oldRel = *(*versRsrc);
	oldRev_n_Fix = *((*versRsrc)+1);
	oldDevStage = *((*versRsrc)+2);
	oldInternalStage = *((*versRsrc)+3);
	HUnlock(versRsrc);
	CloseResFile(fileRef);
	
	oldRev = (oldRev_n_Fix & 0xF0) >> 4;
	oldFix =  oldRev_n_Fix & 0x0F;
	
	/* parse new version */
	HLock(newVersion);
	newVerCopy = NewPtrClear(strlen(*newVersion));
	BlockMove(*newVersion, newVerCopy, strlen(*newVersion));
	newRel = strtok(newVerCopy, ".");
	newRev = strtok(NULL, ".");
	newFix_n_DevStage = strtok(NULL, ".");
	newInternalStage = strtok(NULL, ".");
	HUnlock(newVersion);
	
	/* resolve fix and devStage 
	 *(defaulting devStage to 0x80(==release) if not detected) 
	 */
	newDevStage = 0x80; 					/* release */
	if (NULL != strchr(newFix_n_DevStage, 'd')) 
		newDevStage = 0x20;					/* development */
	else if (NULL != strchr(newFix_n_DevStage, 'a'))
		newDevStage = 0x40;					/* alpha */
	else if (NULL != strchr(newFix_n_DevStage, 'b')) 
		newDevStage = 0x60;					/* beta */
	 	
	newFix[0] = *newFix_n_DevStage;
	newFix[1] = 0;
	
	/* compare 'vers' -- old version -- with supplied new version */
	intVal = atoi(newRel);
	if (oldRel < intVal)
	{
		diffLevel = 5;
		goto au_revoir;
	}
	else if (oldRel > intVal)
	{
		diffLevel = -5;
		goto au_revoir;
	}
		
	intVal = atoi(newRev);
	if (oldRev < intVal)
	{
		diffLevel = 4;
		goto au_revoir;
	}
	else if (oldRev > intVal)
	{
		diffLevel = -4;
		goto au_revoir;
	}
		
	intVal = atoi(newFix);
	if (oldFix < intVal)
	{	
		diffLevel = 3;
		goto au_revoir;
	}
	else if (oldFix > intVal)
	{
		diffLevel = -3;
		goto au_revoir;
	}
	
	intVal = atoi(newInternalStage);
	if (oldInternalStage < intVal)
	{
		diffLevel = 2;
		goto au_revoir;
	}
	else if (oldInternalStage > intVal)
	{
		diffLevel = -2;
		goto au_revoir;
	}
	
	if (oldDevStage < newDevStage)
	{
		diffLevel = 1;
		goto au_revoir;
	}
	else if (oldDevStage > newDevStage)
	{
		diffLevel = -1;
		goto au_revoir;
	}
	
	/* else they are equal */
	diffLevel = 0;

au_revoir:
	if (newVerCopy)
		DisposePtr(newVerCopy);
			
	return diffLevel;
}

Boolean
VerifyDiskSpace(void)
{
    char dsNeededStr[255], dsAvailStr[255];
    short alertRV;
    Str255 pMessage, pStr;
    AlertStdAlertParamRec *alertdlg;
    
    if (sDSNeededK > sDSAvailK)
    {
        sprintf(dsNeededStr, "%d", sDSNeededK);
        sprintf(dsAvailStr, "%d", sDSAvailK);

        GetResourcedString(pMessage, rInstList, sSpaceMsg1);
        pstrcat(pMessage, CToPascal(dsAvailStr));
        pstrcat(pMessage, CToPascal("KB \r"));
        GetResourcedString(pStr, rInstList, sSpaceMsg2);
        pstrcat(pStr, CToPascal(dsNeededStr));
        pstrcat(pStr, CToPascal("KB \r\r"));
        pstrcat(pMessage, pStr);
        GetResourcedString(pStr, rInstList, sSpaceMsg3);
        pstrcat(pMessage, pStr);
        alertdlg = (AlertStdAlertParamRec *)NewPtrClear(sizeof(AlertStdAlertParamRec));
        alertdlg->defaultButton = kAlertStdAlertCancelButton;
        alertdlg->defaultText = (ConstStringPtr)NewPtrClear(kKeyMaxLen);
        alertdlg->cancelText = (ConstStringPtr)NewPtrClear(kKeyMaxLen);
        GetResourcedString((unsigned char *)alertdlg->defaultText, rInstList, sOKBtn);
        GetResourcedString((unsigned char *)alertdlg->cancelText, rInstList, sQuitBtn);
        StandardAlert(kAlertCautionAlert, pMessage, nil, alertdlg, &alertRV);
        if (alertRV == 2)
        {
            gDone = true;
        }
        return false;
    }    
    
    return true;
}

void
EnableSetupTypeWin(void)
{
    EnableNavButtons();
	
    /* ensure 'Go Back' button is visbily disabled 
     * (since functionality is disconnected) 
     */
    if (gControls->backB)
        HiliteControl(gControls->backB, kDisableControl);
        
    if (gControls->stw->instType)
        HiliteControl(gControls->stw->instType, kEnableControl);
    if (gControls->stw->destLoc)
        HiliteControl(gControls->stw->destLoc, kEnableControl);
}

void
DisableSetupTypeWin(void)
{
	DisableNavButtons();
	
	if (gControls->stw->instType)
		HiliteControl(gControls->stw->instType, kDisableControl);
	if (gControls->stw->destLoc)
		HiliteControl(gControls->stw->destLoc, kDisableControl);
}
