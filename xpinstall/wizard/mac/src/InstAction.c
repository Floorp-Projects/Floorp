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

pascal void* Install(void* unused)
{	
	short			vRefNum, srcVRefNum;
	long			dirID, srcDirID, modulesDirID;
	OSErr 			err;
	FSSpec			idiSpec, coreFileSpec, redirectSpec;
#ifdef MIW_DEBUG
	FSSpec			tmpSpec;
#endif
	SDISTRUCT		sdistruct;
	Str255			pIDIfname, pModulesDir;
	StringPtr		coreFile;
	THz				ourHZ;
	Boolean 		isDir = false, bCoreExists = false;
	GrafPtr			oldPort;

#ifndef MIW_DEBUG
	/* get "Temporary Items" folder path */
	ERR_CHECK_RET(FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder, &vRefNum, &dirID), (void*)0);
#else
	/* for DEBUG builds dump downloaded items in "<currProcessVolume>:Temp NSInstall:" */
	vRefNum = gControls->opt->vRefNum;
	err = FSMakeFSSpec( vRefNum, 0, TEMP_DIR, &tmpSpec );
	if (err != noErr)
	{
		err = FSpDirCreate(&tmpSpec, smSystemScript, &dirID);
		if (err != noErr)
		{
			ErrorHandler();
			return (void*)0;
		}
	}
	else
	{
		err = FSpGetDirectoryID( &tmpSpec, &dirID, &isDir );
		if (!isDir || err!=noErr)
		{
			ErrorHandler();
			return (void*)0;
		}
	}	
#endif /* MIW_DEBUG */
	
	err = GetCWD(&srcDirID, &srcVRefNum);
	if (err != noErr)
	{
		ErrorHandler();
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
		/* download location is same as extraction location */
		srcVRefNum = vRefNum;
		srcDirID = dirID;
		
		GetIndString(pIDIfname, rStringList, sTempIDIName);
	
		/* preparing to download */
		gSDDlg = true;
		ourHZ = GetZone();
		GetPort(&oldPort);

#if MOZILLA == 0
		if (gControls->cfg->redirect.numURLs > 0)
		{
			if (DownloadRedirect(vRefNum, dirID, &redirectSpec))
				ParseRedirect(&redirectSpec);
		}
#endif /* MOZILLA == 0 */

		/* generate idi */
		if (!GenerateIDIFromOpt(pIDIfname, dirID, vRefNum, &idiSpec))
		{
			ErrorHandler();
			return (void*) nil;
		}		
	
		/* populate SDI struct */
		sdistruct.dwStructSize 	= sizeof(SDISTRUCT);
		sdistruct.fsIDIFile 	= idiSpec;
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
				ErrorHandler();
				if (coreFile)
					DisposePtr((Ptr)coreFile);
				return (void*) nil;
			}
						
			/* run all .xpi's through XPInstall */
			err = RunAllXPIs(srcVRefNum, srcDirID, vRefNum, dirID);
			if (err!=noErr)
				ErrorHandler();
				
			CleanupExtractedFiles(vRefNum, dirID);
			
			if (!bCoreExists)
			{
				err = FSpDelete(&coreFileSpec);
#ifdef MIW_DEBUG
				if (err!=noErr) SysBeep(10); 
#endif
			}
		}
		
		if (coreFile)
			DisposePtr((Ptr)coreFile);
	}
	
	/* launch the downloaded apps who had the LAUNCHAPP attr set */
	LaunchApps(vRefNum, dirID);
	
	/* run apps that were set in RunAppsX sections */
	if (gControls->cfg->numRunApps > 0)
		RunApps();
	 
#if MOZILLA == 0
	/* cleanup downloaded .xpis */
	DeleteXPIs(vRefNum, dirID);  /* temp folder location is supplied */
#endif

	/* wind down app */
	gDone = true;
	
	return (void*) 0;
}

Boolean
DownloadRedirect(short vRefNum, long dirID, FSSpecPtr redirectINI)
{
	Boolean 	bSuccess = true;
	char 		*buf = NULL, *idx = NULL, *leaf = NULL;;
	int			i;
	FSSpec		getRedirectIDI;
	short		refNum;
	long		count;
	SDISTRUCT	sdistruct;
	StringPtr	pLeaf = NULL;
	OSErr		err = noErr;
	
	buf = NewPtrClear(2048);
	if (!buf)
		return false;
		
	/* generate IDI */
	
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
	
	/* iterate <n>=URLn */
	for (i = 0; i < gControls->cfg->redirect.numURLs; i++)
	{
		idx = ltoa(i);
		
		strcat(buf, idx);
		strcat(buf, "=");
		HLock(gControls->cfg->redirect.url[i]);
		strcat(buf, *(gControls->cfg->redirect.url[i]));
		HUnlock(gControls->cfg->redirect.url[i]);
		strcat(buf, "\r");
		
		if (idx)
			free(idx);
		idx = NULL;
	}
	
	/* write out buffer to temp location */
	err = FSMakeFSSpec(vRefNum, dirID, "\pGetRedirect.idi", &getRedirectIDI);
	if (err == noErr)
		FSpDelete(&getRedirectIDI);
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
	
	/* verify redirect.ini existence */
	HLock(gControls->cfg->redirect.url[0]);
	leaf = strrchr(*(gControls->cfg->redirect.url[0]), '/');
	if (!leaf)
	{
		bSuccess = false;
		goto BAIL;
	}
	pLeaf = CToPascal(leaf+1);
	HUnlock(gControls->cfg->redirect.url[0]);
	
	err = FSMakeFSSpec(vRefNum, dirID, pLeaf, redirectINI);
	if (err != noErr)
		bSuccess = false;
	
BAIL:
	if (buf)
		DisposePtr((Ptr)buf);
	if (pLeaf)
		DisposePtr((Ptr)pLeaf);
		
	return bSuccess;
}

