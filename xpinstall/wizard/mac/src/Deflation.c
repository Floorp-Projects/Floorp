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

static FSSpec 	coreFileList[kMaxCoreFiles];
static short	currCoreFile = 0;

/*-----------------------------------------------------------*
 *   Deflation
 *-----------------------------------------------------------*/

OSErr
ExtractCoreFile(void)
{
	OSErr 					err = noErr;
	StringPtr 				coreFile, coreDirPath, extractedFile, pdir;
 	short 					fullPathLen, tgtVRefNum;
 	Handle 					fullPathH;
 	void					*hZip, *hFind;
 	PRInt32					rv;
	Boolean 				bFoundAll = false, isDir;
 	char					filename[255] = "\0", dir[255] = "\0", *lastslash;
 	FSSpec					coreDirFSp, extractedFSp, outFSp, extractedDir;
 	long					coreDirID, tgtDirID, extractedDirID;
 	
 	// err = GetCWD(&tgtDirID, &tgtVRefNum);
 	tgtDirID = gControls->opt->dirID;
 	tgtVRefNum = gControls->opt->vRefNum;
 	
	/* if there's a core file... */
	HLockHi(gControls->cfg->coreFile);
	if (*gControls->cfg->coreFile != NULL)
	{
		/* make local copy and unlock handle */
		coreFile = CToPascal(*gControls->cfg->coreFile);
		if (!coreFile)
		{
			err= memFullErr;
			goto cleanup;
		}
	}
	else
		return fnfErr;
	HUnlock(gControls->cfg->coreFile);
	
	/* if there's a relative subdir... */
	HLock(gControls->cfg->coreDir);
	if (*gControls->cfg->coreDir != NULL)
	{
		coreDirPath = CToPascal(*gControls->cfg->coreDir);
		if (!coreDirPath)
		{
			err = memFullErr;
			goto cleanup;
		}
	}		
	HUnlock(gControls->cfg->coreDir);
	
	// if coreDir specified create the core dir
	if (coreDirPath[0] > 0)
	{
		err = FSMakeFSSpec( tgtVRefNum, tgtDirID, coreDirPath, &coreDirFSp );
		if (err!=noErr && err!=fnfErr)
			return err;
		err = FSpDirCreate( &coreDirFSp, smSystemScript, &coreDirID );
		if (err!=noErr && err!=dupFNErr)
			return err;
		
		// move core file to core dir
		ERR_CHECK_RET(ForceMoveFile(tgtVRefNum, tgtDirID, coreFile, coreDirID), err);			
			
		ERR_CHECK_RET(GetFullPath(tgtVRefNum, coreDirID, coreFile, &fullPathLen, &fullPathH), err);
	}
	else
	{
		ERR_CHECK_RET(GetFullPath(tgtVRefNum, tgtDirID, coreFile, &fullPathLen, &fullPathH), err);
	}

	HLock(fullPathH);
	*(*fullPathH+fullPathLen) = '\0';
	rv = ZIP_OpenArchive( *fullPathH, &hZip );
	HUnlock(fullPathH);
	if (rv!=ZIP_OK) return rv;
	
	hFind = ZIP_FindInit( hZip, NULL ); /* NULL to match all files in archive */
	while (!bFoundAll)
	{
		rv = ZIP_FindNext( hFind, filename, 255 );
		if (rv==ZIP_ERR_FNF)
		{
			bFoundAll = true;
			break;
		}
		else if (rv!=ZIP_OK)
			return rv;
		
		lastslash = strrchr(filename, '/');
		if (lastslash == (&filename[0] + strlen(filename) - 1)) /* dir entry encountered */
			continue;
		
		// extract the file
		err = GetFullPath(tgtVRefNum, tgtDirID, "\p", &fullPathLen, &fullPathH); /* get dirpath */
		if (err!=noErr)
			return err;
		HLock(fullPathH);
		strcat(*fullPathH, filename);	/* tack on filename to dirpath */
		fullPathLen += strlen(filename);
		*(*fullPathH+fullPathLen) = '\0';
		rv = ZIP_ExtractFile( hZip, filename, *fullPathH );
		HUnlock(fullPathH);
		if (rv!=ZIP_OK)
			return rv;
		
		// AppleSingle decode if need be
		extractedFile = CToPascal(filename); 
		err = FSMakeFSSpec(tgtVRefNum, tgtDirID, extractedFile, &extractedFSp);
		err = FSMakeFSSpec(tgtVRefNum, tgtDirID, extractedFile, &outFSp);
		err = AppleSingleDecode(&extractedFSp, &outFSp);
		
		// check if file is leaf or nested in subdir
		dir[0] = 0;
		ResolveDirs(filename, dir);
		if (*dir)
		{
			pdir = CToPascal(dir);
			
			if (!pstrcmp(extractedFSp.name, outFSp.name))
			{
				err = FSpDelete(&extractedFSp);
			}
		}
		
		// if there's a coreDir specified move the file into it
		if (coreDirPath[0] > 0) 
		{
			if (*dir) 		// coreDir:extractedDir:<leaffile>
			{
				err = DirCreate(coreDirFSp.vRefNum, coreDirID, pdir, &extractedDirID);
				if (err==noErr)
					FSMakeFSSpec(outFSp.vRefNum, coreDirID, pdir, &coreFileList[currCoreFile]); // track for deletion
				else if (err!=dupFNErr)
					goto cleanup;
				ERR_CHECK_RET(ForceMoveFile(outFSp.vRefNum, outFSp.parID, outFSp.name, extractedDirID), err);
				FSMakeFSSpec(outFSp.vRefNum, extractedDirID, outFSp.name, &coreFileList[currCoreFile]);
			}
			else 			// else coreDir:<leaffile>
			{
				ERR_CHECK_RET(ForceMoveFile(outFSp.vRefNum, outFSp.parID, outFSp.name, coreDirID), err);
				FSMakeFSSpec(outFSp.vRefNum, coreDirID, outFSp.name, &coreFileList[currCoreFile]); 
			}
		}		
		else if (*dir)		// extractedDir:<leaffile>
		{
			err = FSMakeFSSpec(tgtVRefNum, tgtDirID, pdir, &extractedDir);
			if (err==noErr) // already created
				err = FSpGetDirectoryID(&extractedDir, &extractedDirID, &isDir);
			else			// otherwise mkdir
			{
				err = FSpDirCreate(&extractedDir, smSystemScript, &extractedDirID);
				FSMakeFSSpec(tgtVRefNum, tgtDirID, pdir, &coreFileList[currCoreFile]); // track for deletion
			}
			if (err!=noErr && err!=dupFNErr)
				goto cleanup;
				
			ERR_CHECK_RET(ForceMoveFile(outFSp.vRefNum, outFSp.parID, outFSp.name, extractedDirID), err);
			FSMakeFSSpec(outFSp.vRefNum, extractedDirID, outFSp.name, &coreFileList[currCoreFile]);
		}
		else				// just cwd:<leaffile>
		{
			FSMakeFSSpec(outFSp.vRefNum, outFSp.parID, outFSp.name, &coreFileList[currCoreFile]);
		}
		
		if (*dir && pdir)
			DisposePtr((Ptr) pdir);
		if (extractedFile)
			DisposePtr((Ptr)extractedFile);
		
		currCoreFile++;
	}
	
cleanup:							
	// dispose of coreFile, coreDirPath
	if (coreFile)
		DisposePtr((Ptr) coreFile);
	if (coreDirPath)
		DisposePtr((Ptr) coreDirPath);
	return err;	
}

