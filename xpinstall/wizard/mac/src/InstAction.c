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
 *   Install Action
 *-----------------------------------------------------------*/

static Boolean bXPIsExisted = true;

pascal void* Install(void* unused)
{	
	short			vRefNum, srcVRefNum;
	long			dirID, srcDirID, modulesDirID;
	OSErr 			err;
	FSSpec			idiSpec, coreFileSpec;
#if MOZILLA == 0
	FSSpec 			redirectSpec;
	HRESULT			dlErr;
	short           siteIndex;
#endif
#ifdef MIW_DEBUG
	FSSpec			tmpSpec;
#endif /* MIW_DEBUG */
	SDISTRUCT		sdistruct;
	Str255			pIDIfname, pModulesDir;
	StringPtr		coreFile = NULL;
	THz				ourHZ = NULL;
	Boolean 		isDir = false, bCoreExists = false;
	GrafPtr			oldPort = NULL;

#ifndef MIW_DEBUG
	/* get "Temporary Items" folder path */
	ERR_CHECK_RET(FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder, &vRefNum, &dirID), (void*)0);
#else
	/* for DEBUG builds dump downloaded items in "<currProcessVolume>:Temp NSInstall:" */
	vRefNum = gControls->opt->vRefNum;
	err = FSMakeFSSpec( vRefNum, 0, kTempFolder, &tmpSpec );
	if (err != noErr)
	{
		err = FSpDirCreate(&tmpSpec, smSystemScript, &dirID);
		if (err != noErr)
		{
			ErrorHandler(err);
			return (void*)0;
		}
	}
	else
	{
		err = FSpGetDirectoryID( &tmpSpec, &dirID, &isDir );
		if (!isDir || err!=noErr)
		{
			ErrorHandler(err);
			return (void*)0;
		}
	}	
