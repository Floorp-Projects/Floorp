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
 *   globals
 *-----------------------------------------------------------*/

Boolean 	gDone = false;
Boolean 	gSDDlg = false;
WindowPtr 	gWPtr = NULL;
short		gCurrWin = 0;
InstWiz		*gControls = NULL;
Boolean     gInstallStarted = false;

EventProc 			gSDIEvtHandler;  /* SDI */
SDI_NETINSTALL 		gInstFunc;
CFragConnectionID	gConnID;


/*-----------------------------------------------------------*
 *   Application Setup
 *-----------------------------------------------------------*/

void main(void)
{	
	OSErr err = noErr;
	
	Init();
	if (VerifyEnv() && !gDone)	
	{
		err = NavLoad();
		if (err!=noErr)
			SysBeep(10);	// XXX better error handling
			
		ShowWindow(gWPtr);
		MainEventLoop();
	}
}

Boolean
VerifyEnv(void)
{
	long	response;
	OSErr 	err = noErr;
	Boolean bEnvOK = true;
	
	// gestalt to check we are running 8.5 or later
	err = Gestalt('sysv', &response);
	if (err != noErr)
	{
		// errors already!  we are bailing
		ErrorHandler(err);
		bEnvOK = false;
	}
	
	if (response < 0x00000850)
	{
		// we are bailing
		StopAlert(160, nil);
		bEnvOK = false;
	}
	
	// it's all good
	return bEnvOK;
}

void Init(void)
{
	Str255		 	winTitle;
#if CFG_IS_REMOTE == 1
	ThreadID		tid;
	ThreadState		state;
#endif
	OSErr			err = noErr;
	
	gDone = false;
	InitManagers();
	InitControlsObject();	
	CleanTemp();

#if (SDINST_IS_DLL == 1) && (MOZILLA == 0)
	if (!InitSDLib())
	{
		ErrorHandler(eLoadLib);
		return;
	}
#endif

#if CFG_IS_REMOTE == 1
	if (!SpawnSDThread(PullDownConfig, &tid))
	{
		ErrorHandler(eSpawn);
		return;
	}

	/* block/busy wait till download finishes */
	while (1)
	{
		GetThreadState(tid, &state);
		if (state == kStoppedThreadState)
			break;
		else
			sleep(1);
	}

	ERR_CHECK(DisposeThread(tid, (void*)nil, false));

#endif /* CFG_IS_REMOTE == 1 */

	gWPtr = GetNewCWindow(rRootWin, NULL, (WindowPtr) -1);	
    GetIndString( winTitle, rTitleStrList, sNSInstTitle);
	SetWTitle( gWPtr, winTitle );	
	SetWRefCon(gWPtr, kMIWMagic);
	MakeMenus();

	ParseConfig(); 
	InitOptObject();
	
	ShowLicenseWin();	
}

OSErr
GetCWD(long *outDirID, short *outVRefNum)
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
			*outDirID = pInfo.processAppSpec->parID;
			*outVRefNum = pInfo.processAppSpec->vRefNum;
		}
	}
	
	return err;
}

void
InitOptObject(void)
{
	FSSpec 	tmp;
	OSErr	err=noErr;
	Boolean isDir;
	
	gControls->opt = (Options*)NewPtrClear(sizeof(Options));

	if (!gControls->opt)
	{
		ErrorHandler(eMem);
		return;
	}
	
	/* SetupTypeWin options */
	gControls->opt->instChoice = 1;		
	gControls->opt->folder = (unsigned char *)NewPtrClear(64*sizeof(unsigned char));
	if (!gControls->opt->folder)
	{
		ErrorHandler(eMem);
		return;
	}
	
	/* TerminalWIn options */
	gControls->opt->siteChoice = 1;
	gControls->opt->saveBits = false;
	
	gControls->opt->vRefNum = -1;
	err = FSMakeFSSpec(gControls->opt->vRefNum, 0, "\p", &tmp);
	pstrcpy( gControls->opt->folder, tmp.name );
	err = FSpGetDirectoryID( &tmp, &gControls->opt->dirID, &isDir );

}