OSErr
AppleSingleDecode(FSSpecPtr fd, FSSpecPtr outfd)
{
	OSErr	err = noErr;
	nsAppleSingleDecoder *decoder;
	
	// if file is AppleSingled
	if (nsAppleSingleDecoder::IsAppleSingleFile(fd))
	{
		// decode it
		decoder = new nsAppleSingleDecoder(fd, outfd);
		if (!decoder)
			return memFullErr;
			
		ERR_CHECK_RET(decoder->Decode(), err);
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
		*(dir+  (dirpath-fname)+1) = 0; // NULL terminate
	}
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
		err = FSMakeFSSpec(vRefNum, parID, name, &tmpFSp);
		err = FSpDelete(&tmpFSp);
		err = CatMove(vRefNum, parID, name, newDirID, nil);
	}
	
	return err;		
}

OSErr
CleanupExtractedFiles(void)
{
	OSErr		err = noErr;
	FSSpec		coreDirFSp;
	StringPtr	pcoreDir;
	short		i = 0;
	
	HLock(gControls->cfg->coreDir);
	if (*gControls->cfg->coreDir != NULL && **gControls->cfg->coreDir != NULL)		
	{
		// just need to delete the core dir and its contents
		
		pcoreDir = CToPascal(*gControls->cfg->coreDir);
		err = FSMakeFSSpec(gControls->opt->vRefNum, gControls->opt->dirID, pcoreDir, &coreDirFSp);
		if (err == noErr)
		{
			err = FSpDelete( &coreDirFSp );
			return err;
		}
		else
			return err;
	}		
	HUnlock(gControls->cfg->coreDir);
	
	// otherwise iterate through coreFileList deleteing each individually
	for (i=0; i<currCoreFile+1; i++)
	{
		FSpDelete( &coreFileList[i] );
	}

	if (pcoreDir)
		DisposePtr((Ptr) pcoreDir);	
	return err;
}