#endif /* MIW_DEBUG */
	
	err = GetCWD(&srcDirID, &srcVRefNum);
	if (err != noErr)
	{
		ErrorHandler(err);
		return (void*)nil;
	}
	
	/* get the "Installer Modules" relative subdir */
	GetIndString(pModulesDir, rStringList, sInstModules);
	isDir = false;  /* reuse */
	modulesDirID = 0;
	GetDirectoryID(srcVRefNum, srcDirID, pModulesDir, &modulesDirID, &isDir);
	srcDirID = modulesDirID;
	
	if (!isDir || !ExistArchives(srcVRefNum, srcDirID))
	{	
		bXPIsExisted = false;
		GetIndString(pIDIfname, rStringList, sTempIDIName);
	
		/* preparing to download */
		gSDDlg = true;
		ourHZ = GetZone();
		GetPort(&oldPort);

#if MOZILLA == 0
        HLock(gControls->cfg->redirect.subpath);
		if (gControls->cfg->redirect.subpath && *(gControls->cfg->redirect.subpath))
		{
		    HUnlock(gControls->cfg->redirect.subpath);
		    
		    /* replace global URL from redirect.ini */
		    if (DownloadRedirect(vRefNum, dirID, &redirectSpec))
                ParseRedirect(&redirectSpec);
		}
		else
		{
	        HUnlock(gControls->cfg->redirect.subpath);
	        
		    /* otherwise if site selector exists, replace global URL with selected site */
	        if (gControls->cfg->numSites > 0)
	        {
		        if (gControls->cfg->globalURL)
		            DisposeHandle(gControls->cfg->globalURL);
                gControls->cfg->globalURL = NewHandleClear(kValueMaxLen);
                
                siteIndex = gControls->opt->siteChoice - 1;
	            HLock(gControls->cfg->globalURL);
	            HLock(gControls->cfg->site[siteIndex].domain);
	            strcpy(*(gControls->cfg->globalURL), *(gControls->cfg->site[siteIndex].domain));
	            HUnlock(gControls->cfg->globalURL);
	            HUnlock(gControls->cfg->site[siteIndex].domain);
		    }
	    }
#endif /* MOZILLA == 0 */

		/* generate idi */
		if (!GenerateIDIFromOpt(pIDIfname, dirID, vRefNum, &idiSpec))
		{
			ErrorHandler(err);
			return (void*) nil;
		}		
	
		/* populate SDI struct */
		sdistruct.dwStructSize 	= sizeof(SDISTRUCT);
		sdistruct.fsIDIFile 	= idiSpec;
		sdistruct.dlDirVRefNum 	= srcVRefNum;
		sdistruct.dlDirID 		= srcDirID;
		sdistruct.hwndOwner    	= NULL;
	
		/* call SDI_NetInstall */
#if MOZILLA == 0	
#if SDINST_IS_DLL == 1
		dlErr = gInstFunc(&sdistruct);
#else
		dlErr = SDI_NetInstall(&sdistruct);
#endif /* SDINST_IS_DLL */
		if (dlErr != 0)
		{
		    if (dlErr != 0x800704C7)
			    ErrorHandler(dlErr);
			else 
			    gDone = true;
			return (void*) nil;
		}
#endif /* MOZILLA */

		SetPort(oldPort);
	
		if (gWPtr)
		{
			GetPort(&oldPort);

			SetPort(gWPtr);
			BeginUpdate(gWPtr);
			DrawControls(gWPtr);
			ShowLogo(true);
			UpdateTerminalWin();
			EndUpdate(gWPtr);
		
			SetPort(oldPort);
		}
		SetZone(ourHZ);
		gSDDlg = false;
	
		FSpDelete(&idiSpec);
	}
	else
		bCoreExists = true;
    /* otherwise core exists in cwd:InstallerModules, different from extraction location */

	
	/* check if coreFile was downloaded */
	HLock(gControls->cfg->coreFile);
	if (*gControls->cfg->coreFile != NULL)
	{
		coreFile = CToPascal(*gControls->cfg->coreFile);
		if (!coreFile)
			return (void*) memFullErr;
	}
	HUnlock(gControls->cfg->coreFile);
		
	if (coreFile != NULL && *coreFile > 0) /* core file was specified */
	{
		err = FSMakeFSSpec(srcVRefNum, srcDirID, coreFile, &coreFileSpec);
		if (err==noErr) /* core file was downloaded or packaged with installer */
		{
			InitProgressBar();
			
			/* extract contents of downloaded or packaged core file */
			err = ExtractCoreFile(srcVRefNum, srcDirID, vRefNum, dirID);
			if (err!=noErr) 
			{
				ErrorHandler(err);
				if (coreFile)
					DisposePtr((Ptr)coreFile);
				return (void*) nil;
			}
						
			/* run all .xpi's through XPInstall */
			err = RunAllXPIs(srcVRefNum, srcDirID, vRefNum, dirID);
			if (err!=noErr)
				ErrorHandler(err);
				
			CleanupExtractedFiles(vRefNum, dirID);
		}
		
		if (coreFile)
			DisposePtr((Ptr)coreFile);
	}
	
	/* launch the downloaded apps who had the LAUNCHAPP attr set */
	if (err == noErr)
		LaunchApps(srcVRefNum, srcDirID);
	
	/* run apps that were set in RunAppsX sections */
	if (err == noErr && gControls->cfg->numRunApps > 0)
		RunApps();
	 
#if MOZILLA == 0
	/* cleanup downloaded .xpis */
	if (!gControls->opt->saveBits  && !bXPIsExisted)
		DeleteXPIs(srcVRefNum, srcDirID);  /* "Installer Modules" folder location is supplied */
#endif

	/* wind down app */
	gDone = true;
	
	return (void*) 0;
}

#define GETRED_BUF_SIZE 512

