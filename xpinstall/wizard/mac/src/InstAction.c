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

#include "nsFTPConn.h"
#include "nsHTTPConn.h"

/*-----------------------------------------------------------*
 *   Install Action
 *-----------------------------------------------------------*/

static Boolean bXPIsExisted = true;
static long sCurrTotalDLSize = 0;

// info for download progress callback
static int sCurrComp = 0;
static Handle sCurrFullPath = 0;
static int sCurrFullPathLen = 0;
static char *sCurrURL = 0;
static time_t sCurrStartTime;  /* start of download of current file */

pascal void* Install(void* unused)
{	
	short			vRefNum, srcVRefNum;
	long			dirID, srcDirID, modulesDirID;
	OSErr 			err;
	FSSpec			coreFileSpec;
	short			dlErr;
	short           siteIndex;
#ifdef MIW_DEBUG
	FSSpec			tmpSpec;
#endif /* MIW_DEBUG */
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
		ourHZ = GetZone();
		GetPort(&oldPort);

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

        InitDLProgControls();
        dlErr = DownloadXPIs(srcVRefNum, srcDirID);
		if (dlErr != 0)
		{
		    ErrorHandler(dlErr);
			return (void*) nil;
		}
        ClearDLProgControls();

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
	 
	// cleanup downloaded .xpis 
	if (!gControls->opt->saveBits  && !bXPIsExisted)
		DeleteXPIs(srcVRefNum, srcDirID);  // "Installer Modules" folder location is supplied 

	/* wind down app */
	gDone = true;
	
	return (void*) 0;
}

long
ComputeTotalDLSize(void)
{
    int i, compsDone, instChoice;
    long totalDLSize = 0;
    
	compsDone = 0;
	instChoice = gControls->opt->instChoice-1;
	
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected -or- not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{    
                totalDLSize += gControls->cfg->comp[i].size;
                
                compsDone++;
            }
        }
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
    }
    
    return totalDLSize;
}

short
DownloadXPIs(short destVRefNum, long destDirID)
{
    short rv = 0;
    Handle dlPath;
    short dlPathLen = 0;
    int i, compsDone, instChoice;
        
    GetFullPath(destVRefNum, destDirID, "\p", &dlPathLen, &dlPath);
    
	compsDone = 0;
	instChoice = gControls->opt->instChoice-1;
	
	// loop through 0 to kMaxComponents
	for(i=0; i<kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) )
		{ 
			// if custom and selected -or- not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{    
			    // set up vars for dl callback to use
                sCurrComp = i;
                sCurrFullPath = dlPath;
                sCurrFullPathLen = dlPathLen;
                
                // download given full path and archive name
                rv = DownloadFile(dlPath, dlPathLen, gControls->cfg->comp[i].archive);
                if (rv != 0)
                {
                    ErrorHandler(rv);
                    break;
                }
                
                compsDone++;
            }
        }
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
    }
        
    return rv;
}

const char kHTTP[8] = "http://";
const char kFTP[7] = "ftp://";

