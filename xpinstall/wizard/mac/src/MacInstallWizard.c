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
WindowPtr 	gWPtr = NULL;
short		gCurrWin = 0;
InstWiz		*gControls = NULL;
InstINIRes  *gStrings = NULL;
Boolean     gInstallStarted = false;
ErrTableEnt *gErrTable = NULL;
short gErrTableSize = 0;



/*-----------------------------------------------------------*
 *   Application Setup
 *-----------------------------------------------------------*/

int gState;

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
        ErrorHandler(err, nil);
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
	OSErr			err = noErr;
    Str255			instMode;
    Ptr				pinstMode;
	
	gDone = false;
	InitManagers();
	InitControlsObject();	
	CleanTemp();

	ParseInstall();

	gWPtr = GetNewCWindow(rRootWin, NULL, (WindowPtr) -1);	
    GetIndString( instMode, rTitleStrList, sNSInstTitle);
    pinstMode = PascalToC(instMode);
#if MOZILLA == 0
    GetResourcedString(winTitle, rInstList, sNsTitle);
#else
    GetResourcedString(winTitle, rInstList, sMoTitle);
#endif
	SetWTitle( gWPtr, winTitle );	
	SetWRefCon(gWPtr, kMIWMagic);
	MakeMenus();

	ParseConfig(); 
	InitOptObject();
	
	ShowWelcomeWin();	
	SetThemeWindowBackground(gWPtr, kThemeBrushDialogBackgroundActive, true); 
	
	/* Set menu */
	InitNewMenu();
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
        ErrorHandler(eMem, nil);
		return;
	}
	
	/* SetupTypeWin options */
	gControls->opt->instChoice = 1;		
	gControls->opt->folder = (unsigned char *)NewPtrClear(64*sizeof(unsigned char));
	if (!gControls->opt->folder)
	{
        ErrorHandler(eMem, nil);
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
        ErrorHandler(eMem, nil);
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
        ErrorHandler(eMem, nil);
	}
	
	gControls->state = eInstallNotStarted;
	
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
        ErrorHandler(eMem, nil);
		return;
	}
	
	SetMenuBar(mbarHdl);
	
	if ( (menuHdl = GetMenuHandle(mApple)) != nil) 
	{
		AppendResMenu(menuHdl, 'DRVR');
	}
	else
        ErrorHandler(eMenuHdl, nil); 

	ERR_CHECK(HMGetHelpMenuHandle(&menuHdl));
	DisableItem(menuHdl, 1);

	DrawMenuBar();
}

static 	RgnHandle gMouseRgn;

void MainEventLoop(void)
{
	gMouseRgn = NewRgn();
	
	while (!gDone) 
	{	
        YieldToAnyThread();  /* download thread */
	    MainEventLoopPass();	
	}
	
	if (gMouseRgn)
		DisposeRgn(gMouseRgn);
	gMouseRgn = (RgnHandle) 0;
	Shutdown();
}
 
// following needs to return int so it can be used as a libXPnet callback

int BreathFunc()
{
    static int ticks = 0;
    
    ticks++;
    if ( ( ticks % 4 ) == 0 ) {
        ticks = 0;
        MainEventLoopPass();
        if ( gDone == true ) {     // this is likely because user selected Quit from the file menu
            if (gMouseRgn)
		        DisposeRgn(gMouseRgn);
	        gMouseRgn = (RgnHandle) 0;
	        Shutdown();
	    } 
    }
    return 1;
}

void  MainEventLoopPass()
{
    EventRecord evt;
	Boolean		notHandled = true;

    if (!gDone)	 /* after cx switch back ensure not done */
    {
		if(WaitNextEvent(everyEvent, &evt, 1, gMouseRgn))
		{
			if (gMouseRgn)
				SetRectRgn(gMouseRgn, evt.where.h, evt.where.v, evt.where.h + 1, evt.where.v + 1);
					
			HandleNextEvent(&evt);
		}
	}
}
 