Boolean
DownloadRedirect(short vRefNum, long dirID, FSSpecPtr redirectINI)
{
	Boolean 	bSuccess = true;
	char 		buf[GETRED_BUF_SIZE], *leaf = NULL;
	FSSpec		getRedirectIDI;
	short		refNum;
	long		count;
	SDISTRUCT	sdistruct;
	StringPtr	pLeaf = NULL;
	OSErr		err = noErr;
		
	/* generate IDI */
	memset(buf, 0, GETRED_BUF_SIZE);
	
	/*
	 [Netscape Install]
	  no_ads=true
	  silent=false
	  confirm_install=true
	  execution=false
	*/
	strcpy(buf, "[Netscape Install]\r");
	strcat(buf, "no_ads=true\r");
	strcat(buf, "silent=false\r");
	strcat(buf, "confirm_install=true\r");
	strcat(buf, "execution=false\r\r");
	
	/* [File0] */
	strcat(buf, "[File0]\r");
	strcat(buf, "desc=");
	HLock(gControls->cfg->redirect.desc);
	strcat(buf, *(gControls->cfg->redirect.desc));
	HUnlock(gControls->cfg->redirect.desc);
	strcat(buf, "\r");
	
	/* 1=URL */
	strcat(buf, "1=");

    /* get domain of selected site */
	HLock(gControls->cfg->site[gControls->opt->siteChoice-1].domain);
	strcat(buf, *(gControls->cfg->site[gControls->opt->siteChoice-1].domain));
    HUnlock(gControls->cfg->site[gControls->opt->siteChoice-1].domain);
	
	/* tack on redirect subpath (usually just the file leaf name) */
	HLock(gControls->cfg->redirect.subpath);
	strcat(buf, *(gControls->cfg->redirect.subpath));
	HUnlock(gControls->cfg->redirect.subpath);
	strcat(buf, "\r");
	
	/* write out buffer to temp location */
	err = FSMakeFSSpec(vRefNum, dirID, "\pGetRedirect.idi", &getRedirectIDI);
	if (err == noErr)
		FSpDelete(&getRedirectIDI);
	err = FSpCreate(&getRedirectIDI, 'NSCP', 'TEXT', smSystemScript);
	if ((err != noErr) && (err != dupFNErr))
	{
		ErrorHandler(err);
		return false;
	}
	err = FSpOpenDF(&getRedirectIDI, fsRdWrPerm, &refNum);
	if (err != noErr)
	{
		bSuccess = false;
		goto BAIL;
	}
	count = strlen(buf);
	if (count <= 0)
	{
		bSuccess = false;
		goto BAIL;
	}
	err = FSWrite(refNum, &count, (void*) buf);
	if (err != noErr)
		bSuccess = false;
	FSClose(refNum);
	
	if (!bSuccess)
	    goto BAIL;
	
	/* populate SDI struct */
	sdistruct.dwStructSize 	= sizeof(SDISTRUCT);
	sdistruct.fsIDIFile 	= getRedirectIDI;
	sdistruct.dlDirVRefNum 	= vRefNum;
	sdistruct.dlDirID 		= dirID;
	sdistruct.hwndOwner    	= NULL;
	
	/* call SDI_NetInstall */
#if MOZILLA == 0	
#if SDINST_IS_DLL == 1
		gInstFunc(&sdistruct);
#else
		SDI_NetInstall(&sdistruct);
#endif /* SDINST_IS_DLL */
#endif /* MOZILLA */
	
	bSuccess = false; 
	
	/* verify redirect.ini existence */
	HLock(gControls->cfg->redirect.subpath);
	leaf = strrchr(*(gControls->cfg->redirect.subpath), '/');
	if (!leaf)
	    leaf = *(gControls->cfg->redirect.subpath);
	else
	    leaf++;
	pLeaf = CToPascal(leaf);
	HUnlock(gControls->cfg->redirect.subpath);
	
	err = FSMakeFSSpec(vRefNum, dirID, pLeaf, redirectINI);
	if (err == noErr)
		bSuccess = true;
	
BAIL:
	if (!bSuccess)
	    FSMakeFSSpec(0, 0, "\p", redirectINI);

	if (pLeaf)
		DisposePtr((Ptr)pLeaf);
		
	return bSuccess;
}

