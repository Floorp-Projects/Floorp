/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "MacInstallWizard.h"


/*-----------------------------------------------------------*
 *   Welcome Window
 *-----------------------------------------------------------*/

void 
ShowWelcomeWin(void)
{
	Str255 next;
	Str255 back;
	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	if (gWPtr != NULL)
	{
		SetPort(gWPtr);

		gCurrWin = kWelcomeID; 
	
		GetResourcedString(next, rInstList, sNextBtn);
		GetResourcedString(back, rInstList, sBackBtn);

        ShowWelcomeMsg();
		ShowNavButtons(back, next);
		if (gControls->cfg->bReadme)
			ShowReadmeButton();
	}
	
	SetPort(oldPort);
}

void
ShowWelcomeMsg(void)
{    
    int welcStrLen, i;
    
    for (i = 0; i < kNumWelcMsgs; i++)
    {
        gControls->ww->welcMsgCntl[i] = GetNewControl(rWelcMsgTextbox+i, gWPtr);
    	if (!gControls->ww->welcMsgCntl)
    	{
    		ErrorHandler(eMem, nil);
    		return;
    	}
	}
    
    ControlFontStyleRec fontStyle;

    fontStyle.flags =  kControlUseSizeMask | kControlUseFaceMask;
    fontStyle.size = 18;
    fontStyle.style = normal;
    SetControlFontStyle(gControls->ww->welcMsgCntl[0], &fontStyle);

    HLock(gControls->cfg->welcMsg[0]);
    welcStrLen = strlen(*gControls->cfg->welcMsg[0]);
    HUnlock(gControls->cfg->welcMsg[0]);
    SetControlData(gControls->ww->welcMsgCntl[0], kControlNoPart, 
        kControlStaticTextTextTag, welcStrLen, (Ptr)*gControls->cfg->welcMsg[0]); 
    
    fontStyle.flags =  kControlUseSizeMask;
    fontStyle.size = 12;

    for (i = 1; i < kNumWelcMsgs; i++)
    {
        SetControlFontStyle(gControls->ww->welcMsgCntl[i], &fontStyle);

        HLock(gControls->cfg->welcMsg[i]);
        welcStrLen = strlen(*gControls->cfg->welcMsg[i]);
        HUnlock(gControls->cfg->welcMsg[i]);
        SetControlData(gControls->ww->welcMsgCntl[i], kControlNoPart, 
            kControlStaticTextTextTag, welcStrLen, (Ptr)*gControls->cfg->welcMsg[i]); 
    }  

    for (i = 0; i < kNumWelcMsgs; i++)
    {
	    ShowControl(gControls->ww->welcMsgCntl[i]);
	}
}

void 
InWelcomeContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 			localPt;
	Rect			r;
	ControlPartCode	part;
	GrafPtr			oldPort;
	
	GetPort(&oldPort);
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
			
	HLock((Handle)gControls->nextB);			
	r = (**(gControls->nextB)).contrlRect;
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{
			KillControls(gWPtr);
			ShowLicenseWin();
			return;
		}
	}
	
	HLock((Handle)gControls->ww->readmeButton);
	r = (**(gControls->ww->readmeButton)).contrlRect;
	HUnlock((Handle)gControls->ww->readmeButton);
	if (PtInRect(localPt, &r))
	{
		part = TrackControl(gControls->ww->readmeButton, evt->where, NULL);
		if (part)
		{
			ShowReadme();
			return;
		}
	}
	
	SetPort(oldPort);
}

void
ShowCancelButton(void)
{
	Str255 cancelStr;
	
	GetResourcedString(cancelStr, rInstList, sCancel);
	gControls->cancelB = GetNewControl(rCancelBtn, gWPtr);
	if (gControls->cancelB != NULL)
	{
		SetControlTitle(gControls->cancelB, cancelStr);
		ShowControl(gControls->cancelB);
	}
}

void 
ShowReadmeButton(void)
{
	Str255 readme;
	
	GetResourcedString(readme, rInstList, sReadme);
	gControls->ww->readmeButton = GetNewControl(rReadmeBtn, gWPtr);
	if (gControls->ww->readmeButton != NULL)
	{
		SetControlTitle(gControls->ww->readmeButton, readme);
		ShowControl(gControls->ww->readmeButton);
	}
}
	
void
ShowReadme(void)
{
	Ptr appSig;
	StringPtr file;
	OSErr err = noErr;
	FSSpec appSpec, docSpec;
	Boolean running = nil;
	ProcessSerialNumber psn;
	unsigned short launchFileFlags, launchControlFlags;
	unsigned long appSigULong;
	long currDirID;
	short currVRefNum;
	
	appSig = *gControls->cfg->readmeApp;
	appSigULong = 0x00000000;
	UNIFY_CHAR_CODE(appSigULong, *(appSig), *(appSig+1), *(appSig+2), *(appSig+3));
	err = FindAppUsingSig(appSigULong, &appSpec, &running, &psn);
	if (err != noErr)
	{
		SysBeep(10); // XXX  show error dialog
		goto au_revoir;
	}
	
	file = CToPascal(*gControls->cfg->readmeFile); 
	GetCWD(&currDirID, &currVRefNum);
	err = FSMakeFSSpec(currVRefNum, currDirID, file, &docSpec);
	if (err != noErr)
	{
		SysBeep(10); // XXX  show error dialog
		goto au_revoir;
	}
		
	launchFileFlags = NULL;
	launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
	err = LaunchAppOpeningDoc(running, &appSpec, &psn, &docSpec, 
							launchFileFlags, launchControlFlags);
	if (err != noErr)
	{
		SysBeep(10); // XXX  show error dialog
		goto au_revoir;
	}

au_revoir:	
	if (file)
		DisposePtr((Ptr) file);
		
	return;
}

