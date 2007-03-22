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

#define XP_MAC 1
#include "xpistub.h"

/*-----------------------------------------------------------*
 *   XPInstall Glue
 *-----------------------------------------------------------*/

/*================== XPI Stub Entry Points ================== */
typedef		nsresult (*XPI_InitProc)(const FSSpec& aXPIStubDir, const FSSpec& aProgramDir, const char* aLogName, pfnXPIProgress progressCB);
typedef 	nsresult (*XPI_InstallProc)(const FSSpec& file, const char* args,long flags);
typedef		nsresult (*XPI_ExitProc)();

/*================== XPI Run APIs ============================*/
OSErr		LoadXPIStub(XPI_InitProc* pfnInit, 
						XPI_InstallProc* pfnInstall, 
						XPI_ExitProc* pfnExit, 
						CFragConnectionID* connID,
						FSSpec& aTargetDir, Str255 msg);
Boolean		UnloadXPIStub(CFragConnectionID* connID);
OSErr		RunXPI(FSSpec&, XPI_InstallProc*);
int			CountSelectedXPIs();
Boolean		IsArchiveXPI(StringPtr archive);
void 		ProgressMsgInit();

/*================== Progress Bar Callbacks ==================*/
void	 	xpicbProgress(const char* msg, PRInt32 val, PRInt32 max);

/*================== Macros ==================================*/
#define XPI_ERR_CHECK(_call) 	\
rv = _call;						\
if (NS_FAILED(rv))				\
{								\
	ErrorHandler(rv, nil);			\
	return rv;					\
}

#ifdef MIW_DEBUG
#define XPISTUB_DLL "\pxpistubDebug.shlb"
#else
#define XPISTUB_DLL "\pxpistub.shlb"
#endif

static Boolean bMaxDiscovered = false;
static Boolean bProgMsgInit = false;

void 		
ProgressMsgInit()
{
	Rect	r;
	
	if (gControls->tw->allProgressMsg)
	{
		HLock((Handle)gControls->tw->allProgressMsg);
		SetRect(&r, (*gControls->tw->allProgressMsg)->viewRect.left,
				(*gControls->tw->allProgressMsg)->viewRect.top,
				(*gControls->tw->allProgressMsg)->viewRect.right,
				(*gControls->tw->allProgressMsg)->viewRect.bottom );
		HUnlock((Handle)gControls->tw->allProgressMsg);
		
		TESetText("", 0, gControls->tw->allProgressMsg);
		
		EraseRect(&r);
	}
	
	bProgMsgInit = true;
	
	return;
}