void
InitControlsObject(void)
{	
	gControls 		= (InstWiz *) 		NewPtrClear(sizeof(InstWiz));
	if (!gControls)
	{
		ErrorHandler(eMem);
		return;
	}
	
	gControls->lw 	= (LicWin *) 		NewPtrClear(sizeof(LicWin));
	gControls->ww 	= (WelcWin *) 		NewPtrClear(sizeof(WelcWin));
	gControls->stw 	= (SetupTypeWin *) 	NewPtrClear(sizeof(SetupTypeWin));	
	gControls->cw 	= (CompWin *) 		NewPtrClear(sizeof(CompWin));
	gControls->aw 	= (CompWin *) 		NewPtrClear(sizeof(CompWin));
	gControls->tw 	= (TermWin*) 		NewPtrClear(sizeof(TermWin));

	if (!gControls->lw || !gControls->ww || !gControls->stw || 
		!gControls->cw || !gControls->tw)
	{
		ErrorHandler(eMem);
	}
	
	return;
}

void InitManagers(void)
{
	MaxApplZone();	
	MoreMasters(); MoreMasters(); MoreMasters();
	
	InitGraf(&qd.thePort);  
	InitFonts();			
	InitWindows();
	InitMenus();
	TEInit();				
	InitDialogs(NULL);
	
	InitCursor();		
	FlushEvents(everyEvent, 0);	
}

void CleanTemp(void)
{
    OSErr   err = noErr;
    short   vRefNum;
    long    dirID;
    FSSpec  viewerFSp;
    XPISpec *xpiList, *currXPI = 0, *nextXPI = 0;
#ifdef MIW_DEBUG
    Boolean isDir = false;
#endif
    
#ifndef MIW_DEBUG
    /* get "viewer" in "Temporary Items" folder */
    ERR_CHECK(FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder, &vRefNum, &dirID));
    err = FSMakeFSSpec(vRefNum, dirID, kViewerFolder, &viewerFSp);
#else
    /* for DEBUG builds temp is "<currProcessVolume>:Temp NSInstall:" */
    ERR_CHECK(GetCWD(&dirID, &vRefNum));
 	err = FSMakeFSSpec(vRefNum, 0, kTempFolder, &viewerFSp);
	if (err == fnfErr)
	    return; /* no debug temp exists */
	err = FSpGetDirectoryID(&viewerFSp, &dirID, &isDir);
	if (err != noErr || !isDir)
	    return;
    err = FSMakeFSSpec(vRefNum, dirID, kViewerFolder, &viewerFSp);
#endif
    
    /* whack the viewer folder if it exists */
    if (err == noErr)
    {
        ERR_CHECK(DeleteDirectory(viewerFSp.vRefNum, viewerFSp.parID, viewerFSp.name));
    }
    
    /* clean out the zippies (.xpi's) */
    xpiList = (XPISpec *) NewPtrClear(sizeof(XPISpec));
    if (!xpiList)
        return;
    IterateDirectory(vRefNum, dirID, "\p", 1, CheckIfXPI, (void*)&xpiList);
    
    if (xpiList)
    {
        currXPI = xpiList;
        while(currXPI)
        {
            nextXPI = currXPI->next; /* save nextXPI before we blow away currXPI */
            if (currXPI->FSp)
            {
                FSpDelete(currXPI->FSp);
                DisposePtr((Ptr)currXPI->FSp);
            }
            DisposePtr((Ptr)currXPI);
            currXPI = nextXPI;
        }
    }
}