void
EnableWelcomeWin(void)
{
	EnableNavButtons();
	
	if (gControls->cfg->bReadme)
		if (gControls->ww->readmeButton)
			HiliteControl(gControls->ww->readmeButton, kEnableControl);
}

void
DisableWelcomeWin(void)
{
	DisableNavButtons();
	
	if (gControls->cfg->bReadme)
		if (gControls->ww->readmeButton)
			HiliteControl(gControls->ww->readmeButton, kDisableControl);
}

OSErr LaunchAppOpeningDoc (Boolean running, FSSpec *appSpec, ProcessSerialNumber *psn,
	FSSpec *docSpec, unsigned short launchFileFlags, unsigned short launchControlFlags)
{
	ProcessSerialNumber thePSN;
	AEDesc 		target = {0, nil};
	AEDesc 		docDesc = {0, nil};
	AEDesc 		launchDesc = {0, nil};
	AEDescList 	docList = {0, nil};
	AppleEvent 	theEvent = {0, nil};
	AppleEvent 	theReply = {0, nil};
	OSErr 		err = noErr;
	Boolean 		autoParamValue = false;

	if (running) thePSN = *psn;
	err = AECreateDesc(typeProcessSerialNumber, &thePSN, sizeof(thePSN), &target); 
	if (err != noErr) goto exit;
	
	err = AECreateAppleEvent(kCoreEventClass, kAEOpenDocuments, &target,
		kAutoGenerateReturnID, kAnyTransactionID, &theEvent);
	if (err != noErr) goto exit;
	
	if (docSpec)
	{
		err = AECreateList(nil, 0, false, &docList);
		if (err != noErr) goto exit;
		
		err = AECreateDesc(typeFSS, docSpec, sizeof(FSSpec), &docDesc);
		if (err != noErr) goto exit;
		
		err = AEPutDesc(&docList, 0, &docDesc);
		if (err != noErr) goto exit;
		
		err = AEPutParamDesc(&theEvent, keyDirectObject, &docList);
		if (err != noErr) goto exit;
	}
	
	if (running)
	{
		err = AESend(&theEvent, &theReply, kAENoReply, kAENormalPriority, kNoTimeOut, nil, nil);
		if (err != noErr) goto exit;
		if ((launchControlFlags & launchDontSwitch) == 0) {
			err = SetFrontProcess(psn);
			if (err != noErr) goto exit;
		}
	}
	else
	{
		LaunchParamBlockRec	launchThis = {0};
		
		err = AECoerceDesc(&theEvent, typeAppParameters, &launchDesc);
		if (err != noErr) goto exit;
		HLock(theEvent.dataHandle);
		
		launchThis.launchAppSpec = appSpec;
		launchThis.launchAppParameters = (AppParametersPtr)*launchDesc.dataHandle;
		launchThis.launchBlockID = extendedBlock;
		launchThis.launchEPBLength = extendedBlockLen;
		launchThis.launchFileFlags = launchFileFlags;
		launchThis.launchControlFlags = launchControlFlags;
		err = LaunchApplication(&launchThis);
	}
	
exit:

	if (target.dataHandle != nil) AEDisposeDesc(&target);
	if (docDesc.dataHandle != nil) AEDisposeDesc(&docDesc);
	if (launchDesc.dataHandle != nil) AEDisposeDesc(&launchDesc);
	if (docList.dataHandle != nil) AEDisposeDesc(&docList);
	if (theEvent.dataHandle != nil) AEDisposeDesc(&theEvent);
	if (theReply.dataHandle != nil) AEDisposeDesc(&theReply);
	return err;
}

OSErr FindAppUsingSig (OSType sig, FSSpec *fSpec, Boolean *running, ProcessSerialNumber *psn)
{
	OSErr 	err = noErr;
	short 	sysVRefNum, vRefNum, index;
	Boolean 	hasDesktopDB;

	if (running != nil) {
		err = FindRunningAppBySignature(sig, fSpec, psn);
		*running = true;
		if (err == noErr) return noErr;
		*running = false;
		if (err != procNotFound) return err;
	}
	err = GetSysVolume(&sysVRefNum);
	if (err != noErr) return err;
	vRefNum = sysVRefNum;
	index = 0;
	while (true) {
		if (index == 0 || vRefNum != sysVRefNum) {
			err = VolHasDesktopDB(vRefNum, &hasDesktopDB);
			if (err != noErr) return err;
			if (hasDesktopDB) {
				err = FindAppOnVolume(sig, vRefNum, fSpec);
				if (err != afpItemNotFound) return err;
			}
		}
		index++;
		err = GetIndVolume(index, &vRefNum);
		if (err == nsvErr) return fnfErr;
		if (err != noErr) return err;
	}
}