void
ParseRedirect(FSSpecPtr redirectINI)
{
	short 		fileRefNum;
	OSErr 		err = noErr;
	long		dataSize;
	char 		*text = NULL, *cId = NULL, *cSection = NULL;
	Str255		pSection;
	int			siteIndex;
	Handle		domainH = NULL;
	
	/* read in text from downloaded site selector */
	err = FSpOpenDF(redirectINI, fsRdPerm, &fileRefNum);
    if (err != noErr)
        return;
	err = GetEOF(fileRefNum, &dataSize);        
	if (err != noErr)
		return;
	if (dataSize > 0)
	{
		text = (char*) NewPtrClear(dataSize + 1);
		if (!text)
			return;
			
        err = FSRead(fileRefNum, &dataSize, text);
        if (err != noErr)
        {
            FSClose(fileRefNum);
        	goto BAIL;
        }
    }
	FSClose(fileRefNum);
		 
	/* parse text for selected site replacing teh global URL 
	 * with the identifed site's URL in the redirect.ini
	 */
	if (gControls->cfg->numSites > 0)
	{
		siteIndex = gControls->opt->siteChoice - 1;
		if (!gControls->cfg->site[siteIndex].id)
		    goto BAIL;
		
		HLock(gControls->cfg->site[siteIndex].id);
		cId = NewPtrClear(strlen(*(gControls->cfg->site[siteIndex].id)) + 1); // add 1 for null termination
		strcpy(cId, *(gControls->cfg->site[siteIndex].id));
		HUnlock(gControls->cfg->site[siteIndex].id);
				
		GetIndString(pSection, rParseKeys, sSiteSelector);
		cSection = PascalToC(pSection);
		if (!cSection || !cId)
			goto BAIL;
		
		domainH = NewHandleClear(kValueMaxLen);
		if (!domainH ) 
			goto BAIL;
		if (FillKeyValueUsingName(cSection, cId, domainH, text))
		{	
			if (gControls->cfg->globalURL)
				DisposeHandle(gControls->cfg->globalURL);
			gControls->cfg->globalURL = NewHandleClear(kValueMaxLen);
			HLock(domainH);     
			HLock(gControls->cfg->globalURL);
			strcpy(*(gControls->cfg->globalURL), *domainH);
			HUnlock(domainH);   
			HUnlock(gControls->cfg->globalURL);
		}
		if (domainH)
			DisposeHandle(domainH);
	}
	
BAIL:
	if (text)
		DisposePtr((Ptr) text);
	if (cId)
		DisposePtr((Ptr) cId);
	if (cSection)
		DisposePtr((Ptr) cSection);
}

void
IfRemoveOldCore(short vRefNum, long dirID)
{
	FSSpec 	fsViewer;
	OSErr 	err = noErr;
	
	err = FSMakeFSSpec(vRefNum, dirID, kViewerFolder, &fsViewer);
	if (err == noErr)  // old core exists
		err = DeleteDirectory(fsViewer.vRefNum, fsViewer.parID, fsViewer.name);
}

#define IDI_BUF_SIZE 512