pascal void CheckIfXPI(const CInfoPBRec * const cpbPtr, Boolean *quitFlag, void *dataPtr)
{
    OSErr err = noErr;
    char cFilename[256];    /* for convenience: copy the name in cpbPtr->hFileInfo */
    int len = 0;            /* for convenience: length of name string */
    FSSpecPtr currFSp;
    XPISpec *currXPI = 0, *newXPI = 0, **xpiList = 0;
    
    /* param check */
    if (!cpbPtr || !dataPtr)
        return;    
     xpiList = (XPISpec **)dataPtr;
     
    /* file detected */
    if ((cpbPtr->hFileInfo.ioFlAttrib & ioDirMask) == 0)
    {
        if (!cpbPtr->hFileInfo.ioNamePtr)
            return;
        len = *(cpbPtr->hFileInfo.ioNamePtr);            
        strncpy(cFilename, (char*)(cpbPtr->hFileInfo.ioNamePtr + 1), len);
        
        /* check suffix for ".xpi" */   
        if (0 == strncmp(".xpi", cFilename+len-4, 4))
        {
            currFSp = (FSSpecPtr) NewPtrClear(sizeof(FSSpec));
            if (!currFSp)
                return;
            err = FSMakeFSSpec(cpbPtr->hFileInfo.ioVRefNum, cpbPtr->hFileInfo.ioFlParID,
                               cpbPtr->hFileInfo.ioNamePtr, currFSp);
                               
            /* if file exists add it to deletion list */
            if (err == noErr)
            {
                currXPI = *xpiList;
                while (currXPI)
                { 
                    /* list head special case */
                    if (!currXPI->FSp)
                    {
                        newXPI = currXPI;  
                        break;
                    }
                    
                    /* more in list */
                    if (currXPI->next)
                    {
                        currXPI = currXPI->next;
                        continue;
                    }
                    /* list end so allocate new node */ 
                    else
                    {
                        newXPI = (XPISpec *) NewPtrClear(sizeof(XPISpec));
                        if (!newXPI)
                            return;  
                        currXPI->next = newXPI;
                        break;
                    }
                }
                newXPI->FSp = currFSp;
            }
            else
                DisposePtr((Ptr) currFSp);
        }
    }
    
    /* paranoia: make sure we continue iterating */
    *quitFlag = false;
}

void MakeMenus(void)
{
    Handle 		mbarHdl;
	MenuHandle	menuHdl;
	OSErr		err;
	
	if ( !(mbarHdl = GetNewMBar( rMBar)) )
	{
		ErrorHandler(eMem);
		return;
	}
	
	SetMenuBar(mbarHdl);
	
	if ( (menuHdl = GetMenuHandle(mApple)) != nil) 
	{
		AppendResMenu(menuHdl, 'DRVR');
	}
	else
		ErrorHandler(eMenuHdl); 

	if ( (menuHdl = GetMenuHandle(mEdit)) != nil)
		DisableItem(menuHdl, 0);
	else
		ErrorHandler(eMenuHdl);
	
	ERR_CHECK(HMGetHelpMenuHandle(&menuHdl));
	DisableItem(menuHdl, 1);

	DrawMenuBar();
}

void MainEventLoop(void)
{
	EventRecord evt;
	Boolean		notHandled = true;
	THz			ourHZ;
	RgnHandle   mouseRgn;

	mouseRgn = NewRgn();
	
	while (!gDone) 
	{		
		YieldToAnyThread();  /* SmartDownload dialog thread */
		
		if (!gDone)	 /* after cx switch back ensure not done */
		{
			if(WaitNextEvent(everyEvent, &evt, 1, mouseRgn))
			{
				if (mouseRgn)
					SetRectRgn(mouseRgn, evt.where.h, evt.where.v, evt.where.h + 1, evt.where.v + 1);
					
				if (gSDDlg)
				{
					ourHZ = GetZone();
#if MOZILLA == 0
#if SDINST_IS_DLL==1
					notHandled = gSDIEvtHandler(&evt);
#else			
					notHandled = SDI_HandleEvent(&evt);	
#endif /* SDINST_IS_DLL */
#endif /* MOZILLA */
					SetZone(ourHZ);
				}
				else
					notHandled = true;
					
				if (notHandled)
					HandleNextEvent(&evt);
			}
		}
	}
	
	if (mouseRgn)
		DisposeRgn(mouseRgn);
	Shutdown();
}
 
void ErrorHandler(short errCode)
{
// TO DO
//		* handle a "fatality" parameter for recovery

    // only throw up the error dialog once (since we have no fatality param)
    static Boolean bErrHandled = false;
    if (bErrHandled)
        return;
    else
        bErrHandled = true;
        
    Str255      pErrorStr = "\pUnexpected error!";
    Str255      pMessage = "\pError ";
    char        *cErrNo = 0;
    StringPtr   pErrNo = 0;
    
    cErrNo = ltoa(errCode);
    pErrNo = CToPascal(cErrNo);
    
    if (errCode > 0)    // negative errors are definitely from the system so we don't interpret
    {
        GetIndString(pErrorStr, rErrorList, errCode);
        pstrcat(pMessage, pErrNo);
        pstrcat(pMessage, "\p: ");
        pstrcat(pMessage, pErrorStr);
    }
    else
    {
        pstrcpy(pMessage, "\pInstallation failed due to error: ");
        pstrcat(pMessage, pErrNo);
    }  
        
    ParamText(pMessage, "\p", "\p", "\p");
    
    StopAlert(rAlrtError, nil);
	SysBeep(10);
	
    if (cErrNo)
        free(cErrNo);
    if (pErrNo)
        DisposePtr((Ptr) pErrNo); 
        
	gDone = true;
}

