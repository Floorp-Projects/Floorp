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
//dougt: maybe you should init these here to null
Boolean 	gDone;
WindowPtr 	gWPtr;
short		gCurrWin;
InstWiz		*gControls;

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
	OSErr			err;
	
	gDone = false;
	InitManagers();
	InitControlsObject();	
	InitOptObject();

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
//dougt: will sd put up some ui while the main thread blocks?

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
	//pstrcpy(winTitle, "\pNetscape Installer Dude");
	SetWTitle( gWPtr, winTitle );	
	MakeMenus();

	ParseConfig(); 
	
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
//dougt: what happens when allocation fails!	
	/* SetupTypeWin options */
	gControls->opt->instChoice = 1;		
	gControls->opt->folder = (unsigned char *)NewPtrClear(64*sizeof(unsigned char));
//dougt: what happens when allocation fails!	
	ERR_CHECK(GetCWD(&gControls->opt->dirID, &gControls->opt->vRefNum));
	ERR_CHECK(FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, NULL, &tmp));
	
	pstrcpy( gControls->opt->folder, tmp.name );
	
	/* ComponentsWin options */
	for (i=0; i<kMaxComponents; i++)
		gControls->opt->compSelected[i] = kNotSelected;
}

void
InitControlsObject(void)
{	
	gControls 		= (InstWiz *) 		NewPtrClear(sizeof(InstWiz));
	gControls->lw 	= (LicWin *) 		NewPtrClear(sizeof(LicWin));
	gControls->ww 	= (WelcWin *) 		NewPtrClear(sizeof(WelcWin));
	gControls->stw 	= (SetupTypeWin *) 	NewPtrClear(sizeof(SetupTypeWin));	
	gControls->cw 	= (CompWin *) 		NewPtrClear(sizeof(CompWin));
	gControls->tw 	= (TermWin*) 		NewPtrClear(sizeof(TermWin));
//dougt: what happens when allocation fails!
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
//dougt: the use of ErrorHandler is wrong here.  Since it will not 'exit to shell', execution will continue which is not desired.
    Handle 		mbarHdl;
	MenuHandle	menuHdl;
	OSErr		err;
	
	if ( !(mbarHdl = GetNewMBar( rMBar)) )
		ErrorHandler();
	SetMenuBar(mbarHdl);   //dougt: if mbarHdl allocation failes above, poof.
	
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
	
	while (!gDone) 
	{				
		YieldToAnyThread();  /* SmartDownload dialog thread */
		
		if (!gDone)	 /* after cx switch back ensure not done */
		{
			if(WaitNextEvent(everyEvent, &evt, 0, NULL))
			{
#ifdef SDINST_IS_DLL
				notHandled = gSDIEvtHandler(&evt);
#else			
				notHandled = SDI_HandleEvent(&evt);	
#endif
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
/* deallocate config object */
	// TO DO	
	
/* deallocate options object */
	// TO DO
		
/* deallocate all controls */	
//dougt: check for null before deleting!
	DisposePtr( (char*) gControls->lw);
	// DisposeControl(gControls->nextB);  
	// DisposeControl(gControls->backB);
	DisposePtr( (char*) gControls->ww);
	DisposePtr( (char*) gControls->stw);
	DisposePtr( (char*) gControls->cw);
	DisposePtr( (char*) gControls->tw);
	
	DisposePtr( (char*) gControls);
	DisposeWindow(gWPtr);
	
	UnloadSDLib(&gConnID);
}