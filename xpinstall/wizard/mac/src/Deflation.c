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
 * Corporation. Portions created by Netscape are Copyright (C) 1998-1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "MacInstallWizard.h"


#define STANDALONE 1
#define XP_MAC 1
#include "zipstub.h"
#include "zipfile.h"
#include "nsAppleSingleDecoder.h"

#include <TextUtils.h>

static FSSpec 	coreFileList[kMaxCoreFiles];
static short	currCoreFile = 0;

#define SLASHES_2_COLONS(_path)										\
do {																\
	char	*delim;													\
	long	count = 0, len = strlen(_path);							\
																	\
	while ( (count < len) && ((delim = strchr(_path, '/')) != 0) )	\
	{																\
		*delim = ':';												\
		count++;													\
	}																\
} while(0)


/*-----------------------------------------------------------*
 *   Inflation
 *-----------------------------------------------------------*/
OSErr
ExtractCoreFile(short srcVRefNum, long srcDirID, short tgtVRefNum, long tgtDirID)
{
	OSErr 			err = noErr;
	StringPtr 		coreFile = 0;
	UInt32          endTicks;
	short			fullPathLen = 0;
	Handle			fullPathH = 0;
	Ptr				fullPathStr = 0;
	PRInt32			rv = 0;
	void			*hZip = 0, *hFind = 0;

	/* if there's a core file... */
	HLock(gControls->cfg->coreFile);
	if (*gControls->cfg->coreFile != NULL)
	{
		/* make local copy and unlock handle */
		coreFile = CToPascal(*gControls->cfg->coreFile);
		if (!coreFile)
		{
			err = memFullErr;
			goto cleanup;
		}
	}
	else
		return fnfErr;
	HUnlock(gControls->cfg->coreFile);
	
	ERR_CHECK_RET(GetFullPath(srcVRefNum, srcDirID, coreFile, &fullPathLen, &fullPathH), err);
	
	
	/* --- o p e n   a r c h i v e --- */
	
	/* extract the full path string from the handle so we can NULL terminate */
	HLock(fullPathH);
	fullPathStr = NewPtrClear(fullPathLen+1);
	strncat(fullPathStr, *fullPathH, fullPathLen);
	*(fullPathStr+fullPathLen) = '\0';
	
	rv = ZIP_OpenArchive( fullPathStr, &hZip );
	
	HUnlock(fullPathH);
	if (rv!=ZIP_OK) 
		goto cleanup;

	/* initialize the search */
	hFind = ZIP_FindInit( hZip, NULL ); /* null to match all files in archive */
	
	
	/* --- i n f l a t e   a l l   f i l e s --- */
	
	err = InflateFiles(hZip, hFind, tgtVRefNum, tgtDirID);
	if (err!=noErr)
		goto cleanup;	/* XXX review later: this check may be pointless */
	
	
	/* --- c l o s e   a r c h i v e --- */
cleanup:

	//if (hFind)
	//	rv = ZIP_FindFree( hFind );
#ifdef MIW_DEBUG
		if (rv!=ZIP_OK) SysBeep(10);
#endif
	if (hZip)
		rv = ZIP_CloseArchive( &hZip );
#ifdef MIW_DEBUG
		if (rv!=ZIP_OK) SysBeep(10); 
#endif
	if (coreFile)
		DisposePtr((Ptr)coreFile);
	if (fullPathH)
		DisposeHandle(fullPathH);
	if (fullPathStr)
		DisposePtr(fullPathStr);

    /* pause till frag registry is updated */
    endTicks = TickCount() + 60;
    while (TickCount() < endTicks)
    {
        YieldToAnyThread();
    }
    
	return err;
}