void 	
xpicbProgress(const char* msg, PRInt32 val, PRInt32 max)
{
	Boolean indeterminateFlag = false;
	Rect	r;
	Str255	installingStr, fileStr, ofStr;
	char	*valStr, *maxStr, *leaf;
	StringPtr	pleaf = nil;
	long		last;
	GrafPtr oldPort;
	GetPort(&oldPort);
	
	if (gWPtr)
	{
		SetPort(gWPtr);
		if (gControls->tw->xpiProgressBar)
		{
			if (!bProgMsgInit)
				ProgressMsgInit();
				
			if (max!=0 && !bMaxDiscovered)
			{
				SetControlData(gControls->tw->xpiProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag,
								sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
				SetControlMaximum(gControls->tw->xpiProgressBar, max);
				if (gControls->tw->allProgressBar)
					SetControlValue(gControls->tw->allProgressBar,
									GetControlValue(gControls->tw->allProgressBar)+1);
									
				bMaxDiscovered = true;
			}
			else if (!bMaxDiscovered)
			{
				if (gWPtr)
					IdleControls(gWPtr);
				if (gControls->tw->xpiProgressMsg)
				{	
					HLock((Handle)gControls->tw->xpiProgressMsg);
					SetRect(&r, (*gControls->tw->xpiProgressMsg)->viewRect.left,
								(*gControls->tw->xpiProgressMsg)->viewRect.top,
								(*gControls->tw->xpiProgressMsg)->viewRect.right,
								(*gControls->tw->xpiProgressMsg)->viewRect.bottom );
					HUnlock((Handle)gControls->tw->xpiProgressMsg);
					if (msg)
					{
						EraseRect(&r);
						
						GetResourcedString(installingStr, rInstList, sProcessing);
						leaf = strrchr(msg, ':');
						if (leaf)
						{
							pleaf = CToPascal(leaf+1);
							pstrcat(installingStr, pleaf);
						}
						else
						{
							installingStr[0] = 1;
							installingStr[1] = ' ';
						}
						
						last = installingStr[0] + 1;
						if (last > 255)
							last = 255;
						installingStr[last] = 0;
						
						if (installingStr[0] > 0)	
							TESetText(&installingStr[1], installingStr[0], gControls->tw->xpiProgressMsg);
							
						TEUpdate(&r, gControls->tw->xpiProgressMsg);
						
						if (pleaf)
							DisposePtr((Ptr) pleaf);
					}
				}
			}
			
			if (bMaxDiscovered)
			{
				SetControlValue(gControls->tw->xpiProgressBar, val);
				
				if (gControls->tw->xpiProgressMsg)
				{
					GetResourcedString(installingStr, rInstList, sProcessing);
					GetResourcedString(fileStr, rInstList, sFileSp);
					GetResourcedString(ofStr, rInstList, sSpOfSp);
					HLock((Handle)gControls->tw->xpiProgressMsg);
					SetRect(&r, (*gControls->tw->xpiProgressMsg)->viewRect.left,
								(*gControls->tw->xpiProgressMsg)->viewRect.top,
								(*gControls->tw->xpiProgressMsg)->viewRect.right,
								(*gControls->tw->xpiProgressMsg)->viewRect.bottom );
					HUnlock((Handle)gControls->tw->xpiProgressMsg);
									
					valStr = ltoa(val);
					maxStr = ltoa(max);
				
					EraseRect(&r);
					if (valStr && maxStr)
					{
						TESetText(&installingStr[1], installingStr[0], gControls->tw->xpiProgressMsg);
						TEInsert(&fileStr[1], fileStr[0], gControls->tw->xpiProgressMsg);
						TEInsert(valStr, strlen(valStr), gControls->tw->xpiProgressMsg);
						TEInsert(&ofStr[1], ofStr[0], gControls->tw->xpiProgressMsg);
						TEInsert(maxStr, strlen(maxStr), gControls->tw->xpiProgressMsg);
						TEUpdate(&r, gControls->tw->xpiProgressMsg);
					}
					
					if (valStr)
						free(valStr);
					if (maxStr)
						free(maxStr);
				}
			}
		}
	}
	
	SetPort(oldPort);
	return;
}

OSErr
RunAllXPIs(short xpiVRefNum, long xpiDirID, short vRefNum, long dirID)
{
	OSErr 				err = noErr;
	FSSpec				tgtDirSpec, xpiStubDirSpec, xpiSpec;
	XPI_InitProc		xpi_initProc;
	XPI_InstallProc		xpi_installProc;
	XPI_ExitProc		xpi_exitProc;
	CFragConnectionID	connID;
	nsresult 			rv = NS_OK;
	StringPtr			pcurrArchive;
	int					i, len, compsDone = 0, numXPIs, currXPICount = 0, instChoice = gControls->opt->instChoice-1;
	Boolean				isCurrXPI = false, indeterminateFlag = false;
	Str255				installingStr, msg;
	StringPtr           pTargetSubfolder = "\p"; /* subfolder is optional so init empty */
	long                dummyDirID = 0;
	
    err = FSMakeFSSpec(vRefNum, dirID, kViewerFolder, &xpiStubDirSpec); /* xpistub dir */
    if (gControls->cfg->targetSubfolder)
    {
        HLock(gControls->cfg->targetSubfolder);
        if (*gControls->cfg->targetSubfolder)
            pTargetSubfolder = CToPascal(*gControls->cfg->targetSubfolder);
        HUnlock(gControls->cfg->targetSubfolder);
    }
    err = FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, /* program dir */
	                   pTargetSubfolder, &tgtDirSpec);
	if (err == fnfErr)
	    err = FSpDirCreate(&tgtDirSpec, smSystemScript, &dummyDirID);
	    
	if (err != noErr)
    {
        ErrorHandler(err, nil);
        return err;
    }

	ERR_CHECK_MSG(LoadXPIStub(&xpi_initProc, &xpi_installProc, &xpi_exitProc, &connID, xpiStubDirSpec, msg), err, msg);
	XPI_ERR_CHECK(xpi_initProc( xpiStubDirSpec, tgtDirSpec, NULL, xpicbProgress ));
	    
	// init overall xpi indicator
	numXPIs = CountSelectedXPIs();
	if (gControls->tw->allProgressBar)
	{
		SetControlMaximum(gControls->tw->allProgressBar, numXPIs*2); // numXPIs * 2 so that prog bar moves more
		SetControlData(gControls->tw->allProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag,
						sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
		SetControlValue(gControls->tw->allProgressBar, 0);
		Draw1Control(gControls->tw->allProgressBar);
	}
	
	if (gControls->tw->xpiProgressBar)
		ShowControl(gControls->tw->xpiProgressBar);
		
	// enumerate through all .xpi's
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
	    BreathFunc();
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected, or not custom setup type
			// add file to buffer
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{
				// if LAUNCHAPP and DOWNLOAD_ONLY attr wasn't set
			 	if (!gControls->cfg->comp[i].launchapp && !gControls->cfg->comp[i].download_only)
				{
					HLock(gControls->cfg->comp[i].archive);
					pcurrArchive = CToPascal(*gControls->cfg->comp[i].archive);
					HUnlock(gControls->cfg->comp[i].archive);
		
					isCurrXPI = IsArchiveXPI(pcurrArchive);
								
					err = FSMakeFSSpec(xpiVRefNum, xpiDirID, pcurrArchive, &xpiSpec);
					if (err==noErr && isCurrXPI)
					{
						// update package display name
						if (gControls->tw->allProgressMsg)
						{
							ProgressMsgInit();
							GetResourcedString(installingStr, rInstList, sInstalling);
							TEInsert(&installingStr[1], installingStr[0], gControls->tw->allProgressMsg);
							HLock(gControls->cfg->comp[i].shortDesc);
							len = strlen(*gControls->cfg->comp[i].shortDesc);
							TEInsert(*gControls->cfg->comp[i].shortDesc, (len>64?64:len), gControls->tw->allProgressMsg);
							HUnlock(gControls->cfg->comp[i].shortDesc);
						}
						
						err = RunXPI(xpiSpec, &xpi_installProc);
						BreathFunc();
						if (err != NS_OK)
						    break;
						
						// update progess bar
						if (gControls->tw->allProgressBar)
							SetControlValue(gControls->tw->allProgressBar,
											GetControlValue(gControls->tw->allProgressBar)+1);
					}
					if (pcurrArchive)
						DisposePtr((Ptr) pcurrArchive);
				}
				compsDone++;
			}
		}
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
	}
	
	xpi_exitProc();	
	UnloadXPIStub(&connID);
	
    if (pTargetSubfolder)
	    DisposePtr((Ptr)pTargetSubfolder);
	return err;
}