Boolean 	
GenerateIDIFromOpt(Str255 idiName, long dirID, short vRefNum, FSSpec *idiSpec)
{
	Boolean bSuccess = true;
	OSErr 	err;
	short	refNum, instChoice;
	long 	count, compsDone, i;
	char 	ch, buf[IDI_BUF_SIZE];
	Ptr 	keybuf;
	Str255	pkeybuf;
	FSSpec	fsExists;
	StringPtr	pcurrArchive = 0;
	
	err = FSMakeFSSpec(vRefNum, dirID, idiName, idiSpec);
	if ((err != noErr) && (err != fnfErr))
	{
		ErrorHandler(err);
		return false;
	}
	err = FSpCreate(idiSpec, 'NSCP', 'TEXT', smSystemScript);
	if ( (err != noErr) && (err != dupFNErr))
	{
		ErrorHandler(err);
		return false;
	}
	ERR_CHECK_RET(FSpOpenDF(idiSpec, fsRdWrPerm, &refNum), false);
	
	compsDone = 0;
	instChoice = gControls->opt->instChoice-1;
	
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
				// verify that file does not exist already
				HLock(gControls->cfg->comp[i].archive);
				pcurrArchive = CToPascal(*gControls->cfg->comp[i].archive);
				HUnlock(gControls->cfg->comp[i].archive);				
				err = FSMakeFSSpec(vRefNum, dirID, pcurrArchive, &fsExists);
				
				// if file doesn't exist
				if (err == fnfErr)
				{
				    char   fnum[12];
					// get file number 
					// fnum = ltoa(compsDone);
					sprintf(fnum, "%ld", compsDone);
					
				    memset(buf, 0, IDI_BUF_SIZE);
				    
					// construct through concatenation [File<num>]\r
					GetIndString(pkeybuf, rIDIKeys, sFile);

					ch = '[';
					strncpy(buf, &ch, 1);
                    CopyPascalStrToC(pkeybuf, buf + strlen(buf));
					strncat(buf, fnum, strlen(fnum));
					strcat(buf, "]\r");

					// write out \tdesc=
					GetIndString(pkeybuf, rIDIKeys, sDesc);
					ch = '\t';								
					strncat(buf, &ch, 1);					// \t
					CopyPascalStrToC(pkeybuf, buf + strlen(buf));
					ch = '=';								// \tdesc=
					strncat(buf, &ch, 1);
				
					// write out gControls->cfg->comp[i].shortDesc\r
					HLock(gControls->cfg->comp[i].shortDesc);
					strncat(buf, *gControls->cfg->comp[i].shortDesc, strlen(*gControls->cfg->comp[i].shortDesc));				
					HUnlock(gControls->cfg->comp[i].shortDesc);
					ch = '\r';
					strncat(buf, &ch, 1);
			         
					// write out \t0=                       // \t0=
					strcat(buf, "\t0=");
					
					// tack on URL to xpi directory         // \t0=<URL>
					HLock(gControls->cfg->globalURL);
					strcat(buf, *gControls->cfg->globalURL);
					HUnlock(gControls->cfg->globalURL);
					
					// tack on 'archive\r'                  // \t0=<URL>/archive\r
					HLock(gControls->cfg->comp[i].archive);
					strncat(buf, *gControls->cfg->comp[i].archive, strlen(*gControls->cfg->comp[i].archive));
					HUnlock(gControls->cfg->comp[i].archive);
					ch = '\r';
					strncat(buf, &ch, 1);
					
					count = strlen(buf);
	                err = FSWrite(refNum, &count, buf);
	                
					if (err != noErr)
					    goto BAIL;
					    
					compsDone++;
				}
			}
		}
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
	}			
	
	// terminate by entering Netscape Install section
	memset(buf, 0, IDI_BUF_SIZE);
	GetIndString(pkeybuf, rIDIKeys, sNSInstall);
	keybuf = PascalToC(pkeybuf);
	ch = '[';
	strncpy(buf, &ch, 1);					// [
	strncat(buf, keybuf, strlen(keybuf));	// [Netscape Install
	if (keybuf)
		DisposePtr(keybuf);
	
	keybuf = NewPtrClear(2);
	keybuf = "]\r";				
	strncat(buf, keybuf, strlen(keybuf));	// [Netscape Install]\r
	if (keybuf)
		DisposePtr(keybuf);
	
	// write out \tcore_file=<filename>\r
	AddKeyToIDI( sCoreFile, gControls->cfg->coreFile, buf );
	
	// write out \tcore_dir=<dirname>\r
	AddKeyToIDI( sCoreDir, gControls->cfg->coreDir, buf );
	
	// write out \tno_ads=<boolean>\r
	AddKeyToIDI( sNoAds, gControls->cfg->noAds, buf );
	
	// write out \tsilent=<boolean>\r
	AddKeyToIDI( sSilent, gControls->cfg->silent, buf );
	
	// write out \texecution=<boolean>\r
	AddKeyToIDI( sExecution, gControls->cfg->execution, buf );
	
	// write out \tconfirm_install=<boolean>
	AddKeyToIDI( sConfirmInstall, gControls->cfg->confirmInstall, buf );
	
	// write buf to disk
	count = strlen(buf);
	ERR_CHECK_RET(FSWrite(refNum, &count, buf), false);

