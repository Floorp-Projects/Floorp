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

#define STANDALONE 1
#define XP_MAC 1
#include "zipstub.h"
#include "zipfile.h"
#include <ctype.h>

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

ConstStr255Param kDLMarker = "\pCurrent Download";

#include <Math64.h>

/* VersGreaterThan4 - utility function to test if it's >4.x running */
static Boolean VersGreaterThan4(FSSpec *fSpec)
{
	Boolean result = false;
	short	fRefNum = 0;
	
	SetResLoad(false);
	fRefNum = FSpOpenResFile(fSpec, fsRdPerm);
	SetResLoad(true);
	if (fRefNum != -1)
	{
		Handle	h;
		h = Get1Resource('vers', 2);
		if (h && **(unsigned short**)h >= 0x0500)
			result = true;
		CloseResFile(fRefNum);
	}
		
	return result;
}

/* Variant of FindRunningAppBySignature that looks from a specified PSN */
static OSErr FindNextRunningAppBySignature (OSType sig, FSSpec *fSpec, ProcessSerialNumber *psn)
{
	OSErr 			err = noErr;
	ProcessInfoRec 	info;
	FSSpec			tempFSSpec;
	
	while (true)
	{
		err = GetNextProcess(psn);
		if (err != noErr) return err;
		info.processInfoLength = sizeof(ProcessInfoRec);
		info.processName = nil;
		info.processAppSpec = &tempFSSpec;
		err = GetProcessInformation(psn, &info);
		if (err != noErr) return err;
		
		if (info.processSignature == sig)
		{
			if (fSpec != nil)
				*fSpec = tempFSSpec;
			return noErr;
		}
	}
	
	return procNotFound;
}

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
	
	ProcessSerialNumber thePSN;
	FSSpec				theSpec;
	DialogPtr pDlg = nil;

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
			ErrorHandler(err, nil);
			return (void*)0;
		}
	}
	else
	{
		err = FSpGetDirectoryID( &tmpSpec, &dirID, &isDir );
		if (!isDir || err!=noErr)
		{
			ErrorHandler(err, nil);
			return (void*)0;
		}
	}	