short
DownloadFile(Handle destFolder, long destFolderLen, Handle archive)
{
    short rv = 0;
    char *URL = 0, *proxyServerURL = 0, *destFile = 0, *destFolderCopy = 0;
    int globalURLLen, archiveLen, proxyServerURLLen;
    char *ftpHost = 0, *ftpPath = 0;
    
    // make URL using globalURL
    HLock(archive);
    HLock(gControls->cfg->globalURL);
    globalURLLen = strlen(*gControls->cfg->globalURL);
    archiveLen = strlen(*archive);
    URL = (char *) malloc(globalURLLen + archiveLen + 1); // add 1 for NULL termination
    sprintf(URL, "%s%s", *gControls->cfg->globalURL, *archive);
    HUnlock(gControls->cfg->globalURL);
    
    // set up for dl progress callback
    sCurrURL = URL;
    
    // make dest file using dest folder and archive name
    HLock(destFolder);
    destFolderCopy = (char *) malloc(destFolderLen + 1);  // GetFullPath doesn't NULL terminate
    if (! destFolderCopy)
    {    
        HUnlock(destFolder);
        return eMem;
    }
    strncpy(destFolderCopy, *destFolder, destFolderLen);
    *(destFolderCopy + destFolderLen) = 0;
    HUnlock(destFolder);
    
    destFile = (char *) malloc(destFolderLen + archiveLen + 1);
    sprintf(destFile, "%s%s", destFolderCopy, *archive);
    HUnlock(archive);
        
    // was proxy info specified?
    if (gControls->opt->proxyHost && gControls->opt->proxyPort)
    {
        // make HTTP URL with "http://proxyHost:proxyPort"
        proxyServerURLLen = strlen(kHTTP) + strlen(gControls->opt->proxyHost) + 1 + 
                            strlen(gControls->opt->proxyPort) + 1;
        proxyServerURL = (char *) malloc(proxyServerURLLen);
        sprintf(proxyServerURL, "%s%s:%s", kHTTP, gControls->opt->proxyHost, gControls->opt->proxyPort);
        
        nsHTTPConn *conn = new nsHTTPConn(proxyServerURL);
        
        // set proxy info: proxied URL, username, password
        conn->SetProxyInfo(URL, gControls->opt->proxyUsername, gControls->opt->proxyPassword);
        
        // open an HTTP connection
        rv = conn->Open();
        if (rv == nsHTTPConn::OK)
        {
            sCurrStartTime = time(NULL);
            rv = conn->Get(DLProgressCB, destFile);
            conn->Close();
        }
    }
        
    // else do we have an HTTP URL? 
    else if (strncmp(URL, kHTTP, strlen(kHTTP)) == 0)
    {
        // open an HTTP connection
        nsHTTPConn *conn = new nsHTTPConn(URL);
        
        rv = conn->Open();
        if (rv == nsHTTPConn::OK)
        {
            sCurrStartTime = time(NULL);
            rv = conn->Get(DLProgressCB, destFile);
            conn->Close();
        }
    }
    
    // else do we have an FTP URL?
    else if (strncmp(URL, kFTP, strlen(kFTP)) == 0)
    {
        rv = ParseFTPURL(URL, &ftpHost, &ftpPath);
        if ((0 == strlen(ftpHost)) || (0 == strlen(ftpPath)))
        {
            rv = nsHTTPConn::E_MALFORMED_URL;
        }
        else
        {
            // open an FTP connection
            nsFTPConn *conn = new nsFTPConn(ftpHost);
            
            rv = conn->Open();
            if (rv == nsFTPConn::OK)
            {
                sCurrStartTime = time(NULL);
                rv = conn->Get(ftpPath, destFile, nsFTPConn::BINARY, 1, DLProgressCB);
                conn->Close();
            }
        }
        if (ftpHost)
            free(ftpHost);
        if (ftpPath)
            free(ftpPath);
    }
        
    // else not supported so report an error
    else
        rv = nsHTTPConn::E_MALFORMED_URL;
        
    return rv;
}

int 
ParseFTPURL(char *aURL, char **aHost, char **aPath)
{
    char *pos, *nextSlash, *nextColon, *end, *hostEnd;
    int protoLen = strlen(kFTP);

    if (!aURL || !aHost || !aPath)
        return -1;

    if (strncmp(aURL, kFTP, protoLen) != 0)
        return nsHTTPConn::E_MALFORMED_URL;

    pos = aURL + protoLen;
    nextColon = strchr(pos, ':');
    nextSlash = strchr(pos, '/');

    // only host in URL, assume '/' for path
    if (!nextSlash)
    {
        int copyLen;
        if (nextColon)
            copyLen = nextColon - pos;
        else
            copyLen = strlen(pos);

        *aHost = (char *) malloc(copyLen + 1); // to NULL terminate
        if (!aHost)
            return eMem;
        memset(*aHost, 0, copyLen + 1);
        strncpy(*aHost, pos, copyLen);

        *aPath = (char *) malloc(2);
        strcpy(*aPath, "/");

        return 0;
    }
    
    // normal parsing: both host and path exist
    if (nextColon)
        hostEnd = nextColon;
    else
        hostEnd = nextSlash;
    *aHost = (char *) malloc(hostEnd - pos + 1); // to NULL terminate
    if (!*aHost)
        return eMem;
    memset(*aHost, 0, hostEnd - pos + 1);
    strncpy(*aHost, pos, hostEnd - pos);
    *(*aHost + (hostEnd - pos)) = 0; // NULL terminate

    pos = nextSlash;
    end = aURL + strlen(aURL);

    *aPath = (char *) malloc(end - pos + 1);
    if (!*aPath)
    {
        if (*aHost)
            free(*aHost);
        return eMem;
    }
    memset(*aPath, 0, end - pos + 1);
    strncpy(*aPath, pos, end - pos);

    return 0;
}