BAIL:	
	// close file
	ERR_CHECK_RET(FSClose(refNum), false);
	
	return bSuccess;
}

void
AddKeyToIDI(short key, Handle val, char *ostream)
{
	Str255	pkeybuf;
	char 	*keybuf, *cval, ch;
	
	HLock(val);
	cval = *val;
	
	GetIndString(pkeybuf, rIDIKeys, key);
	keybuf = PascalToC(pkeybuf);
	ch = '\t';
	strncat(ostream, &ch, 1);					// \t
	strncat(ostream, keybuf, strlen(keybuf));	// \t<key>
	ch = '=';
	strncat(ostream, &ch, 1);					// \t<key>=
	strncat(ostream, cval, strlen(cval));		// \t<key>=<val>
	ch = '\r';
	strncat(ostream, &ch, 1);					// \t<key>=<val>\r
	
	HUnlock(val);
	
	if (keybuf)
		DisposePtr(keybuf);
}

Boolean
ExistArchives(short vRefNum, long dirID)
{
	int 		compsDone = 0, i;
	int 		instChoice = gControls->opt->instChoice - 1;
	OSErr 		err = noErr;
	StringPtr	pArchiveName;
	FSSpec		fsCurr;
	Boolean		bAllExist = true;
	
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected, or not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{
				HLock(gControls->cfg->comp[i].archive);
				pArchiveName = CToPascal(*gControls->cfg->comp[i].archive);
				HUnlock(gControls->cfg->comp[i].archive);
				
				err = FSMakeFSSpec(vRefNum, dirID, pArchiveName, &fsCurr);
				if (err != noErr)
				{
					bAllExist = false;
					if (pArchiveName)
						DisposePtr((Ptr)pArchiveName);
					break;
				}
				
				if (pArchiveName)
					DisposePtr((Ptr)pArchiveName);
			}
		}
		
		compsDone++;
	}
	
	return bAllExist;
}

void 
LaunchApps(short vRefNum, long dirID)
{
	int 				compsDone = 0, i;
	int 				instChoice = gControls->opt->instChoice-1;
	FSSpec 				fsCurrArchive, fsCurrApp;
	OSErr 				err = noErr;
	StringPtr 			pArchiveName;
	LaunchParamBlockRec	launchPB;
	
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected, or not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{
				// if the LAUNCHAPP attr was set
				if (gControls->cfg->comp[i].launchapp)
				{
					// AppleSingle decode the app
					HLock(gControls->cfg->comp[i].archive);
					pArchiveName = CToPascal(*gControls->cfg->comp[i].archive);
					HUnlock(gControls->cfg->comp[i].archive);
					
					err = FSMakeFSSpec(vRefNum, dirID, pArchiveName, &fsCurrArchive);
					if (err == noErr) /* archive exists */
					{
						err = AppleSingleDecode(&fsCurrArchive, &fsCurrApp);
						if (err == noErr) /* AppleSingle decoded successfully */
						{	
							// launch the decoded app
							launchPB.launchAppSpec = &fsCurrApp;
							launchPB.launchAppParameters = NULL;
							launchPB.launchBlockID = extendedBlock;
							launchPB.launchEPBLength = extendedBlockLen;
							launchPB.launchFileFlags = NULL;
							launchPB.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
							launchPB.launchControlFlags += launchDontSwitch;

							err = LaunchApplication( &launchPB );
#ifdef MIW_DEBUG
							if (err!=noErr) SysBeep(10);
#endif
							
						}
					}
					
					if (pArchiveName)
						DisposePtr((Ptr)pArchiveName);
				}	
			}
		}
		
		compsDone++;
	}
}