#endif /* MIW_DEBUG */
	
	err = GetCWD(&srcDirID, &srcVRefNum);
	if (err != noErr)
	{
		ErrorHandler(err, nil);
		return (void*)nil;
	}
	
	/* get the "Installer Modules" relative subdir */
	GetIndString(pModulesDir, rStringList, sInstModules);
	isDir = false;  /* reuse */
	modulesDirID = 0;
	GetDirectoryID(srcVRefNum, srcDirID, pModulesDir, &modulesDirID, &isDir);
	srcDirID = modulesDirID;
	
	GetPort(&oldPort);
  ourHZ = GetZone();
	if (!isDir || !ExistArchives(srcVRefNum, srcDirID))
	{	
		bXPIsExisted = false;
		GetIndString(pIDIfname, rStringList, sTempIDIName);
	
	    /* otherwise if site selector exists, replace global URL with selected site */
        if (gControls->cfg->numSites > 0)
        {
	        if (gControls->cfg->globalURL[0])
	            DisposeHandle(gControls->cfg->globalURL[0]);
            gControls->cfg->globalURL[0] = NewHandleClear(kValueMaxLen);
            
            siteIndex = gControls->opt->siteChoice - 1;
            HLock(gControls->cfg->globalURL[0]);
            HLock(gControls->cfg->site[siteIndex].domain);
            strcpy(*(gControls->cfg->globalURL[0]), *(gControls->cfg->site[siteIndex].domain));
            HUnlock(gControls->cfg->globalURL[0]);
            HUnlock(gControls->cfg->site[siteIndex].domain);
	    }
        
        if (gControls->state != eResuming)
            InitDLProgControls(false);
        dlErr = DownloadXPIs(srcVRefNum, srcDirID);
        if (dlErr == nsFTPConn::E_USER_CANCEL)
        {
            return (void *) nil;
        }
		    if (dlErr != 0)
		    {
		      ErrorHandler(dlErr, nil);
			    return (void*) nil;
		    }
    }
 	  else
		  bCoreExists = true;
	
    /* Short term fix for #58928 - prevent install if Mozilla or Netscape running. syd 3/18/2002 */
	  while (FindRunningAppBySignature('MOSS', &theSpec, &thePSN) == noErr ||
		  FindRunningAppBySignature('MOZZ', &theSpec, &thePSN) == noErr)
	  {
			  Str255 itemText;
			  SInt16 itemHit;
			  short itemType;
			  Handle item;
        Rect itemBox, iconRect, windRect;
        CIconHandle cicn;
 
        if ( pDlg == nil ) {
          pDlg = GetNewDialog(151, NULL, (WindowPtr) -1);
        
          ShowWindow(pDlg);
          SelectWindow(pDlg);
        
          GetDialogItem(pDlg, 3, &itemType, &item, &itemBox);
          GetResourcedString(itemText, rInstList, sExecuting);
          SetDialogItemText( item, itemText );

          GetDialogItem(pDlg, 2, &itemType, &item, &itemBox);
          GetResourcedString(itemText, rInstList, sNextBtn);
          SetControlTitle((ControlRecord **)item, itemText);
        
          GetDialogItem(pDlg, 1, &itemType, &item, &itemBox);
          GetResourcedString(itemText, rInstList, sQuitBtn);
          SetControlTitle((ControlRecord **)item, itemText);

#if 1        
          GetDialogItem(pDlg, 4, &itemType, &item, &itemBox);
          cicn = GetCIcon( 129 );
          windRect = pDlg->portRect;
          LocalToGlobal((Point *) &windRect.top);
          LocalToGlobal((Point *) &windRect.bottom);
          SetRect(&iconRect, 
            windRect.left + itemBox.left, 
            windRect.top + itemBox.top, 
            windRect.left + itemBox.left + itemBox.right,
            windRect.top + itemBox.top + itemBox.bottom );
        
          if ( cicn != nil ) 
            PlotCIcon( &iconRect, cicn );
          SysBeep( 20L );
#endif               
        }
        do
        {
          EventRecord evt;
          
          if(WaitNextEvent(everyEvent, &evt, 1, nil))
          {
            if ( IsDialogEvent( &evt ) == false ) 
            {
              // let someone else handle it
              HandleNextEvent(&evt); // XXX
            } 
            else 
            {
              // find out what was clicked
              DialogPtr tDlg;
              DialogSelect( &evt, &tDlg, &itemHit );
            }
          }
        } while(itemHit != 1 && itemHit != 2);

        if ( itemHit == 1 ) {
          DisposeDialog(pDlg);    
          gDone = true;
          return (void*) nil;
        } 
        else {
          itemHit = 0;
          SysBeep( 20L );
        }
    }
	  
	  if (pDlg != nil )
	    DisposeDialog(pDlg);

    SetPort(oldPort);       	
    /* otherwise core exists in cwd:Installer Modules, different from extraction location */
    ClearDLProgControls(false);
    DisableNavButtons();
		if (gWPtr)
		{
			GetPort(&oldPort);

			SetPort(gWPtr);
			BeginUpdate(gWPtr);
			DrawControls(gWPtr);
			ShowLogo(false);
			UpdateTerminalWin();
			EndUpdate(gWPtr);
		
			SetPort(oldPort);
		}
		SetZone(ourHZ);


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
				ErrorHandler(err, nil);
				if (coreFile)
					DisposePtr((Ptr)coreFile);
				return (void*) nil;
			}
		
			/* run all .xpi's through XPInstall */
			err = RunAllXPIs(srcVRefNum, srcDirID, vRefNum, dirID);
			if (err!=noErr)
				ErrorHandler(err, nil);
				
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
			    // XXX should this change to look at archive size instead?
                totalDLSize += gControls->cfg->comp[i].size;
                
                compsDone++;
            }
        }
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
    }
    
    return totalDLSize;
}

#define MAXCRC 4

short
DownloadXPIs(short destVRefNum, long destDirID)
{
    short rv = 0;
    Handle dlPath;
    short dlPathLen = 0;
    int i, compsDone = 0, instChoice = gControls->opt->instChoice-1, resPos = 0;
    Boolean bResuming = false;
    Boolean bDone = false;
    int markedIndex = 0, currGlobalURLIndex = 0;
    CONN myConn;
    int crcPass;
    int count = 0;
    
    GetFullPath(destVRefNum, destDirID, "\p", &dlPathLen, &dlPath);
	DLMarkerGetCurrent(&markedIndex, &compsDone);
	if (markedIndex >= 0)
	    resPos = GetResPos(&gControls->cfg->comp[markedIndex]);
    	
	myConn.URL = (char *) NULL;
	myConn.type = TYPE_UNDEF;
	
	crcPass = 1;
	while ( bDone == false && crcPass <= MAXCRC ) {
    for ( i = 0; i < kNumDLFields; i++ ) {
        ShowControl( gControls->tw->dlLabels[i] );
    }
	for(i = 0; i < kMaxComponents; i++)
	{
		// general test: if component in setup type
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) && 
			 gControls->cfg->comp[i].dirty == true)
		{ 
			// if custom and selected -or- not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) )
			{    
			    // stat file even if index is less than markedIndex
			    // and download file if it isn't locally there;
			    // this can happen if a new setup type was selected
			    if (i < markedIndex)
			    {
			        if (noErr == ExistsXPI(i))
			            continue;  
			    }
			    
			    count++;
			    
			    // set up vars for dl callback to use
                sCurrComp = i;
                sCurrFullPath = dlPath;
                sCurrFullPathLen = dlPathLen;
                
                // download given full path and archive name
                if (i == markedIndex && resPos > 0)
                {
                    gControls->resPos = resPos;
                    gControls->state = eResuming;
                }
TRY_DOWNLOAD:
                rv = DownloadFile(dlPath, dlPathLen, gControls->cfg->comp[i].archive, resPos, currGlobalURLIndex, &myConn);
                if (rv == nsFTPConn::E_USER_CANCEL)
                {
                    break;
                }
                if (rv != 0)
                {
                    if (++currGlobalURLIndex < gControls->cfg->numGlobalURLs)
                        goto TRY_DOWNLOAD;
                        
                    ErrorHandler(rv, nil);
                    break;
                } else
                	currGlobalURLIndex = 0;
                resPos = 0; // reset after first file was resumed in the middle 
                gControls->state = eDownloading;
                compsDone++;
            }
        }
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
			break;  
    } // end for
    
        if ( rv != nsFTPConn::E_USER_CANCEL ) {   // XXX cancel is also paused :-)
    	    bDone = CRCCheckDownloadedArchives(dlPath, dlPathLen, count); 
    	    if ( bDone == false ) {
    	        compsDone = 0;
    	        crcPass++;   
    	    }
        } else {
            break;  // cancel really happened, or we paused
        }
    } // end while
 
 	// clean up the Conn struct
 	
 	CheckConn( "", TYPE_UNDEF, &myConn, true );  
 	     	 
    if ( crcPass >= MAXCRC ) {
        ErrorHandler( eDownload, nil );  // XXX need a better error message here
        rv = eDownload;
    }
    
    if (rv == 0)
        DLMarkerDelete();
        
    return rv;
}

