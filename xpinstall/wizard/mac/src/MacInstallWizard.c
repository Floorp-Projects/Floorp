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
static Boolean bInShutdown = false; 

EventProc 			gSDIEvtHandler;  /* SDI */
SDI_NETINSTALL 		gInstFunc;
CFragConnectionID	gConnID;


/*-----------------------------------------------------------*
 *   Application Setup
 *-----------------------------------------------------------*/

void main(void)
{	
	Init();
	MainEventLoop();
}

void Init(void)
{
	Str255		 	winTitle;
	ThreadID		tid;
	ThreadState		state;
	
	gDone = false;
	InitManagers();
	InitControlsObject();	

#ifdef SDINST_IS_DLL
	if (!InitSDLib())
	{
		ErrorHandler();
		return;
	}
#endif

#if CFG_IS_REMOTE == 1
	if (!SpawnSDThread(PullDownConfig, &tid))
	{
		ErrorHandler();
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
    GetIndString( winTitle, rStringList, sNSInstTitle);
	SetWTitle( gWPtr, winTitle );	
	SetWRefCon(gWPtr, kMIWMagic);
	MakeMenus();

	ParseConfig(); 
	InitOptObject();
	
	ShowLicenseWin();	
	ShowWindow(gWPtr);
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
	int 	i;
	FSSpec 	tmp;
	OSErr	err=noErr;
	
	gControls->opt = (Options*)NewPtrClear(sizeof(Options));

	if (!gControls->opt)
	{
		ErrorHandler();
		return;
	}
	
	/* SetupTypeWin options */
	gControls->opt->instChoice = 1;		
	gControls->opt->folder = (unsigned char *)NewPtrClear(64*sizeof(unsigned char));
	if (!gControls->opt->folder)
	{
		ErrorHandler();
		return;
	}
	
	ERR_CHECK(GetCWD(&gControls->opt->dirID, &gControls->opt->vRefNum));
	ERR_CHECK(FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, NULL, &tmp));
	
	pstrcpy( gControls->opt->folder, tmp.name );
	
	/* ComponentsWin options */
	for (i=0; i<kMaxComponents; i++)
	{
		if (gControls->cfg->st[0].comp[i] == kNotInSetupType)
			gControls->opt->compSelected[i] = kNotSelected;
		else if (gControls->cfg->st[0].comp[i] == kInSetupType)
			gControls->opt->compSelected[i] = kSelected;
	}	
}

void
InitControlsObject(void)
{	
	gControls 		= (InstWiz *) 		NewPtrClear(sizeof(InstWiz));
	if (!gControls)
	{
		ErrorHandler();
		return;
	}
	
	gControls->lw 	= (LicWin *) 		NewPtrClear(sizeof(LicWin));
	gControls->ww 	= (WelcWin *) 		NewPtrClear(sizeof(WelcWin));
	gControls->stw 	= (SetupTypeWin *) 	NewPtrClear(sizeof(SetupTypeWin));	
	gControls->cw 	= (CompWin *) 		NewPtrClear(sizeof(CompWin));
	gControls->tw 	= (TermWin*) 		NewPtrClear(sizeof(TermWin));

	if (!gControls->lw || !gControls->ww || !gControls->stw || 
		!gControls->cw || !gControls->tw)
	{
		ErrorHandler();
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

void MakeMenus(void)
{
    Handle 		mbarHdl;
	MenuHandle	menuHdl;
	OSErr		err;
	
	if ( !(mbarHdl = GetNewMBar( rMBar)) )
	{
		ErrorHandler();
		return;
	}
	
	SetMenuBar(mbarHdl);
	
	if (menuHdl = GetMenuHandle(mApple)) 
	{
		AppendResMenu(menuHdl, 'DRVR');
		DisableItem(menuHdl, 0);
	}
	else
		ErrorHandler(); 
		
	if (menuHdl = GetMenuHandle(mFile))
		DisableItem(menuHdl, 0);
	else
		ErrorHandler();
		
	if (menuHdl = GetMenuHandle(mEdit))
		DisableItem(menuHdl, 0);
	else
		ErrorHandler();
	
	ERR_CHECK(HMGetHelpMenuHandle(&menuHdl));
	DisableItem(menuHdl, 1);

	DrawMenuBar();
}

void MainEventLoop(void)
{
	EventRecord evt;
	Boolean		notHandled = true;
	THz			ourHZ;
	
	while (!gDone) 
	{	
		if (gSDDlg)			
			YieldToAnyThread();  /* SmartDownload dialog thread */
		
		if (!gDone && !bInShutdown)	 /* after cx switch back ensure not done */
		{
			if(WaitNextEvent(everyEvent, &evt, 0, NULL))
			{
				if (gSDDlg)
				{
					ourHZ = GetZone();
#ifdef SDINST_IS_DLL
					notHandled = gSDIEvtHandler(&evt);
#else			
					notHandled = SDI_HandleEvent(&evt);	
#endif
					SetZone(ourHZ);
				}
				else
					notHandled = true;
					
				if (notHandled)
					HandleNextEvent(&evt);
			}
		}
	}
	
	Shutdown();
}
 
void ErrorHandler(void)
{
//TODO: this needs to be fixed.  
	SysBeep(10);
	gDone = true;
}

void Shutdown(void)
{
	WindowPtr	frontWin;
	long 		MIWMagic = 0;
		
	bInShutdown = true;
	UnloadSDLib(&gConnID);
	
/* deallocate config object */
	// TO DO	
	
/* deallocate options object */
	// TO DO
		
/* deallocate all controls */	

#if 0
/* XXX gets dispose by DisposeWindow() ? */
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