void
RunApps(void)
{
	OSErr 				err = noErr;
	int 				i;
#if 0
	Ptr					appSigStr;
#endif
	Ptr					docStr;	
	StringPtr			relAppPath;
	OSType 				appSig = 0x00000000;
	FSSpec 				app, doc;
	ProcessSerialNumber	psn;
	StringPtr			pdocName;
	unsigned short 		launchFileFlags, launchControlFlags;
	Boolean 			running = nil;
	LaunchParamBlockRec	launchPB;
	
	for (i = 0; i < gControls->cfg->numRunApps; i++)
	{	
		// convert str to ulong
		HLock(gControls->cfg->apps[i].targetApp);
#if 0
		appSigStr = *(gControls->cfg->apps[i].targetApp);
		UNIFY_CHAR_CODE(appSig, *(appSigStr), *(appSigStr+1), *(appSigStr+2), *(appSigStr+3));
		err =  FindAppUsingSig(appSig, &app, &running, &psn);
#endif
		relAppPath = CToPascal(*(gControls->cfg->apps[i].targetApp));
		err = FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, relAppPath, &app);
		HUnlock(gControls->cfg->apps[i].targetApp);
		if (err != noErr)
			continue;
		
		// if doc supplied
		HLock(gControls->cfg->apps[i].targetDoc);
		docStr = *(gControls->cfg->apps[i].targetDoc);
		if ( gControls->cfg->apps[i].targetDoc && docStr && *docStr )
		{	
			// qualify and create an FSSpec to it ensuring it exists
			pdocName = CToPascal(docStr);
			if (pdocName)
			{
				err = FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, pdocName, &doc);
			
				// launch app using doc
				if (err == noErr)
				{
					launchFileFlags = NULL;
					launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
					LaunchAppOpeningDoc(running, &app, &psn, &doc, 
										launchFileFlags, launchControlFlags);
				}
			}
			if (pdocName)
				DisposePtr((Ptr)pdocName);
		}
		// else if doc not supplied
		else
		{
			// launch app							
			launchPB.launchAppSpec = &app;
			launchPB.launchAppParameters = NULL;
			launchPB.launchBlockID = extendedBlock;
			launchPB.launchEPBLength = extendedBlockLen;
			launchPB.launchFileFlags = NULL;
			launchPB.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;

			err = LaunchApplication( &launchPB );
			
		}
		HUnlock(gControls->cfg->apps[i].targetDoc);
		
		if (relAppPath)
			DisposePtr((Ptr) relAppPath);
	}
	
	return;
}

void
DeleteXPIs(short vRefNum, long dirID)
{
	int 		compsDone = 0, i;
	int 		instChoice = gControls->opt->instChoice - 1;
	OSErr 		err = noErr;
	StringPtr	pArchiveName;
	FSSpec		fsCurr;
	
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected, or not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{
				HLock(gControls->cfg->comp[i].archive);
				pArchiveName = CToPascal(*gControls->cfg->comp[i].archive);
				HUnlock(gControls->cfg->comp[i].archive);
				
				err = FSMakeFSSpec(vRefNum, dirID, pArchiveName, &fsCurr);
				if (err == noErr)
				{
					err = FSpDelete(&fsCurr);
#ifdef MIW_DEBUG
					if (err != noErr)
						SysBeep(10);
#endif
				}
				
				if (pArchiveName)
					DisposePtr((Ptr)pArchiveName);
			}
		}
		
		compsDone++;
	}
}