const char kHTTP[8] = "http://";
const char kFTP[7] = "ftp://";

short
DownloadFile(Handle destFolder, long destFolderLen, Handle archive, int resPos, int urlIndex, CONN *myConn)
{
    short rv = 0;
    char *URL = 0, *proxyServerURL = 0, *destFile = 0, *destFolderCopy = 0;
    int globalURLLen, archiveLen, proxyServerURLLen;
    char *ftpHost = 0, *ftpPath = 0;
    Boolean bGetTried = false;
    Boolean isNewConn;
    
    // make URL using globalURL
    HLock(archive);
    DLMarkerSetCurrent(*archive);
    HLock(gControls->cfg->globalURL[urlIndex]);
    globalURLLen = strlen(*gControls->cfg->globalURL[urlIndex]);
    archiveLen = strlen(*archive);
    URL = (char *) malloc(globalURLLen + archiveLen + 1); // add 1 for NULL termination
    sprintf(URL, "%s%s", *gControls->cfg->globalURL[urlIndex], *archive);
    HUnlock(gControls->cfg->globalURL[urlIndex]);
    
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
     
    UpdateControls( gWPtr, gWPtr->visRgn ); 
    ShowLogo( false );  
    // was proxy info specified?
    if (gControls->opt->proxyHost && gControls->opt->proxyPort)
    {
        nsHTTPConn *conn;
        
        // make HTTP URL with "http://proxyHost:proxyPort"
        proxyServerURLLen = strlen(kHTTP) + strlen(gControls->opt->proxyHost) + 1 + 
                            strlen(gControls->opt->proxyPort) + 1;
        proxyServerURL = (char *) calloc(proxyServerURLLen, sizeof( char ) );
        sprintf(proxyServerURL, "%s%s:%s", kHTTP, gControls->opt->proxyHost, gControls->opt->proxyPort);
         
        isNewConn = CheckConn( proxyServerURL, TYPE_PROXY, myConn, false ); // closes the old connection if any
        
        if ( isNewConn == true ) {
        
        	conn = new nsHTTPConn(proxyServerURL, BreathFunc);
        
        	// set proxy info: proxied URL, username, password

        	conn->SetProxyInfo(URL, gControls->opt->proxyUsername, gControls->opt->proxyPassword);
        
        	myConn->conn = (void *) conn;
        	
        	// open an HTTP connection
        	rv = conn->Open();
            if ( rv == nsHTTPConn::OK ) {
            	myConn->conn = (void *) conn;
            	myConn->type = TYPE_PROXY;
            	myConn->URL = (char *) calloc( strlen( proxyServerURL ) + 1, sizeof( char ) );
            	if ( myConn->URL != (char *) NULL )
            		strcpy( myConn->URL, proxyServerURL );
           }
        } else {
        	conn = (nsHTTPConn *) myConn->conn;
        }
        
        if (isNewConn ==false || rv == nsHTTPConn::OK)
        {
            sCurrStartTime = time(NULL);
            bGetTried = true;
            rv = conn->Get(DLProgressCB, destFile, resPos);
        }
    }
        
    // else do we have an HTTP URL? 
    else if (strncmp(URL, kHTTP, strlen(kHTTP)) == 0)
    {
    	nsHTTPConn *conn;

        // XXX for now, the URL is always different, so we always create a new
        // connection here
        
        isNewConn = CheckConn( URL, TYPE_HTTP, myConn, false ); // closes the old connection if any
        
        if ( isNewConn == true ) {
        	// open an HTTP connection
        	
        	conn = new nsHTTPConn(URL, BreathFunc);
        
        	myConn->conn = (void *) conn;
        
        	rv = conn->Open();
            if ( rv == nsHTTPConn::OK ) {
            	myConn->conn = (void *) conn;
            	myConn->type = TYPE_HTTP;
            	myConn->URL = (char *) calloc( strlen( URL ) + 1, sizeof( char ) );
            	if ( myConn->URL != (char *) NULL )
            		strcpy( myConn->URL, URL );
           }
        } else
        	conn = (nsHTTPConn *) myConn->conn;
        
        if (isNewConn == false || rv == nsHTTPConn::OK)
        {
            sCurrStartTime = time(NULL);
            bGetTried = true;
            rv = conn->Get(DLProgressCB, destFile, resPos);
        }
    }
    
    // else do we have an FTP URL?
    else if (strncmp(URL, kFTP, strlen(kFTP)) == 0)
    {
        rv = ParseFTPURL(URL, &ftpHost, &ftpPath);
        if (!*ftpHost || !*ftpPath)
        {
            rv = nsHTTPConn::E_MALFORMED_URL;
        }
        else
        {
            nsFTPConn *conn;
            
            // open an FTP connection
        	isNewConn = CheckConn( ftpHost, TYPE_FTP, myConn, false ); // closes the old connection if any
        
        	if ( isNewConn == true ) {
        	
            	conn = new nsFTPConn(ftpHost, BreathFunc);
            
            	rv = conn->Open();
            	if ( rv == nsFTPConn::OK ) {
            		myConn->conn = (void *) conn;
            		myConn->type = TYPE_FTP;
            		myConn->URL = (char *) calloc( strlen( ftpHost ) + 1, sizeof( char ) );
            		if ( myConn->URL != (char *) NULL )
            			strcpy( myConn->URL, ftpHost );
            	}
            } else 
            	conn = (nsFTPConn *) myConn->conn;
            	
            if (isNewConn == false || rv == nsFTPConn::OK)
            {
                sCurrStartTime = time(NULL);
                bGetTried = true;
                rv = conn->Get(ftpPath, destFile, nsFTPConn::BINARY, resPos, 1, DLProgressCB);
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
    
    if (bGetTried && rv != 0)
    {
    	// Connection was closed for us, clear out the Conn information
    	
 //   	myConn->type = TYPE_UNDEF;
 //   	free( myConn->URL );
 //   	myConn->URL = (char *) NULL;
    	
        if (++urlIndex < gControls->cfg->numGlobalURLs)
            return eDLFailed;  // don't pase yet: continue trying failover URLs
            
        /* the get failed before completing; simulate pause */
        SetPausedState();
        rv = nsFTPConn::E_USER_CANCEL;
    }
    
    return rv;
}

/* 
 * Name: CheckConn
 *
 * Arguments: 
 *
 * char *URL;     -- URL of connection we need to have established
 * int type;	  -- connection type (TYPE_HTTP, etc.)
 * CONN *myConn;  -- connection state (info about currently established connection)
 * Boolean force; -- force closing of connection
 *
 * Description:
 *
 * This function determines if the caller should open a connection based upon the current
 * connection that is open (if any), and the type of connection desired.
 * If no previous connection was established, the function returns true. If the connection
 * is for a different protocol, then true is also returned (and the previous connection is
 * closed). If the connection is for the same protocol and the URL is different, the previous
 * connection is closed and true is returned. Otherwise, the connection has already been
 * established, and false is returned.
 *
 * Return Value: If a new connection needs to be opened, true. Otherwise, false is returned.
 *
 * Original Code: Syd Logan (syd@netscape.com) 6/24/2001
 *
*/

Boolean
CheckConn( char *URL, int type, CONN *myConn, Boolean force )
{
	nsFTPConn *fconn;
	nsHTTPConn *hconn;
	Boolean retval = false;

	if ( myConn->type == TYPE_UNDEF )
		retval = true;					// first time
	else if ( ( myConn->type != type || strcmp( URL, myConn->URL ) || force == true ) && gControls->state != ePaused) {
		retval = true;
		switch ( myConn->type ) {
		case TYPE_HTTP:
		case TYPE_PROXY:
			hconn = (nsHTTPConn *) myConn->conn;
			hconn->Close();
			break;
		case TYPE_FTP:
			fconn = (nsFTPConn *) myConn->conn;
			fconn->Close();
			break;
		}
	}
	
	if ( retval == true && myConn->URL != (char *) NULL )
    	free( myConn->URL );

	return retval;
}

OSErr 
DLMarkerSetCurrent(char *aXPIName)
{
    OSErr err = noErr;
    short vRefNum = 0;
    long dirID = 0;
    FSSpec fsMarker;
    short markerRefNum;
    long count = 0;
    
    if (!aXPIName)
        return paramErr;
        
    err = GetInstallerModules(&vRefNum, &dirID);
    if (err != noErr)
        return err;
        
    // check if marker file exists        
    err = FSMakeFSSpec(vRefNum, dirID, kDLMarker, &fsMarker);
    
    // delete old marker and recreate it so we truncate to 0
    if (err == noErr)
        FSpDelete(&fsMarker);

    err = FSpCreate(&fsMarker, 'ttxt', 'TEXT', smSystemScript);
    if (err != noErr)
        return err;
        
    // open data fork
    err = FSpOpenDF(&fsMarker, fsWrPerm, &markerRefNum);
    if (err != noErr)
        goto BAIL;
        
    // write xpi name into marker's data fork at offset 0    
    count = strlen(aXPIName);
    err = FSWrite(markerRefNum, &count, (void *) aXPIName);
    
BAIL:
    // close marker file
    FSClose(markerRefNum);
    
    return err;
}

OSErr
DLMarkerGetCurrent(int *aMarkedIndex, int *aCompsDone)
{
    OSErr err = noErr;
    char xpiName[255];
    short vRefNum = 0;
    long dirID = 0;
    long count = 0;
    FSSpec fsMarker;
    short markerRefNum;
    
    if (!aMarkedIndex || !aCompsDone)
        return paramErr;
        
    err = GetInstallerModules(&vRefNum, &dirID);
    if (err != noErr)
        return err;
        
    // check if marker file exists
    err = FSMakeFSSpec(vRefNum, dirID, kDLMarker, &fsMarker);
    if (err == noErr)
    {
        // open for reading
        err = FSpOpenDF(&fsMarker, fsRdPerm, &markerRefNum);
        if (err != noErr)
            goto CLOSE_FILE;
            
        // get file size
        err = GetEOF(markerRefNum, &count);
        if (err != noErr)
            goto CLOSE_FILE;
            
        // read file contents
        err = FSRead(markerRefNum, &count, (void *) xpiName);
        if (err == noErr)
        {
            if (count <= 0)
                err = readErr;
            else
            {
                xpiName[count] = 0; // ensure only reading 'count' bytes
                err = GetIndexFromName(xpiName, aMarkedIndex, aCompsDone);
            }
        }
        
CLOSE_FILE:
        // close file
        FSClose(markerRefNum);
    }
    
    return err;
}

OSErr 
DLMarkerDelete(void)
{
    OSErr err;
    short vRefNum = 0;
    long dirID = 0;
    FSSpec fsMarker;
    
    err = GetInstallerModules(&vRefNum, &dirID);
    if (err == noErr)
    {
        err = FSMakeFSSpec(vRefNum, dirID, kDLMarker, &fsMarker);
        if (err == noErr)
            FSpDelete(&fsMarker);
    }
    
    return noErr;
}

OSErr
GetIndexFromName(char *aXPIName, int *aIndex, int *aCompsDone)
{
    OSErr err = noErr;
    int i, compsDone = 0, instChoice = gControls->opt->instChoice - 1;
    
    if (!aXPIName || !aIndex || !aCompsDone)
        return paramErr;
        
    // loop through 0 to kMaxComponents
	for(i = 0; i < kMaxComponents; i++)
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
                HLock(gControls->cfg->comp[i].archive);
                if (strncmp(aXPIName, (*(gControls->cfg->comp[i].archive)), strlen(aXPIName)) == 0)
                {
                    HUnlock(gControls->cfg->comp[i].archive);
                    *aIndex = i;
                    *aCompsDone = compsDone;
                    break;
                }   
                else
                    HUnlock(gControls->cfg->comp[i].archive);                 
                compsDone++;
            }
        }
		else if (compsDone >= gControls->cfg->st[instChoice].numComps)
		{
		    err = userDataItemNotFound;
			break;
	    }
    }
    return err;
}