OSErr
RunXPI(FSSpec& aXPI, XPI_InstallProc *xpi_installProc)
{	
	nsresult	rv;
	OSErr	 	err = noErr;
	long		flags = 0x1000; /* XPI_NO_NEW_THREAD = 0x1000 from nsISoftwareUpdate.h */
	Boolean		indeterminateFlag = true;
	
	XPI_ERR_CHECK((*xpi_installProc)( aXPI, "", flags ));
	
	/* reset progress bar to barber poll */
	bMaxDiscovered = false;
	if (gControls->tw->xpiProgressBar)
		SetControlData(gControls->tw->xpiProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag,
						sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
	if (NS_FAILED(rv))
		return -1;


	return err;
}

int
CountSelectedXPIs()
{
	int i, instChoice = gControls->opt->instChoice - 1, compsDone = 0, numXPIs = 0;
	
	// enumerate through all .xpi's
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected, or not custom setup type
			// add file to buffer
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{
				// if LAUNCHAPP and DOWNLOAD_ONLY attr wasn't set
			 	if (!gControls->cfg->comp[i].launchapp && !gControls->cfg->comp[i].download_only)
					numXPIs++;
			}
		}
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
	}
	
	return numXPIs;
}

/*-------------------------------------------------------------------
 *   XPI Stub Load/Unload
 *-------------------------------------------------------------------*/
OSErr
LoadXPIStub(XPI_InitProc* pfnInit, XPI_InstallProc* pfnInstall, XPI_ExitProc* pfnExit, 
			CFragConnectionID* connID, FSSpec& aTargetDir, Str255 errName)
{
	OSErr				err;
	FSSpec				fslib;
	Str63 				fragName = XPISTUB_DLL;
	Ptr					mainAddr, symAddr;
	CFragSymbolClass	symClass;
	long				tgtDirID;
	Boolean 			isDir;
	
	err = FSpGetDirectoryID( &aTargetDir, &tgtDirID, &isDir );
	if (err!=noErr)
		return err;
	else if (!isDir)
		return paramErr;
		
	err = FSMakeFSSpec(aTargetDir.vRefNum, tgtDirID, fragName, &fslib);
	if (err!=noErr)
		return err;
		
	err = GetDiskFragment(&fslib, 0, kCFragGoesToEOF, nil, /*kPrivateCFragCopy*/kReferenceCFrag, connID, &mainAddr, errName);
										   
	if ( err == noErr && *connID != NULL)
	{
		ERR_CHECK_RET( FindSymbol(*connID, "\pXPI_Init", &symAddr, &symClass), err );
		*pfnInit = (XPI_InitProc) symAddr;
		
		ERR_CHECK_RET( FindSymbol(*connID, "\pXPI_Install", &symAddr, &symClass), err);
		*pfnInstall = (XPI_InstallProc) symAddr;
		
		ERR_CHECK_RET( FindSymbol(*connID, "\pXPI_Exit", &symAddr, &symClass), err);
		*pfnExit = (XPI_ExitProc) symAddr;
	}
	
	return err;
}

Boolean
UnloadXPIStub(CFragConnectionID* connID)
{
    if ((connID != NULL) && (*connID != NULL))
	{
		CloseConnection(connID);
		*connID = NULL;
   	    return true;
	}
	
	return false; 
}

Boolean 
IsArchiveXPI(StringPtr archive)
{
	long 	len = *archive;
	char 	*extStart = (char*)(&archive[1] + len - 4); /* extension length of ".xpi" = 4 */
	
	if (strncmp(extStart, ".xpi", 4) == 0)
		return true;
	
	return false;
}