void
WhackDirectories(char *filename)
{
	// Level ouot essential files and components to satisfy
	// lib loading and dependency resolution issues
	
	Ptr		componentPathStr 	= 0;
	Ptr		tempStr				= 0;
	Ptr		finalStr			= 0;
	long	prefixLen			= 0;
	long    skipLen             = 0;
	
	componentPathStr = NewPtrClear(strlen(filename) + 1);
	finalStr		 = NewPtrClear(strlen(filename) + 1);
	strcpy(componentPathStr, filename);				// e.g. HD:Target:Essential Files:foo.shlb
													//   or HD:Target:Components:bar.shlb
	LowercaseText(componentPathStr, strlen(componentPathStr), smSystemScript);
	if((tempStr = strstr(componentPathStr, "essential files")) != NULL)
	{
		prefixLen = tempStr - componentPathStr;		// e.g. HD:Target:

		strncpy(finalStr, filename, prefixLen);  	// e.g. HD:Target:
		skipLen = prefixLen + strlen("essential files") + 1;  // add 1 to skip over extra ':'
		strcat(finalStr, filename + skipLen);
		strcpy(filename, finalStr);
	}
	else
	if ((tempStr = strstr(componentPathStr, "components")) != NULL)
	{
		prefixLen = tempStr - componentPathStr;		// e.g. HD:Target:

		strncpy(finalStr, filename, prefixLen);  	// e.g. HD:Target:
		skipLen = prefixLen + strlen("components") + 1;  // add 1 to skip over extra ':'
		strcat(finalStr, filename + skipLen);
		strcpy(filename, finalStr);					// e.g. HD:Target:foo.shlb
	}

	if(componentPathStr)
		DisposePtr(componentPathStr);
	if(finalStr)
		DisposePtr(finalStr);
}

OSErr
InflateFiles(void *hZip, void *hFind, short tgtVRefNum, long tgtDirID)
{
	OSErr		err = noErr;
	Boolean		bFoundAll = false;
	PRInt32		rv = 0;
	char		filename[255] = "\0", *lastslash, *leaf, macfilename[255] = "\0";
	Handle		fullPathH = 0;
	short 		fullPathLen = 0;
	Ptr			fullPathStr = 0;
	StringPtr	extractedFile = 0;
	FSSpec		extractedFSp, outFSp;
	
	
	while (!bFoundAll)
	{
		/* find next item if one exists */
		rv = ZIP_FindNext( hFind, filename, 255 );
		if (rv==ZIP_ERR_FNF)
		{
			bFoundAll = true;
			break;
		}
		else if (rv!=ZIP_OK)
			return rv;	
		
		/* ignore if item is a dir entry */	
		lastslash = strrchr(filename, '/');
		if (lastslash == (&filename[0] + strlen(filename) - 1)) /* dir entry encountered */
			continue;

		/* grab leaf filename only */
		if (lastslash == 0)
			leaf = filename;
		else
			leaf = lastslash + 1;
		
		/* obtain and NULL terminate the full path string */
		err = GetFullPath(tgtVRefNum, tgtDirID, "\p", &fullPathLen, &fullPathH); /* get dirpath */
		if (err!=noErr)
			return err;

        strcpy(macfilename, filename);        
        SLASHES_2_COLONS(macfilename);
		HLock(fullPathH);
		fullPathStr = NewPtrClear(fullPathLen + strlen(macfilename) + 1);
		strncat(fullPathStr, *fullPathH, fullPathLen);
		strcat(fullPathStr, macfilename);	/* tack on filename to dirpath */
		*(fullPathStr+fullPathLen+strlen(macfilename)) = '\0';
		
		/* create directories if file is nested in new subdirs */
		WhackDirectories(fullPathStr);
		err = DirCreateRecursive(fullPathStr);			
		
		if (err!=noErr)
		{
			if (fullPathStr) 
				DisposePtr((Ptr)fullPathStr);
			if (fullPathH)
			{
				HUnlock(fullPathH);				
				DisposeHandle(fullPathH);
			}
			continue; /* XXX do we want to do this? */
		}
		
		/* extract the file to its full path destination */
		rv = ZIP_ExtractFile( hZip, filename, fullPathStr );
		
		HUnlock(fullPathH);
		if (fullPathH)
			DisposeHandle(fullPathH);
		if (rv!=ZIP_OK)
		{
			if (fullPathStr)
				DisposePtr((Ptr)fullPathStr);			
			return rv;
		}
		
		/* AppleSingle decode if need be */
		extractedFile = CToPascal(fullPathStr); 
		if (extractedFile)
		{
			err = FSMakeFSSpec(0, 0, extractedFile, &extractedFSp);
			err = FSMakeFSSpec(0, 0, extractedFile, &outFSp);
			err = AppleSingleDecode(&extractedFSp, &outFSp);
			
			/* delete original file if named different than final file */
			if (!pstrcmp(extractedFSp.name, outFSp.name))
			{
				err = FSpDelete(&extractedFSp);
			}
		}
			
		/* record for cleanup later */
		FSMakeFSSpec(outFSp.vRefNum, outFSp.parID, outFSp.name, &coreFileList[currCoreFile]);
		currCoreFile++;
		
		/* progress bar update (roll the barber poll) */
		if (gWPtr)
			IdleControls(gWPtr);	
			
		if (extractedFile)
			DisposePtr((Ptr)extractedFile);
		if (fullPathStr)
			DisposePtr(fullPathStr);
	}
        
	return err;
}