void
ParseRedirect(FSSpecPtr redirectINI)
{
	short 		fileRefNum;
	Boolean		bSuccess = false;
	OSErr 		err = noErr;
	long		dataSize;
	char 		*text = NULL, *cDomain = NULL, *cSection = NULL, *cIndex = NULL;
	Str255		pSection, pDomainRoot;
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
		text = (char*) NewPtrClear(dataSize);
		if (!text)
			return;
			
        err = FSRead(fileRefNum, &dataSize, text);
        if (err != noErr)
        	goto BAIL;
    }
	FSClose(fileRefNum);
		 
	/* parse text for selected site replacing selected 
	 * site with parsed text (new site domain) 
	 */
	if (gControls->cfg->numSites > 0)
	{
		siteIndex = gControls->opt->siteChoice - 1;
		
		GetIndString(pSection, rParseKeys, sSiteSelector);
		GetIndString(pDomainRoot, rParseKeys, sDomain);
		cDomain = NewPtrClear(pDomainRoot[0] + 4);
		cSection = PascalToC(pSection);
		if (!cSection || !cDomain)
			goto BAIL;
		
		cIndex = ltoa(siteIndex);
		if (!cIndex) 
			goto BAIL;
		
		memset(cDomain, 0 , pDomainRoot[0] + 4);
		strncpy(cDomain, (char*)&pDomainRoot[1], pDomainRoot[0]);
		strcat(cDomain, cIndex);
		
		domainH = NewHandleClear(kValueMaxLen);
		if (!domainH ) 
			goto BAIL;
		if (FillKeyValueUsingName(cSection, cDomain, domainH, text))
		{	
			if (gControls->cfg->site[siteIndex].domain)
				DisposeHandle(gControls->cfg->site[siteIndex].domain);
			gControls->cfg->site[siteIndex].domain = NewHandleClear(kValueMaxLen);
			HLock(domainH);
			HLock(gControls->cfg->site[siteIndex].domain);
			strcpy(*(gControls->cfg->site[siteIndex].domain), *domainH);
			HUnlock(domainH);
			HUnlock(gControls->cfg->site[siteIndex].domain);
		}
		if (domainH)
			DisposeHandle(domainH);
	}
	
BAIL:
	if (text)
		DisposePtr((Ptr) text);
	if (cDomain)
		DisposePtr((Ptr) cDomain);
	if (cSection)
		DisposePtr((Ptr) cSection);
	if (cIndex)
		free(cIndex);
}