void ErrorHandler(short errCode, Str255 msg)
{
// TO DO
//		* handle a "fatality" parameter for recovery

    Str255      pErrorStr;
    Str255      pMessage, errMsg;
    char        *cErrNo = 0;
    StringPtr   pErrNo = 0;
    AlertStdAlertParamRec *alertdlg;

    // only throw up the error dialog once (since we have no fatality param)
    static Boolean bErrHandled = false;
    if (bErrHandled)
        return;
    else
        bErrHandled = true;
        
    // if install.ini read failed
    if( errCode == eInstRead )
    {
        GetIndString(pErrorStr, rStringList, errCode);
        ParamText(pErrorStr, "\p", "\p", "\p");
        StopAlert(rAlrtError, nil);
        SysBeep(10);
        gDone = true;
        return;
    }
	
    GetResourcedString(pMessage, rErrorList, eErr1);
    GetResourcedString(pErrorStr, rErrorList, eErr2);
    
    cErrNo = ltoa(errCode);
    pErrNo = CToPascal(cErrNo);
    
    if (errCode > 0)    // negative errors are definitely from the system so we don't interpret
    {
        GetResourcedString(pErrorStr, rErrorList, errCode);
        pstrcat(pMessage, pErrNo);
        pstrcat(pMessage, "\p: ");
        pstrcat(pMessage, pErrorStr);
    }
    else
    {
        GetResourcedString(pMessage, rErrorList, eErr3);
        if ( LookupErrorMsg( errCode, errMsg ) == true )
          pstrcat(pMessage, errMsg);
        else 
          pstrcat(pMessage, pErrNo);
        if ( msg[0] != 0 ) {
          pstrcat(pMessage, "\p : ");
          pstrcat(pMessage, msg);
        }
    }  
        
    alertdlg = (AlertStdAlertParamRec *)NewPtrClear(sizeof(AlertStdAlertParamRec));
    alertdlg->defaultButton = kAlertStdAlertOKButton;
    alertdlg->defaultText = (ConstStringPtr)NewPtrClear(kKeyMaxLen);
    GetResourcedString((unsigned char *)alertdlg->defaultText, rInstList, sOKBtn);
    StandardAlert(kAlertStopAlert, pMessage, nil, alertdlg, 0);
	  SysBeep(10);
	
    if (cErrNo)
        free(cErrNo);
    if (pErrNo)
        DisposePtr((Ptr) pErrNo); 
        
	gDone = true;
}

Boolean
LookupErrorMsg( short code, Str255 msg )
{
    int i;
    Boolean retval = false;
    msg[0] = 1; msg[1] = ' ';
    
    for ( i = 0; i < gErrTableSize; i++ ) {
      if ( gErrTable[i].num == code ) {
          pstrcat( msg, gErrTable[i].msg );
          retval = true;
          break;
        }   
    }
    return( retval );
}

void Shutdown(void)
{
	WindowPtr	frontWin;
	long 		MIWMagic = 0;

	NavUnload();
	
#if 0

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

	if (gControls->nextB)
		DisposeControl(gControls->nextB);  
	if (gControls->backB)
		DisposeControl(gControls->backB);
	
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
		
#endif /* 0 */
			
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
	ExitToShell();
}

//set new menu groups and items from install.ini
void InitNewMenu()
{
    MenuHandle		instMenu=0;
    MenuRef			fileMenu, editMenu;
    Str255			menuText;

    instMenu = GetMenuHandle(mApple);
#if MOZILLA == 0
    	GetResourcedString(menuText, rInstMenuList, sMenuAboutNs);
#else
    	GetResourcedString(menuText, rInstMenuList, sMenuAboutMo);
#endif
    SetMenuItemText(instMenu, iAbout, menuText);
    
    GetResourcedString(menuText, rInstMenuList, sMenuFile);
    fileMenu = NewMenu(mFile, menuText);
    InsertMenu(fileMenu, mFile);
    GetResourcedString(menuText, rInstMenuList, sMenuEdit);
    editMenu = NewMenu(mEdit, menuText);
    InsertMenu(editMenu, mEdit);
    DrawMenuBar();

    GetResourcedString(menuText, rInstMenuList, sMenuQuit);
    AppendMenu(fileMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuQuitHot);
    SetItemCmd(fileMenu, iQuit, menuText[1]);
    
    GetResourcedString(menuText, rInstMenuList, sMenuUndo);
    AppendMenu(editMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuUndoHot);
    SetItemCmd(editMenu, iUndo, menuText[1]);
    pstrcpy(menuText, CToPascal("-"));
    AppendMenu(editMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuCut);
    AppendMenu(editMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuCutHot);
    SetItemCmd(editMenu, iCut, menuText[1]);
    GetResourcedString(menuText, rInstMenuList, sMenuCopy);
    AppendMenu(editMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuCopyHot);
    SetItemCmd(editMenu, iCopy, menuText[1]);
    GetResourcedString(menuText, rInstMenuList, sMenuPaste);
    AppendMenu(editMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuPasteHot);
    SetItemCmd(editMenu, iPaste, menuText[1]);
    GetResourcedString(menuText, rInstMenuList, sMenuClear);
    AppendMenu(editMenu, menuText);
    GetResourcedString(menuText, rInstMenuList, sMenuClearHot);
    SetItemCmd(editMenu, iClear, menuText[1]);
}