void
InitProgressBar(void)
{
	Boolean	indeterminateFlag = true;
	Rect	r;
	Str255	extractingStr;
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	if (gWPtr)
	{
		SetPort(gWPtr);
		
		gControls->tw->allProgressBar = NULL;
		gControls->tw->allProgressBar = GetNewControl(rAllProgBar, gWPtr);
		
		gControls->tw->xpiProgressBar = NULL;
		gControls->tw->xpiProgressBar = GetNewControl(rPerXPIProgBar, gWPtr);
		
		if (gControls->tw->allProgressBar && gControls->tw->xpiProgressBar)
		{
			/* init overall prog indicator */
			SetControlData(gControls->tw->allProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag,
							sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
			Draw1Control(gControls->tw->allProgressBar);
			
			/* init xpi package name display */
			gControls->tw->allProgressMsg = NULL;
			HLock((Handle)gControls->tw->allProgressBar);
			SetRect(&r, (*gControls->tw->allProgressBar)->contrlRect.left,
						(*gControls->tw->allProgressBar)->contrlRect.top - 21,
						(*gControls->tw->allProgressBar)->contrlRect.right,
						(*gControls->tw->allProgressBar)->contrlRect.top - 5 );
			HUnlock((Handle)gControls->tw->allProgressBar);
			gControls->tw->allProgressMsg = TENew(&r, &r);
			if (gControls->tw->allProgressMsg)
			{
				GetIndString(extractingStr, rStringList, sExtracting);
				TEInsert(&extractingStr[1], extractingStr[0], gControls->tw->allProgressMsg);	
			}
			
			/* init per xpi prog indicator */
			SetControlData(gControls->tw->xpiProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag,
							sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
			HideControl(gControls->tw->xpiProgressBar);
			
			
			TextFace(normal);
			TextSize(9);
			TextFont(applFont);	
			
			gControls->tw->xpiProgressMsg = NULL;	/* used by XPInstall progress callback */
			HLock((Handle)gControls->tw->xpiProgressBar);
			SetRect(&r, (*gControls->tw->xpiProgressBar)->contrlRect.left,
						(*gControls->tw->xpiProgressBar)->contrlRect.top - 21,
						(*gControls->tw->xpiProgressBar)->contrlRect.right,
						(*gControls->tw->xpiProgressBar)->contrlRect.top - 5 );
			HUnlock((Handle)gControls->tw->xpiProgressBar);
			gControls->tw->xpiProgressMsg = TENew(&r, &r);
			
			TextFont(systemFont);	/* restore systemFont */
			TextSize(12);
		}
	}
	
	SetPort(oldPort);
}

Boolean
InitSDLib(void)
{
	Str255			libName, pModulesDir;
	FSSpec			libSpec;
	short			vRefNum;
	long			dirID, cwdDirID;
	Boolean			isDir = false;
	OSErr 			err;
	
	ERR_CHECK_RET(GetCWD(&cwdDirID, &vRefNum), false);
	
	/* get the "Installer Modules" relative subdir */
	GetIndString(pModulesDir, rStringList, sInstModules);
	GetDirectoryID(vRefNum, cwdDirID, pModulesDir, &dirID, &isDir);
	if (!isDir)		/* bail if we can't find the "Installer Modules" dir */
		return false;
		
	/* initialize SDI lib and struct */
	GetIndString(libName, rStringList, sSDLib);
	ERR_CHECK_RET(FSMakeFSSpec(vRefNum, dirID, libName, &libSpec), false);
	if (!LoadSDLib(libSpec, &gInstFunc, &gSDIEvtHandler, &gConnID))
	{
		ErrorHandler(eLoadLib);
		return false;
	}

	return true;
}

Boolean		
LoadSDLib(FSSpec libSpec, SDI_NETINSTALL *outSDNFunc, EventProc *outEvtProc, CFragConnectionID *outConnID)
{
	OSErr		err;
	Str255		errName;
	Ptr			mainAddr;
	Ptr			symAddr;
	CFragSymbolClass	symClass;
	
	ERR_CHECK_RET(GetDiskFragment(&libSpec, 0, kCFragGoesToEOF, nil, kReferenceCFrag, outConnID, &mainAddr, errName), false);
	
	if (*outConnID != NULL)
	{
		ERR_CHECK_RET(FindSymbol(*outConnID, "\pSDI_NetInstall", &symAddr, &symClass), false);
		*outSDNFunc = (SDI_NETINSTALL) symAddr;
		
		ERR_CHECK_RET(FindSymbol(*outConnID, "\pSDI_HandleEvent", &symAddr, &symClass), false);			
		*outEvtProc = (EventProc) symAddr;
	}
	
	return true;
}

Boolean		
UnloadSDLib(CFragConnectionID *connID)
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