void Shutdown(void)
{
	WindowPtr	frontWin;
	long 		MIWMagic = 0, i;

#if (SDINST_IS_DLL == 1) && (MOZILLA == 0)
	UnloadSDLib(&gConnID);
#endif
	NavUnload();
	
/* deallocate config object */
    if (gControls->cfg)
    {
        /* General */
        if (gControls->cfg->targetSubfolder)
            DisposePtr((Ptr) gControls->cfg->targetSubfolder);
        if (gControls->cfg->globalURL)
            DisposePtr((Ptr) gControls->cfg->globalURL);
            
        /* LicenseWin */
        if (gControls->cfg->licFileName)
            DisposePtr((Ptr) gControls->cfg->licFileName);        
            
        /* WelcomeWin */
        for (i = 0; i < kNumWelcMsgs; i++)
        {
            if (gControls->cfg->welcMsg[i])
                DisposePtr((Ptr) gControls->cfg->welcMsg[i]);  
        }      
        if (gControls->cfg->readmeFile)
            DisposePtr((Ptr) gControls->cfg->readmeFile);    
        if (gControls->cfg->readmeApp)
            DisposePtr((Ptr) gControls->cfg->readmeApp);
            
        /* ComponentsWin and AdditionsWin */
        if (gControls->cfg->selCompMsg)
            DisposePtr((Ptr) gControls->cfg->selCompMsg);
        if (gControls->cfg->selAddMsg)
            DisposePtr((Ptr) gControls->cfg->selAddMsg);

        /* TerminalWin */            
        if (gControls->cfg->startMsg)
            DisposePtr((Ptr) gControls->cfg->startMsg);
        if (gControls->cfg->saveBitsMsg)
            DisposePtr((Ptr) gControls->cfg->saveBitsMsg);
                        
        /* "Tunneled" IDI keys */
        if (gControls->cfg->coreFile)
            DisposePtr((Ptr) gControls->cfg->coreFile);  
        if (gControls->cfg->coreDir)
            DisposePtr((Ptr) gControls->cfg->coreDir);  
        if (gControls->cfg->noAds)
            DisposePtr((Ptr) gControls->cfg->noAds);  
        if (gControls->cfg->silent)
            DisposePtr((Ptr) gControls->cfg->silent);  
        if (gControls->cfg->execution)
            DisposePtr((Ptr) gControls->cfg->execution);  
        if (gControls->cfg->confirmInstall)
            DisposePtr((Ptr) gControls->cfg->confirmInstall);
            
        DisposePtr((Ptr)gControls->cfg);
    }
    	
/* deallocate options object */
	if (gControls->opt && gControls->opt->folder)
	{
		DisposePtr((Ptr) gControls->opt->folder);
		DisposePtr((Ptr) gControls->opt);
	}
		
/* deallocate all controls */	

#if 0
/* XXX gets disposed by DisposeWindow() ? */
	if (gControls->nextB)
		DisposeControl(gControls->nextB);  
	if (gControls->backB)
		DisposeControl(gControls->backB);
#endif
	
	if (gControls->lw)
		DisposePtr( (char*) gControls->lw);
	if (gControls->ww)
		DisposePtr( (char*) gControls->ww);
	if (gControls->stw)
		DisposePtr( (char*) gControls->stw);
	if (gControls->cw)
		DisposePtr( (char*) gControls->cw);
	if (gControls->tw)
		DisposePtr( (char*) gControls->tw);
	
	if (gControls)
		DisposePtr( (char*) gControls);
			
	frontWin = FrontWindow();
	MIWMagic = GetWRefCon(frontWin);
	if (MIWMagic != kMIWMagic)
		if (gWPtr)
			BringToFront(gWPtr);

	if (gWPtr)
	{
		HideWindow(gWPtr);
		DisposeWindow(gWPtr);
	}
}