void
CompressToFit(char *origStr, char *outStr, int outStrLen)
{
    int origStrLen;
    int halfOutStrLen;
    char *lastPart; // last origStr part start

    if (!origStr || !outStr || outStrLen <= 0)
        return;
        
    origStrLen = strlen(origStr);    
    halfOutStrLen = outStrLen/2;
    lastPart = origStr + origStrLen - halfOutStrLen;
    
    strncpy(outStr, origStr, halfOutStrLen);
    *(outStr + halfOutStrLen) = 0;
    strcat(outStr, "É");
    strncat(outStr, lastPart, strlen(lastPart));
    *(outStr + outStrLen + 1) = 0;
}

float
ComputeRate(int bytes, time_t startTime, time_t endTime)
{
    double period = difftime(endTime, startTime);
    float rate = bytes/period;
    
    rate /= 1024;  // convert from bytes/sec to KB/sec
    
    return rate;
}

#define kProgMsgLen 51

int
DLProgressCB(int aBytesSoFar, int aTotalFinalSize)
{   
    static int yielder = 0, yieldFrequency = 64;
    int len;
    char compressedStr[kProgMsgLen + 1];  // add one for NULL termination
    char *fullPathCopy = 0; // GetFullPath doesn't null terminate
    float rate = 0;
    time_t now;
    Rect teRect;
      
    if (aTotalFinalSize != sCurrTotalDLSize)
    {
        sCurrTotalDLSize = aTotalFinalSize;
        if (gControls->tw->dlProgressBar)
            SetControlMaximum(gControls->tw->dlProgressBar, (aTotalFinalSize/1024));
        
        // set short desc name of package being downloaded in Downloading field
        if (gControls->cfg->comp[sCurrComp].shortDesc)
        {
            HLock(gControls->cfg->comp[sCurrComp].shortDesc);
            if (*(gControls->cfg->comp[sCurrComp].shortDesc) && gControls->tw->dlProgressMsgs[0])
            {
                HLock((Handle)gControls->tw->dlProgressMsgs[0]);
                teRect = (**(gControls->tw->dlProgressMsgs[0])).viewRect;
                HUnlock((Handle)gControls->tw->dlProgressMsgs[0]);                
                
                len = strlen(*(gControls->cfg->comp[sCurrComp].shortDesc));
                TESetText(*(gControls->cfg->comp[sCurrComp].shortDesc), len, 
                    gControls->tw->dlProgressMsgs[0]);
                TEUpdate(&teRect, gControls->tw->dlProgressMsgs[0]);
            }
            HUnlock(gControls->cfg->comp[sCurrComp].shortDesc);
        }

        // compress URL string and insert in From field
        if (sCurrURL && gControls->tw->dlProgressMsgs[1])
        {
            HLock((Handle)gControls->tw->dlProgressMsgs[1]);
            teRect = (**(gControls->tw->dlProgressMsgs[1])).viewRect;
            HUnlock((Handle)gControls->tw->dlProgressMsgs[1]);
            
            CompressToFit(sCurrURL, compressedStr, kProgMsgLen);
            TESetText(compressedStr, kProgMsgLen, gControls->tw->dlProgressMsgs[1]);
            TEUpdate(&teRect, gControls->tw->dlProgressMsgs[1]);
        }
                            
        // compress fullpath string and insert in To field
        if (sCurrFullPath)
        {
            HLock(sCurrFullPath);
            if (*sCurrFullPath && gControls->tw->dlProgressMsgs[2])
            {
                fullPathCopy = (char *)malloc(sCurrFullPathLen + 1);               
                if (fullPathCopy)
                {
                    strncpy(fullPathCopy, (*sCurrFullPath), sCurrFullPathLen);
                    *(fullPathCopy + sCurrFullPathLen) = 0;
                    
                    HLock((Handle)gControls->tw->dlProgressMsgs[2]);
                    teRect = (**(gControls->tw->dlProgressMsgs[2])).viewRect;
                    HUnlock((Handle)gControls->tw->dlProgressMsgs[2]);
                    
                    CompressToFit(fullPathCopy, compressedStr, kProgMsgLen);
                    TESetText(compressedStr, kProgMsgLen, gControls->tw->dlProgressMsgs[2]);
                    TEUpdate(&teRect, gControls->tw->dlProgressMsgs[2]);
                    
                    free(fullPathCopy);
                }                
            }
            HUnlock(sCurrFullPath);
        }
    }
        
    if (gControls->tw->dlProgressBar)
    {                
        if (++yielder == yieldFrequency)
        {
            SetControlValue(gControls->tw->dlProgressBar, (aBytesSoFar/1024));

            // update rate info
            now = time(NULL);
            rate = ComputeRate(aBytesSoFar, sCurrStartTime, now);
            
            sprintf(compressedStr, "%d KB of %d KB  (%.2f KB/sec)", 
                aBytesSoFar/1024, aTotalFinalSize/1024, rate);
            HLock((Handle)gControls->tw->dlProgressMsgs[3]);
            teRect = (**(gControls->tw->dlProgressMsgs[3])).viewRect;
            HUnlock((Handle)gControls->tw->dlProgressMsgs[3]);
            
            TESetText(compressedStr, strlen(compressedStr), gControls->tw->dlProgressMsgs[3]);
            TEUpdate(&teRect, gControls->tw->dlProgressMsgs[3]);
            
            yielder = 0;   
            YieldToAnyThread();
        }
    }
    
    return 0;
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

const int kNumDLFields = 4;

void
InitDLProgControls(void)
{
	Boolean	indeterminateFlag = false;
	Rect r;
	GrafPtr	oldPort;
	GetPort(&oldPort);    
	int i;
		
	if (gWPtr)
	{
	    SetPort(gWPtr);
	    
	    gControls->tw->dlProgressBar = GetNewControl(rDLProgBar, gWPtr);
	    if (gControls->tw->dlProgressBar)
	    {
	        SetControlData(gControls->tw->dlProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag, 
	            sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
            Draw1Control(gControls->tw->dlProgressBar);
            
            // draw labels
            Str255 labelStr;
            for (i = 0; i < kNumDLFields; ++i)
            {
                gControls->tw->dlLabels[i] = GetNewControl(rLabDloading + i, gWPtr);
                if (gControls->tw->dlLabels[i])
                {
                    GetIndString(labelStr, rStringList, sLabDloading + i);
                    SetControlData(gControls->tw->dlLabels[i], kControlNoPart, 
                                kControlStaticTextTextTag, labelStr[0], (Ptr)&labelStr[1]); 
                    ShowControl(gControls->tw->dlLabels[i]);
                }
            }
            
            TextFace(normal);
            TextSize(9);
            TextFont(applFont);
            for (i = 0; i < kNumDLFields; ++i)
            {          
                SetRect(&r, (*gControls->tw->dlLabels[i])->contrlRect.right,
                            (*gControls->tw->dlLabels[i])->contrlRect.top + 1,
                            (*gControls->tw->dlLabels[i])->contrlRect.right + 310,
                            (*gControls->tw->dlLabels[i])->contrlRect.bottom + 1 );
             
                            
                gControls->tw->dlProgressMsgs[i] = TENew(&r, &r);
            }
            TextSize(12);
            TextFont(systemFont);
	    }
	}
	
	SetPort(oldPort);
}

void
ClearDLProgControls(void)
{
    Rect teRect;
        
    for (int i = 0; i < kNumDLFields; ++i)
    {
        if (gControls->tw->dlLabels[i])
            DisposeControl(gControls->tw->dlLabels[i]);
        if (gControls->tw->dlProgressMsgs[i])
        {
            HLock((Handle)gControls->tw->dlProgressMsgs[i]);
            teRect = (**(gControls->tw->dlProgressMsgs[i])).viewRect;
            HUnlock((Handle)gControls->tw->dlProgressMsgs[i]);
            EraseRect(&teRect);
                        
            TEDispose(gControls->tw->dlProgressMsgs[i]);
        }
    }
    
    if (gControls->tw->dlProgressBar)
    {
        DisposeControl(gControls->tw->dlProgressBar);
        gControls->tw->dlProgressBar = NULL;
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