Boolean 	
GenerateIDIFromOpt(Str255 idiName, long dirID, short vRefNum, FSSpec *idiSpec)
{
	Boolean bSuccess = true;
	OSErr 	err;
	short	refNum, instChoice;
	long 	count, compsDone, i, j, len;
	char 	ch, siteDomain[255];
	Ptr 	buf, keybuf, fnum;
	Str255	pfnum, pkeybuf;
	FSSpec	fsExists;
	StringPtr	pcurrArchive = 0;
	
	err = FSMakeFSSpec(vRefNum, dirID, idiName, idiSpec);
	if ((err != noErr) && (err != fnfErr))
	{
		ErrorHandler();
		return false;
	}
	err = FSpCreate(idiSpec, 'NSCP', 'TEXT', smSystemScript);
	if ( (err != noErr) && (err != dupFNErr))
	{
		ErrorHandler();
		return false;
	}
	ERR_CHECK_RET(FSpOpenDF(idiSpec, fsRdWrPerm, &refNum), false);
	
	// setup buffer to at least 8K
	buf = NULL;
	buf = NewPtrClear(kGenIDIFileSize);
	if (!buf)
	{
		ErrorHandler();
		return false;
	}
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
// XXX shouldn't this be #if MOZILLA == 1
				// verify that file does not exist already
				HLock(gControls->cfg->comp[i].archive);
				pcurrArchive = CToPascal(*gControls->cfg->comp[i].archive);
				HUnlock(gControls->cfg->comp[i].archive);				
				err = FSMakeFSSpec(vRefNum, dirID, pcurrArchive, &fsExists);
				
				// if file doesn't exist
				if (err == fnfErr)
				{
// XXX shouldn't this be #endif /* MOZILLA == 1 */
					// get file number from STR# resource
					GetIndString(pfnum, rIndices, compsDone+1);
					fnum = PascalToC(pfnum);
				
					// construct through concatenation [File<num>]\r
					GetIndString(pkeybuf, rIDIKeys, sFile);
					keybuf = PascalToC(pkeybuf);
					ch = '[';
					strncat(buf, &ch, 1);
					strncat(buf, keybuf, strlen(keybuf));
					strncat(buf, fnum, strlen(fnum));
					DisposePtr(keybuf);
				
					keybuf = NewPtrClear(3);
					keybuf = "]\r";
					strncat(buf, keybuf, strlen(keybuf));
					if (keybuf)
						DisposePtr(keybuf);

					// write out \tdesc=
					GetIndString(pkeybuf, rIDIKeys, sDesc);
					keybuf = PascalToC(pkeybuf);
					ch = '\t';								
					strncat(buf, &ch, 1);					// \t
					strncat(buf, keybuf, strlen(keybuf));	// \tdesc
					ch = '=';								// \tdesc=
					strncat(buf, &ch, 1);
					if (keybuf)
						DisposePtr(keybuf);
				
					// write out gControls->cfg->comp[i].shortDesc\r
					HLock(gControls->cfg->comp[i].shortDesc);
					strncat(buf, *gControls->cfg->comp[i].shortDesc, strlen(*gControls->cfg->comp[i].shortDesc));				
					HUnlock(gControls->cfg->comp[i].shortDesc);
					ch = '\r';
					strncat(buf, &ch, 1);
			
					// if [Site Selector] sections exists
					if (gControls->cfg->numSites > 0)
					{
						memset(siteDomain, 0 , 255);
						
						// get domain of selected site
						HLock(gControls->cfg->site[gControls->opt->siteChoice-1].domain);
						len = strlen(*(gControls->cfg->site[gControls->opt->siteChoice-1].domain));
						strncpy(siteDomain, *(gControls->cfg->site[gControls->opt->siteChoice-1].domain),
								(len < 255) ? len : 255);
						HUnlock(gControls->cfg->site[gControls->opt->siteChoice-1].domain);
					}
							
					// iterate over gControls->cfg->comp[i].numURLs
					for (j=0; j<gControls->cfg->comp[i].numURLs; j++)
					{
						// write out \tnumURLs+1= from STR# resource
						GetIndString(pkeybuf, rIndices, j+2); // j+2 since 1-based idx, not 0-based
						keybuf = PascalToC(pkeybuf);
						ch = '\t';
						strncat(buf, &ch, 1);					// \t
						strncat(buf, keybuf, strlen(keybuf));	// \t<n>
						ch = '=';
						strncat(buf, &ch, 1);					// \t<n>=
						if (keybuf)
							DisposePtr(keybuf);
							
						// if [Site Selector] section exists
						if (gControls->cfg->numSites > 0 && j == 0)
						{
							// use selected DomainX to replace Domain0 im curr ComponentX section
							strncat(buf, siteDomain, strlen(siteDomain));
						}
						else
						{
							// get domain for this index
							HLock(gControls->cfg->comp[i].domain[j]);
							strncat(buf, *gControls->cfg->comp[i].domain[j], strlen(*gControls->cfg->comp[i].domain[j]));
							HUnlock(gControls->cfg->comp[i].domain[j]);
						}

						// tack on server path for this index
						HLock(gControls->cfg->comp[i].serverPath[j]);
						strncat(buf, *gControls->cfg->comp[i].serverPath[j], strlen(*gControls->cfg->comp[i].serverPath[j]));
						HUnlock(gControls->cfg->comp[i].serverPath[j]);						
						
						// tack on 'archive\r'
						HLock(gControls->cfg->comp[i].archive);
						strncat(buf, *gControls->cfg->comp[i].archive, strlen(*gControls->cfg->comp[i].archive));
						HUnlock(gControls->cfg->comp[i].archive);
						ch = '\r';
						strncat(buf, &ch, 1);
					}
					if (fnum)
						DisposePtr(fnum);
					compsDone++;
// XXX shouldn't this be #if MOZILLA == 1
				}
// XXX shouldn't this be #endif /* MOZILLA == 1 */
			}
		}
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
	}			
	
	// terminate by entering Netscape Install section
	GetIndString(pkeybuf, rIDIKeys, sNSInstall);
	keybuf = PascalToC(pkeybuf);
	ch = '[';
	strncat(buf, &ch, 1);					// [
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
	
	// close file
	ERR_CHECK_RET(FSClose(refNum), false)
	
	if (buf)
		DisposePtr(buf);
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
	Ptr					appSigStr, docStr;	
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
		ErrorHandler();
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