int
GetResPos(InstComp *aComp)
{
    OSErr err = noErr;
    int resPos = 0;
    short vRefNum = 0;
    long dirID = 0;
    Str255 pArchiveName;
    long dataSize = 0, rsrcSize = 0;
    
    if (!aComp)
        return 0;
    
    err = GetInstallerModules(&vRefNum, &dirID);
    if (err == noErr)
    {
        HLock(aComp->archive);
        my_c2pstrcpy(*(aComp->archive), pArchiveName);
        HUnlock(aComp->archive);
        
        err = GetFileSize(vRefNum, dirID, pArchiveName, &dataSize, &rsrcSize);
        if (err == noErr && dataSize > 0)
            resPos = dataSize;
    }
    
    return resPos;
}

OSErr
GetInstallerModules(short *aVRefNum, long *aDirID)
{
    short cwdVRefNum = 0;
    long cwdDirID = 0, imDirID = 0;
    OSErr err;
    Boolean isDir = false;
    Str255 pIMFolder;  // "Installer Modules" fodler
    
    if (!aVRefNum || !aDirID)
        return paramErr;
      
    *aVRefNum = 0;
    *aDirID = 0;
      
    err = GetCWD(&cwdDirID, &cwdVRefNum);
    if (err != noErr)
        return err;
        
    GetIndString(pIMFolder, rStringList, sInstModules);
    err = GetDirectoryID(cwdVRefNum, cwdDirID, pIMFolder, &imDirID, &isDir);
    if (err != noErr)
        return err;
        
    if (isDir)
    {
        *aVRefNum = cwdVRefNum;
        *aDirID = imDirID;
    }
    else
        return dirNFErr;
        
    return err;
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
    
    // don't truncate if already less than acceptable max len
    if (origStrLen < outStrLen)
    {
        strcpy(outStr, origStr);
        *(outStr + strlen(origStr)) = 0;
        return; 
    }
    
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
#define kLowRateThreshold ((float)20)

int
DLProgressCB(int aBytesSoFar, int aTotalFinalSize)
{   
    static int yielder = 0, yieldFrequency = 8;
    int len;
    char compressedStr[kProgMsgLen + 1];  // add one for NULL termination
    char *fullPathCopy = 0; // GetFullPath doesn't null terminate
    float rate = 0;
    time_t now;
    Rect teRect;
    Str255 dlStr;
    char tmp[kKeyMaxLen];
      
    if (gControls->state == ePaused)
    {
        return nsFTPConn::E_USER_CANCEL;
    }
            
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
                EraseRect(&teRect);
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
        // update rate info
        now = time(NULL);
        rate = ComputeRate(aBytesSoFar, sCurrStartTime, now);                      
        
        if ((rate < kLowRateThreshold) || ((++yielder) == yieldFrequency))
        {
            int adjustedBytesSoFar = aBytesSoFar;
            if (gControls->state == eResuming)
                adjustedBytesSoFar += gControls->resPos;
            SetControlValue(gControls->tw->dlProgressBar, (adjustedBytesSoFar/1024));
            
            // create processing string "%d KB of %d KB  (%.2f KB/sec)"
            GetResourcedString(dlStr, rInstList, sDownloadKB);
            strcpy(compressedStr, PascalToC(dlStr));
            sprintf(tmp, "%d", adjustedBytesSoFar/1024);
            strtran(compressedStr, "%d1", tmp);
            sprintf(tmp, "%d", aTotalFinalSize/1024);
            strtran(compressedStr, "%d2", tmp);
            sprintf(tmp, "%.2f", rate);
            strtran(compressedStr, "%.2f", tmp);

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
		ErrorHandler(err, nil);
		return false;
	}
	err = FSpCreate(idiSpec, 'NSCP', 'TEXT', smSystemScript);
	if ( (err != noErr) && (err != dupFNErr))
	{
		ErrorHandler(err, nil);
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
					HLock(gControls->cfg->globalURL[0]);
					strcat(buf, *gControls->cfg->globalURL[0]);
					HUnlock(gControls->cfg->globalURL[0]);
					
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

OSErr
ExistsXPI(int aIndex)
{
    OSErr err = noErr;
    FSSpec fsComp;
    short vRefNum = 0;
    long dirID = 0;
    Str255 pArchive;
    
    if (aIndex < 0)
        return paramErr;
        
    err = GetInstallerModules(&vRefNum, &dirID);
    if (err == noErr)
    {
        HLock(gControls->cfg->comp[aIndex].archive);
        my_c2pstrcpy(*(gControls->cfg->comp[aIndex].archive), pArchive);
        HUnlock(gControls->cfg->comp[aIndex].archive);
        err = FSMakeFSSpec(vRefNum, dirID, pArchive, &fsComp);
    }
    
    return err;
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
					
                compsDone++;
			}
		}
        else if (compsDone >= gControls->cfg->st[instChoice].numComps)
            break;  
	}
}

