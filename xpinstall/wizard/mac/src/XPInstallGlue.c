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

#define XP_MAC 1
#include "xpistub.h"

/*-----------------------------------------------------------*
 *   XPInstall Glue
 *-----------------------------------------------------------*/

/* XPI Stub Entry Points */
typedef		nsresult (*XPI_InitProc)(const FSSpec& aXPIStubDir, const FSSpec& aProgramDir,
									 pfnXPIStart startCB, 
								 	 pfnXPIProgress progressCB, 
								 	 pfnXPIFinal finalCB);
typedef 	nsresult (*XPI_InstallProc)(const FSSpec& file, const char* args,long flags);
typedef		nsresult (*XPI_ExitProc)();

/* XPI Stub Load/Unload */
OSErr		LoadXPIStub(XPI_InitProc* pfnInit, 
						XPI_InstallProc* pfnInstall, 
						XPI_ExitProc* pfnExit, 
						CFragConnectionID* connID,
						FSSpec& aTargetDir);
Boolean		UnloadXPIStub(CFragConnectionID* connID);

/* Progress Bar Callbacks */
void 		xpicbStart(const char *URL, const char* UIName);
void	 	xpicbProgress(const char* msg, PRInt32 val, PRInt32 max);
void 		xpicbFinal(const char *URL, PRInt32 finalStatus);

#define XPI_ERR_CHECK(_call) 	\
rv = _call;						\
if (NS_FAILED(rv))				\
{								\
	ErrorHandler();				\
	return rv;						\
}

#ifdef DEBUG
#define XPISTUB_DLL "\pxpistubDebug.shlb"	
#else
#define XPISTUB_DLL "\pxpistub.shlb"
#endif

static Boolean bMaxDiscovered = false;

void 		
xpicbStart(const char *URL, const char* UIName)
{
	Rect	r;
	Str255	installingStr;
	
	if (gControls->tw->progressMsg)
	{
		HLock((Handle)gControls->tw->progressMsg);
		SetRect(&r, (*gControls->tw->progressMsg)->viewRect.left,
				(*gControls->tw->progressMsg)->viewRect.top,
				(*gControls->tw->progressMsg)->viewRect.right,
				(*gControls->tw->progressMsg)->viewRect.bottom );
		HUnlock((Handle)gControls->tw->progressMsg);
		
		/* Installing <UIName> */
		GetIndString(installingStr, rStringList, sInstalling);
		
		EraseRect(&r);
		if (installingStr[0] > 0)
			TESetText(&installingStr[1], installingStr[0], gControls->tw->progressMsg);
		if (UIName)
			TEInsert(UIName, strlen(UIName), gControls->tw->progressMsg);
		TEUpdate(&r, gControls->tw->progressMsg);
	}
	
	return;
}

void 	
xpicbProgress(const char* msg, PRInt32 val, PRInt32 max)
{
	Boolean indeterminateFlag = false;
	Rect	r;
	Str255	installingStr, fileStr, ofStr;
	char	*valStr, *maxStr;
	GrafPtr oldPort;
	GetPort(&oldPort);
	
	if (gWPtr)
	{
		SetPort(gWPtr);
		if (gControls->tw->progressBar)
		{
			if (max!=0 && !bMaxDiscovered)
			{
				SetControlData(gControls->tw->progressBar, kControlNoPart, kControlProgressBarIndeterminateTag,
								sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
				SetControlMaximum(gControls->tw->progressBar, max);
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
						TESetText(msg, strlen(msg), gControls->tw->xpiProgressMsg);
						TEUpdate(&r, gControls->tw->xpiProgressMsg);
					}
				}
			}
			
			if (bMaxDiscovered)
			{
				SetControlValue(gControls->tw->progressBar, val);
				
				if (gControls->tw->xpiProgressMsg)
				{
					GetIndString(installingStr, rStringList, sInstalling);
					GetIndString(fileStr, rStringList, sFileSp);
					GetIndString(ofStr, rStringList, sSpOfSp);
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

void 		
xpicbFinal(const char *URL, PRInt32 finalStatus)
{
	// TO DO
	// SysBeep(10);
	
	bMaxDiscovered = false;
	return;
}

OSErr
RunAllXPIs(short vRefNum, long dirID)
{
	OSErr 	err = noErr;
	FSSpec	tgtDirSpec, xpiStubDirSpec, xpiSpec;

	err = FSMakeFSSpec(vRefNum, dirID, 0, &xpiStubDirSpec); /* temp dir */
	err = FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, 0, &tgtDirSpec);	/* program dir */
	
	// TO DO
	//		enumerate through all .xpi's
	err = FSMakeFSSpec(vRefNum, dirID, "\pmozilla.jar", &xpiSpec); // XXX change

	if (err==noErr)
		err =  RunXPI(xpiSpec, xpiStubDirSpec, tgtDirSpec);
	else
		ErrorHandler(); 
		
	return err;
}

OSErr
RunXPI(FSSpec& aXPI, FSSpec& aXPIStubDir, FSSpec& aTargetDir)
{	
	nsresult			rv;
	OSErr	 			err = noErr;
	long				flags = 0xFFFF;
	XPI_InitProc		xpi_initProc;
	XPI_InstallProc		xpi_installProc;
	XPI_ExitProc		xpi_exitProc;
	CFragConnectionID	connID;
	
	ERR_CHECK_RET(LoadXPIStub(&xpi_initProc, &xpi_installProc, &xpi_exitProc, &connID, aXPIStubDir), err);
	
	XPI_ERR_CHECK(xpi_initProc( aXPIStubDir, aTargetDir, xpicbStart, xpicbProgress, xpicbFinal ));

	XPI_ERR_CHECK(xpi_installProc( aXPI, "", flags ));
	
	xpi_exitProc();
	UnloadXPIStub(&connID);

	return err;
}

/*-------------------------------------------------------------------
 *   XPI Stub Load/Unload
 *-------------------------------------------------------------------*/
OSErr
LoadXPIStub(XPI_InitProc* pfnInit, XPI_InstallProc* pfnInstall, XPI_ExitProc* pfnExit, 
			CFragConnectionID* connID, FSSpec& aTargetDir)
{
	OSErr				err;
	FSSpec				fslib;
	Str63 				fragName = XPISTUB_DLL;
	Ptr					mainAddr, symAddr;
	Str255				errName;	
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