OSErr
AppleSingleDecode(FSSpecPtr fd, FSSpecPtr outfd)
{
	OSErr	err = noErr;
	
	// if file is AppleSingled
	if (nsAppleSingleDecoder::IsAppleSingleFile(fd))
	{
		// decode it
		nsAppleSingleDecoder decoder(fd, outfd);
			
		ERR_CHECK_RET(decoder.Decode(), err);
	}
	return err;
}

void
ResolveDirs(char *fname, char *dir)
{
	char *delim, *dirpath;
	dirpath = fname;
	Boolean delimFound = false;
	
	while( (delim = strchr(dirpath, '/')) != 0)
	{
		delimFound = true;
		*delim = ':';
		dirpath = delim;
	}
	
	if (delimFound)
	{
		strncpy(dir, fname, dirpath-fname);
		*(dir + (dirpath-fname)+1) = 0; // NULL terminate
	}
}

OSErr
DirCreateRecursive(char* path)
{
	long 		count, len=strlen(path), dummyDirID;
	char 		*delim = '\0', *pathpos = path, *currDir;
	OSErr 		err = noErr;
	StringPtr	pCurrDir;
	FSSpec		currDirFSp;
	
	currDir = (char*) malloc(len+1);
	
	if ((delim=strchr(pathpos, ':'))!=0)		/* skip first since it's volName */
	{
		for (count=0; ((count<len)&&( (delim=strchr(pathpos, ':'))!=0) ); count++)
		{
			currDir[0] = '\0';
			strncpy(currDir, path, delim-path+1);
			currDir[delim-path+1] = '\0';
		
			pCurrDir = CToPascal(currDir);
			if (pCurrDir && *pCurrDir > 0)
			{
				err = FSMakeFSSpec(0, 0, pCurrDir, &currDirFSp);
				if (err == fnfErr)
				{	
					err = FSpDirCreate(&currDirFSp, smSystemScript, &dummyDirID);
					if (err!=noErr) 
					{
						if (currDir)
							free(currDir);
						if (pCurrDir)
							DisposePtr((Ptr)pCurrDir);
						return err;
					}
				}
				
				DisposePtr((Ptr)pCurrDir);
				pathpos = delim+1;
			}	
		}
	}
	
	if (currDir)
		free(currDir);
		
	return err;
}

OSErr
ForceMoveFile(short vRefNum, long parID, ConstStr255Param name, long newDirID)
{
	OSErr 	err = noErr;
	FSSpec 	tmpFSp;
	
	err = CatMove(vRefNum, parID, name, newDirID, nil);
	if (err == dupFNErr)
	{
		// handle for stomping over old file
		err = FSMakeFSSpec(vRefNum, newDirID, name, &tmpFSp);
		err = FSpDelete(&tmpFSp);
		err = CatMove(vRefNum, parID, name, newDirID, nil);
	}
	
	return err;		
}

OSErr
CleanupExtractedFiles(short tgtVRefNum, long tgtDirID)
{
	OSErr		err = noErr;
	FSSpec		coreDirFSp;
	StringPtr	pcoreDir = nil;
	short		i = 0;
	
	HLock(gControls->cfg->coreDir);
	if (*gControls->cfg->coreDir != NULL && **gControls->cfg->coreDir != NULL)		
	{
		// just need to delete the core dir and its contents
		
		pcoreDir = CToPascal(*gControls->cfg->coreDir);
		err = FSMakeFSSpec(tgtVRefNum, tgtDirID, pcoreDir, &coreDirFSp);
		if (err == noErr)
		{
			err = FSpDelete( &coreDirFSp );
		}
	
		HUnlock(gControls->cfg->coreDir);
		goto aurevoir;
	}	
		
	HUnlock(gControls->cfg->coreDir);
	
	// otherwise iterate through coreFileList deleteing each individually
	for (i=0; i<currCoreFile+1; i++)
	{
		FSpDelete( &coreFileList[i] );
	}

aurevoir:
	if (pcoreDir)
		DisposePtr((Ptr) pcoreDir);	
	return err;
}