void
InitDLProgControls(Boolean onlyLabels)
{
	Boolean	indeterminateFlag = false;
	Rect r;
	GrafPtr	oldPort;
	GetPort(&oldPort);    
	int i;
		
	if (gWPtr)
	{
	    SetPort(gWPtr);
	    
	    if ( onlyLabels == false ) {
	        gControls->tw->dlProgressBar = GetNewControl(rDLProgBar, gWPtr);
	        if (gControls->tw->dlProgressBar)
	        {
	            SetControlData(gControls->tw->dlProgressBar, kControlNoPart, kControlProgressBarIndeterminateTag, 
	                sizeof(indeterminateFlag), (Ptr) &indeterminateFlag);
                Draw1Control(gControls->tw->dlProgressBar);
            }
        }
            // draw labels
        Str255 labelStr;
        for (i = 0; i < kNumDLFields; ++i)
        {
            gControls->tw->dlLabels[i] = GetNewControl(rLabDloading + i, gWPtr);
            if (gControls->tw->dlLabels[i])
            {
                GetResourcedString(labelStr, rInstList, sLabDloading + i);
                SetControlData(gControls->tw->dlLabels[i], kControlNoPart, 
                    kControlStaticTextTextTag, labelStr[0], (Ptr)&labelStr[1]); 
                ShowControl(gControls->tw->dlLabels[i]);
            }
        }
        if ( onlyLabels == false ) {    
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
ClearDLProgControls(Boolean onlyLabels)
{
    Rect teRect;
    GrafPtr oldPort;
    
    GetPort(&oldPort);
    SetPort(gWPtr);
        
    for (int i = 0; i < kNumDLFields; ++i)
    {
        if (gControls->tw->dlLabels[i])
            DisposeControl(gControls->tw->dlLabels[i]);
        if (gControls->tw->dlProgressMsgs[i] && onlyLabels == false)
        {
            HLock((Handle)gControls->tw->dlProgressMsgs[i]);
            teRect = (**(gControls->tw->dlProgressMsgs[i])).viewRect;
            HUnlock((Handle)gControls->tw->dlProgressMsgs[i]);
            EraseRect(&teRect);
                        
            TEDispose(gControls->tw->dlProgressMsgs[i]);
        }
    }
    
    if (gControls->tw->dlProgressBar && onlyLabels == false)
    {
        DisposeControl(gControls->tw->dlProgressBar);
        gControls->tw->dlProgressBar = NULL;
    }
    
    SetPort(oldPort);
}

void
InitProgressBar(void)
{
	Boolean	indeterminateFlag = true;
	Rect	r, r2;
	Str255	extractingStr;
	GrafPtr	oldPort;
	Handle rectH;
	OSErr reserr;
	
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
				GetResourcedString(extractingStr, rInstList, sExtracting);
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
	
    rectH = GetResource('RECT', 165);
	reserr = ResError();
	if (reserr == noErr && rectH)
	{
	    HLock(rectH);
		r = (Rect) **((Rect**)rectH);
		SetRect(&r2, r.left, r.top, r.right, r.bottom);
		HUnlock(rectH);
		reserr = ResError();
		if (reserr == noErr)
		{
		    EraseRect(&r2);
		}
	}

	SetPort(oldPort);
}

/* replace a substring "srch" (first found), with a new string "repl" */
void strtran(char* str, const char* srch, const char* repl) 
{
    int lsrch=strlen(srch);
    char *p;
    char tmp[kKeyMaxLen];

	p = strstr(str, srch);
	if( p == nil )
		return;
	strcpy(tmp, p);
	*p = '\0';
	strcat(str, repl);
	p = tmp;
	p = p + lsrch;
	strcat(str, p);
	
}

/* 
 * Name: VerifyArchive
 *
 * Arguments: 
 *
 * char *szArchive;     -- path of archive to verify
 *
 * Description:
 *
 * This function verifies that the specified path exists, that it is a XPI file, and
 * that it has a valid checksum.
 *
 * Return Value: If all tests pass, ZIP_OK. Otherwise, !ZIP_OK
 *
 * Original Code: Syd Logan (syd@netscape.com) 6/25/2001
 *
*/

int 
VerifyArchive(char *szArchive)
{
  void *vZip;
  int  iTestRv;
  OSErr err;
  FSSpec spec;

  err = FSpLocationFromFullPath( strlen( szArchive ), szArchive, &spec );
									
  /* Check for the existance of the from (source) file */
  if(err != noErr)
    return(!ZIP_OK);

  
  if((iTestRv = ZIP_OpenArchive(szArchive, &vZip)) == ZIP_OK)
  {
    BreathFunc();
    /* 1st parameter should be NULL or it will fail */
    /* It indicates extract the entire archive */
    iTestRv = ZIP_TestArchive(vZip);
    ZIP_CloseArchive(&vZip);
  } 
  if ( iTestRv != ZIP_OK )
    err = FSpDelete( &spec );      // nuke it
  return(iTestRv);
}

/* 
 * Name: CRCCheckDownloadedArchives
 *
 * Arguments: 
 *
 * Handle dlPath;     -- a handle to the location of the XPI files on disk
 * short dlPathlen;	  -- length, in bytes, of dlPath
 *
 * Description:
 *
 * This function iterates the XPI files and calls VerifyArchive() on each to determine
 * which archives pass checksum checks. If a file passes, its dirty flag is set to false,
 * so that it is not re-read the next time we dload archives if any of the files come up
 * with an invalid CRC. 
 *
 * Return Value: if all archives pass, true. Otherwise, false.
 *
 * Original Code: Syd Logan (syd@netscape.com) 6/24/2001
 *
*/

Boolean 
CRCCheckDownloadedArchives(Handle dlPath, short dlPathlen, int count)
{
    int i, len;
    Rect teRect;
    Boolean isClean;
    char buf[ 1024 ];
    char validatingBuf[ 128 ];
    Boolean indeterminateFlag = true;
	Str255	validatingStr;
	int compsDone = 0, instChoice = gControls->opt->instChoice-1;

    isClean = true;

    ClearDLProgControls(true); 
    DisablePauseAndResume();      
        
    for ( i = 1; i < kNumDLFields; i++ ) {
        HLock( (Handle) gControls->tw->dlProgressMsgs[i] );
        teRect = (**(gControls->tw->dlProgressMsgs[i])).viewRect;
        EraseRect(&teRect);
        HUnlock( (Handle) gControls->tw->dlProgressMsgs[i] );
    }
    if ( gControls->tw->dlProgressBar) {
        SetControlValue(gControls->tw->dlProgressBar, 0);
        SetControlMaximum(gControls->tw->dlProgressBar, count);
    } 

    GetResourcedString(validatingStr, rInstList, sValidating);
    strncpy( validatingBuf, (const char *) &validatingStr[1], (unsigned char) validatingStr[0] ); 
	for(i = 0; i < kMaxComponents; i++) {
	    BreathFunc();
		if ( (gControls->cfg->st[instChoice].comp[i] == kInSetupType) &&
			 (compsDone < gControls->cfg->st[instChoice].numComps) && 
			 gControls->cfg->comp[i].dirty == true)
		{ 
			// if custom and selected -or- not custom setup type
			if ( ((instChoice == gControls->cfg->numSetupTypes-1) && 
				  (gControls->cfg->comp[i].selected == true)) ||
				 (instChoice < gControls->cfg->numSetupTypes-1) ) {
                if (gControls->cfg->comp[i].shortDesc)
                {
                    HLock(gControls->cfg->comp[i].shortDesc);
                    if (*(gControls->cfg->comp[i].shortDesc) && gControls->tw->dlProgressMsgs[0])
                    {
                        HLock((Handle)gControls->tw->dlProgressMsgs[0]);
                        teRect = (**(gControls->tw->dlProgressMsgs[0])).viewRect;
                        HUnlock((Handle)gControls->tw->dlProgressMsgs[0]);                
                        sprintf( buf, validatingBuf, *(gControls->cfg->comp[i].shortDesc));
                        len = strlen( buf );
                        TESetText(buf, len, 
                            gControls->tw->dlProgressMsgs[0]);
                        TEUpdate(&teRect, gControls->tw->dlProgressMsgs[0]);
                    }
                    HUnlock(gControls->cfg->comp[sCurrComp].shortDesc);
                }
	            if ( gControls->cfg->comp[i].dirty == true ) {
	                HLock(dlPath);
                    HLock(gControls->cfg->comp[i].archive);
                    strncpy( buf, *dlPath, dlPathlen );
                    buf[ dlPathlen ] = '\0';
                    strcat( buf, *(gControls->cfg->comp[i].archive) );
	                HUnlock(dlPath);
                    HUnlock(gControls->cfg->comp[i].archive);
                    if (IsArchiveFile(buf) == false || VerifyArchive( buf ) == ZIP_OK) 
                        gControls->cfg->comp[i].dirty = false;
                    else
                        isClean = false;
                }
                if ( gControls->tw->dlProgressBar) {
                    SetControlValue(gControls->tw->dlProgressBar, 
                        GetControlValue(gControls->tw->dlProgressBar) + 1);
                }  
            }
        }  
    }
    
    InitDLProgControls( true );
    return isClean;
}

/* 
 * Name: IsArchiveFile( char *path )
 * 
 * Arguments:
 * 
 * char *path -- NULL terminated pathname 
 *
 * Description: 
 *  
 * This function extracts the file extension of filename pointed to by path and then 
 * checks it against a table of extensions. If a match occurs, the file is considered
 * to be an archive file that has a checksum we can validate, and we return PR_TRUE.
 * Otherwise, PR_FALSE is returned.
 *
 * Return Value: true if the file extension matches one of those we are looking for,
 * and false otherwise.
 *
 * Original Code: Syd Logan 7/28/2001
 *
*/

static char *extensions[] = { "ZIP", "XPI", "JAR" };  // must be uppercase

Boolean
IsArchiveFile( char *buf ) 
{
    PRBool ret = false;
    char lbuf[1024];
    char *p;
    int i, max;
    
    // if we have a string and it contains a '.'
    
    if ( buf != (char *) NULL && ( p = strrchr( buf, '.' ) ) != (char *) NULL ) {
        p++;
        
        // if there are characters after the '.' then see if there is a match
        
        if ( *p != '\0' ) {
            
            // make a copy of the extension, and fold to uppercase, since mac has no strcasecmp
            // and we need to ensure we are comparing strings of chars that have the same case. 

            strcpy( lbuf, p );
            for ( i = 0; i < strlen( lbuf ); i++ )
            	lbuf[i] = toupper(lbuf[i]);
            
            // search
            	
            max = sizeof( extensions ) / sizeof ( char * );
            for ( i = 0; i < max; i++ ) 
                if ( !strcmp( lbuf, extensions[i] ) ) {
                    ret = true;
                    break;
                }
        }   
    }
    return ( ret );
}
