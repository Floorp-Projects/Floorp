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


#ifndef _MIW_H_
	#include "MacInstallWizard.h"
#endif

#define XP_MAC 1
#include "xpistub.h"

/*-----------------------------------------------------------*
 *   XPInstall Glue
 *-----------------------------------------------------------*/

/* XPI Stub Entry Points */
typedef		nsresult (*XPI_InitProc)(const FSSpec& targetDir,
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

#define XPCOM_ERR_CHECK(_call) 	\
rv = _call;						\
if (NS_FAILED(rv))				\
{								\
	ErrorHandler();				\
	return rv;						\
}

#define XPISTUB_DLL "\pxpistubDebug.shlb";
 

void 		
xpicbStart(const char *URL, const char* UIName)
{
	// TO DO
	// SysBeep(10);
	return;
}

void 	
xpicbProgress(const char* msg, PRInt32 val, PRInt32 max)
{
	// TO DO
	// SysBeep(10);
	return;
}

void 		
xpicbFinal(const char *URL, PRInt32 finalStatus)
{
	// TO DO
	// SysBeep(10);
	return;
}

OSErr
RunXPI(FSSpec& aXPI, FSSpec& aTargetDir)
{	
	nsresult			rv;
	OSErr	 			err = noErr;
	long				flags = 0xFFFF;
	XPI_InitProc		xpi_initProc;
	XPI_InstallProc		xpi_installProc;
	XPI_ExitProc		xpi_exitProc;
	CFragConnectionID	connID;
	
	ERR_CHECK_RET(LoadXPIStub(&xpi_initProc, &xpi_installProc, &xpi_exitProc, &connID, aTargetDir), err);
	
	XPCOM_ERR_CHECK(xpi_initProc( aTargetDir, xpicbStart, xpicbProgress, xpicbFinal ));

	XPCOM_ERR_CHECK(xpi_installProc( aXPI, "", flags ));
	
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
	long				currDirID;
	short				currVRefNum;	
	CFragSymbolClass	symClass;
	
	err = GetCWD(&currDirID, &currVRefNum);
	if (err!=noErr)
		return false;
		
	// TO DO use aTargetDir to load XPISTUB_DLL
	
	err = FSMakeFSSpec(currVRefNum, currDirID, fragName, &fslib);
	if (err!=noErr)
		return err;
		
	err = GetDiskFragment(&fslib, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy/*kReferenceCFrag*/, connID, &mainAddr, errName);
	if (err != noErr)
		return err;
										   
	if ( *connID != NULL)
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
	if (*connID != NULL)
	{
		CloseConnection(connID);
		*connID = NULL;
	}
	else
		return false;
	
	return true; 
}