OSErr FindRunningAppBySignature (OSType sig, FSSpec *fSpec, ProcessSerialNumber *psn)
{
	OSErr 			err = noErr;
	ProcessInfoRec 	info;
	FSSpec			tempFSSpec;
	
	psn->highLongOfPSN = 0;
	psn->lowLongOfPSN  = kNoProcess;
	while (true)
	{
		err = GetNextProcess(psn);
		if (err != noErr) return err;
		info.processInfoLength = sizeof(ProcessInfoRec);
		info.processName = nil;
		info.processAppSpec = &tempFSSpec;
		err = GetProcessInformation(psn, &info);
		if (err != noErr) return err;
		
		if (info.processSignature == sig)
		{
			if (fSpec != nil)
				*fSpec = tempFSSpec;
			return noErr;
		}
	}
	
	return procNotFound;
}

static OSErr VolHasDesktopDB (short vRefNum, Boolean *hasDesktop)
{
	HParamBlockRec 		pb;
	GetVolParmsInfoBuffer	info;
	OSErr 				err = noErr;
	
	pb.ioParam.ioCompletion = nil;
	pb.ioParam.ioNamePtr = nil;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.ioParam.ioBuffer = (Ptr)&info;
	pb.ioParam.ioReqCount = sizeof(info);
	err = PBHGetVolParmsSync(&pb);
	*hasDesktop = err == noErr && (info.vMAttrib & (1L << bHasDesktopMgr)) != 0;
	return err;
}

static OSErr FindAppOnVolume (OSType sig, short vRefNum, FSSpec *file)
{
	DTPBRec pb;
	OSErr err = noErr;
	short ioDTRefNum, i;
	FInfo fInfo;
	FSSpec candidate;
	unsigned long lastModDateTime, maxLastModDateTime;

	memset(&pb, 0, sizeof(DTPBRec));
	pb.ioCompletion = nil;
	pb.ioVRefNum = vRefNum;
	pb.ioNamePtr = nil;
	err = PBDTGetPath(&pb);
	if (err != noErr) return err;
	ioDTRefNum = pb.ioDTRefNum;

	memset(&pb, 0, sizeof(DTPBRec));
	pb.ioCompletion = nil;
	pb.ioIndex = 0;
	pb.ioFileCreator = sig;
	pb.ioNamePtr = file->name;
	pb.ioDTRefNum = ioDTRefNum;
	err = PBDTGetAPPL(&pb, false);
	
	if (err == fnfErr || err == paramErr) return afpItemNotFound;
	if (err != noErr) return err;

	file->vRefNum = vRefNum;
	file->parID = pb.ioAPPLParID;
	
	err = FSpGetFInfo(file, &fInfo);
	if (err == noErr) return noErr;
	
	i = 1;
	maxLastModDateTime = 0;
	while (true) {
		memset(&pb, 0, sizeof(DTPBRec)); 
		pb.ioCompletion = nil;
		pb.ioIndex = i;
		pb.ioFileCreator = sig;
		pb.ioNamePtr = candidate.name;
		pb.ioDTRefNum = ioDTRefNum;
		err = PBDTGetAPPLSync(&pb);
		if (err != noErr) break;
		candidate.vRefNum = vRefNum;
		candidate.parID = pb.ioAPPLParID;
		err = GetLastModDateTime(&candidate, &lastModDateTime);
		if (err == noErr) {
			if (lastModDateTime > maxLastModDateTime) {
				maxLastModDateTime = lastModDateTime;
				*file = candidate;
			}
		}
		i++;
	}
	
	return maxLastModDateTime > 0 ? noErr : afpItemNotFound;
}

OSErr GetSysVolume (short *vRefNum)
{
	long dir;
	
	return FindFolder(kOnSystemDisk, kSystemFolderType, false, vRefNum, &dir);
}

OSErr GetIndVolume (short index, short *vRefNum)
{
	ParamBlockRec pb;
	OSErr err = noErr;
	
	pb.volumeParam.ioCompletion = nil;
	pb.volumeParam.ioNamePtr = nil;
	pb.volumeParam.ioVolIndex = index;
	
	err = PBGetVInfoSync(&pb);
	
	*vRefNum = pb.volumeParam.ioVRefNum;
	return err;
}

OSErr GetLastModDateTime(const FSSpec *fSpec, unsigned long *lastModDateTime)
{
	CInfoPBRec	pBlock;
	OSErr 		err = noErr;
	
	pBlock.hFileInfo.ioNamePtr = (StringPtr)fSpec->name;
	pBlock.hFileInfo.ioVRefNum = fSpec->vRefNum;
	pBlock.hFileInfo.ioFDirIndex = 0;
	pBlock.hFileInfo.ioDirID = fSpec->parID;
	err = PBGetCatInfoSync(&pBlock);
	if (err != noErr) return err;
	*lastModDateTime = pBlock.hFileInfo.ioFlMdDat;
	return noErr;
}

