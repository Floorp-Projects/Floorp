/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include <string.h>

#include <Types.h>
#include <Files.h>
#include <Devices.h>
#include <Folders.h>
#include <Errors.h>
#include <Resources.h>
#include <Processes.h>
#include <TextUtils.h>

#include <fcntl.h>

#include "FullPath.h"		/* MoreFiles */

#include "primpl.h"
#include "MacErrorHandling.h"
#include "mdmac.h"

#include "macio.h"

/* forward declarations */
extern unsigned long gJanuaryFirst1970Seconds;

extern void WaitOnThisThread(PRThread *thread, PRIntervalTime timeout);
extern void DoneWaitingOnThisThread(PRThread *thread);
extern void AsyncNotify(PRThread *thread);


/* PB for Read and Write */
struct ExtendedParamBlock {
	/* PB must be first so that the file system can get the right data. */
 	ParamBlockRec 	pb;
	PRThread 		*thread;
};
typedef struct ExtendedParamBlock ExtendedParamBlock;


/* XXX Not done yet for 68K */
/* I/O completion routne for _MD_READ and _MD_WRITE */
static void AsyncIOCompletion (ExtendedParamBlock *pbAsyncPtr)
{
    _PRCPU *cpu = _PR_MD_CURRENT_CPU();
    PRThread *thread = pbAsyncPtr->thread;    
    PRIntn is;
    
    if (_PR_MD_GET_INTSOFF()) {
        thread->md.missedIONotify = PR_TRUE;
        cpu->u.missed[cpu->where] |= _PR_MISSED_IO;
        return;
    }

    _PR_INTSOFF(is);

    thread->md.osErrCode = noErr;
    DoneWaitingOnThisThread(thread);

    _PR_FAST_INTSON(is);
}

void  _MD_SetError(OSErr oserror)
{
    PRErrorCode code;

    switch (oserror) {
      case memFullErr:
        code = PR_OUT_OF_MEMORY_ERROR;
        break;
      case fnfErr:
        code = PR_FILE_NOT_FOUND_ERROR;
        break;
      case dupFNErr:
        code = PR_FILE_EXISTS_ERROR;
        break;
      case ioErr:
        code = PR_IO_ERROR;
        break;
      case nsvErr:
      case wrgVolTypErr:
        code = PR_INVALID_DEVICE_STATE_ERROR;
        break;
      case bdNamErr:
      case fsRnErr:
        code = PR_NAME_TOO_LONG_ERROR;
        break;
      case tmfoErr:
        code = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
      case opWrErr:
      case wrPermErr:
      case permErr:
      case afpAccessDenied:
        code = PR_NO_ACCESS_RIGHTS_ERROR;
        break;
      case afpObjectTypeErr:
        code = PR_DIRECTORY_LOOKUP_ERROR;
        break;
      case wPrErr:
      case vLckdErr:
        code = PR_DEVICE_IS_LOCKED_ERROR;
        break;
      case fLckdErr:
        code = PR_FILE_IS_LOCKED_ERROR;
        break;
      case dirNFErr:
        code = PR_NOT_DIRECTORY_ERROR;
        break;
      case dirFulErr:
        code = PR_MAX_DIRECTORY_ENTRIES_ERROR;
        break;
      case dskFulErr:
        code = PR_NO_DEVICE_SPACE_ERROR;
        break;
      case rfNumErr:
      case fnOpnErr:
        code = PR_BAD_DESCRIPTOR_ERROR;
        break;
      case eofErr:
        code = PR_END_OF_FILE_ERROR;
        break;
      case posErr:
      case gfpErr:
        code = PR_FILE_SEEK_ERROR;
        break;
      case fBsyErr:
        code = PR_FILE_IS_BUSY_ERROR;
        break;
      case extFSErr:
        code = PR_REMOTE_FILE_ERROR;
        break;
      case abortErr:
        code = PR_PENDING_INTERRUPT_ERROR;
        break;
      case paramErr:
        code = PR_INVALID_ARGUMENT_ERROR;
        break;
      case unimpErr:
        code = PR_NOT_IMPLEMENTED_ERROR;
        break;
    }

    PR_SetError(code, oserror);
}

void _MD_IOInterrupt(void)
{
    PRCList *qp;
    PRThread *thread, *me = _PR_MD_CURRENT_THREAD();

    PR_ASSERT(_PR_MD_GET_INTSOFF() != 0);

    _PR_SLEEPQ_LOCK(me->cpu);
    qp = _PR_PAUSEQ(me->cpu).next;
    while (qp != &_PR_PAUSEQ(me->cpu)) {

		thread = _PR_THREAD_PTR(qp);
		PR_ASSERT(thread->flags & _PR_ON_PAUSEQ);

		qp = qp->next;
		
		if (thread->md.missedIONotify) {
			thread->md.missedIONotify = PR_FALSE;
			DoneWaitingOnThisThread(thread);
		}

		if (thread->md.missedAsyncNotify) {
			thread->md.missedAsyncNotify = PR_FALSE;
			AsyncNotify(thread);
		}
    }
    qp = _PR_SLEEPQ(me->cpu).next;
    while (qp != &_PR_SLEEPQ(me->cpu)) {

		thread = _PR_THREAD_PTR(qp);
		PR_ASSERT(thread->flags & _PR_ON_SLEEPQ);

		qp = qp->next;
		
		if (thread->md.missedIONotify) {
			thread->md.missedIONotify = PR_FALSE;
			DoneWaitingOnThisThread(thread);
		}

		if (thread->md.missedAsyncNotify) {
			thread->md.missedAsyncNotify = PR_FALSE;
			AsyncNotify(thread);
		}
    }
	_PR_SLEEPQ_UNLOCK(thread->cpu);
}

/* 
** All PR_read and PR_Write calls are synchronous from caller's perspective.
** They are internally made asynchronous calls.  This gives cpu to other
** user threads while the async io is in progress.
*/
PRInt32 ReadWriteProc(PRFileDesc *fd, void *buf, PRUint32 bytes, IOOperation op)
{
	PRInt32 refNum = fd->secret->md.osfd;
	OSErr				err;
	ExtendedParamBlock 	pbAsync;
	PRThread			*me = _PR_MD_CURRENT_THREAD();
	_PRCPU *cpu = _PR_MD_CURRENT_CPU();

	/* quick hack to allow PR_fprintf, etc to work with stderr, stdin, stdout */
	/* note, if a user chooses "seek" or the like as an operation in another function */
	/* this will not work */
	if (refNum >= 0 && refNum < 3)
	{
		switch (refNum)
		{
				case 0:
					/* stdin - not on a Mac for now */
					err = paramErr;
					goto ErrorExit;
					break;
				case 1: /* stdout */
				case 2: /* stderr */
					puts(buf);
					break;
		}
		
		return (bytes);
	}
	else
	{
		static IOCompletionUPP	sCompletionUPP = NULL;
		
		PRBool  doingAsync = PR_FALSE;
		
		/* allocate the callback Universal Procedure Pointer (UPP). This actually allocates
		   a 32 byte Ptr in the heap, so only do this once
		*/
		if (!sCompletionUPP)
			sCompletionUPP = NewIOCompletionProc((IOCompletionProcPtr)&AsyncIOCompletion);
			
		/* grab the thread so we know which one to post to at completion */
		pbAsync.thread	= me;

		pbAsync.pb.ioParam.ioCompletion	= sCompletionUPP;
		pbAsync.pb.ioParam.ioResult		= noErr;
		pbAsync.pb.ioParam.ioRefNum		= refNum;
		pbAsync.pb.ioParam.ioBuffer		= buf;
		pbAsync.pb.ioParam.ioReqCount	= bytes;
		pbAsync.pb.ioParam.ioPosMode	= fsAtMark;
		pbAsync.pb.ioParam.ioPosOffset	= 0;

		/* 
		** Issue the async read call and wait for the io semaphore associated
		** with this thread.
		** Async file system calls *never* return error values, so ignore their
		** results (see <http://developer.apple.com/technotes/fl/fl_515.html>);
		** the completion routine is always called.
		*/
		me->io_fd = refNum;
		me->md.osErrCode = noErr;
		if (op == READ_ASYNC)
		{
			/*
			**  Skanky optimization so that reads < 20K are actually done synchronously
			**  to optimize performance on small reads (e.g. registry reads on startup)
			*/
			if ( bytes > 20480L )
			{
				doingAsync = PR_TRUE;
				me->io_pending = PR_TRUE;
				
				(void)PBReadAsync(&pbAsync.pb);
			}
			else
			{
				pbAsync.pb.ioParam.ioCompletion = NULL;
				me->io_pending = PR_FALSE;
				
				err = PBReadSync(&pbAsync.pb);
				if (err != noErr && err != eofErr)
					goto ErrorExit;
			}
		}
		else
		{
			doingAsync = PR_TRUE;
			me->io_pending = PR_TRUE;

			/* writes are currently always async */
			(void)PBWriteAsync(&pbAsync.pb);
		}
		
		if (doingAsync) {
			WaitOnThisThread(me, PR_INTERVAL_NO_TIMEOUT);
		}
	}
	
	err = me->md.osErrCode;
	if (err != noErr)
		goto ErrorExit;

	err = pbAsync.pb.ioParam.ioResult;
	if (err != noErr && err != eofErr)
		goto ErrorExit;
	
	return pbAsync.pb.ioParam.ioActCount;

ErrorExit:
	me->md.osErrCode = err;
	_MD_SetError(err);
	return -1;
}

/*
Special WriteSyncProc for logging only.  IO occurs synchronously.  Otherwise,
logging internal to NSPR causes ReadWriteProc above to recurse on PR_WaitSem logging.
*/
PRInt32 WriteSyncProc(PRFileDesc *fd, void *buf, PRUint32 bytes)
{
	PRInt32				refNum = fd->secret->md.osfd;
	OSErr				err;
 	ParamBlockRec 		pb;
	PRThread			*me = _PR_MD_CURRENT_THREAD();
	
	if (refNum >= 0 && refNum < 3)
	{
		PR_ASSERT(FALSE);	/* writing to these is hazardous to a Mac's health (refNum 2 is the system file) */
		err = paramErr;
		goto ErrorExit;
	}

	pb.ioParam.ioCompletion	= NULL;
	pb.ioParam.ioResult		= noErr;
	pb.ioParam.ioRefNum		= refNum;
	pb.ioParam.ioBuffer		= buf;
	pb.ioParam.ioReqCount	= bytes;
	pb.ioParam.ioPosMode	= fsAtMark;
	pb.ioParam.ioPosOffset	= 0;

	err = PBWriteSync(&pb);

	if (err != noErr)
		goto ErrorExit;
	else
		return pb.ioParam.ioActCount;

ErrorExit:
	me->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

/* File I/O functions called by PR I/O routines */
PRInt32 _MD_Open(const char *path, PRIntn flags, int mode)
{
// Macintosh doesn't really have mode bits, just drop them
#pragma unused (mode)

	OSErr 				err;
 	HParamBlockRec 		hpb;
 	ParamBlockRec 		pb;
	char	 			*macFileName = NULL;
	Str255				pascalName;
	PRInt8 				perm;

    err = ConvertUnixPathToMacPath(path, &macFileName);
	
	if (err != noErr)
		goto ErrorExit;

	hpb.ioParam.ioCompletion	= NULL;
	PStrFromCStr(macFileName, pascalName);
	PR_DELETE(macFileName);
	hpb.ioParam.ioNamePtr 	= pascalName;
	hpb.ioParam.ioVRefNum 	= 0;
	hpb.ioParam.ioVersNum 	= 0;
	hpb.fileParam.ioDirID	= 0;

	if (flags & PR_RDWR)
		perm = fsRdWrPerm;
	else if (flags & PR_WRONLY)
		perm = fsWrPerm;
	else
		perm = fsRdPerm;	
	hpb.ioParam.ioPermssn 	= perm;

	
    if (flags & PR_CREATE_FILE) {
		err = PBHCreateSync(&hpb);
               
       /* If opening with the PR_EXCL flag the existence of the file prior to opening is an error */
       if ((flags & PR_EXCL) &&  (err == dupFNErr)) {
           err = PR_FILE_EXISTS_ERROR;
           goto ErrorExit;
       }
       
       if ((err != noErr) && (err != dupFNErr))
          goto ErrorExit;
	}
 
    err = PBHOpenDFSync(&hpb);

	if (err != noErr)
		goto ErrorExit;

	if (flags & PR_TRUNCATE) {
		pb.ioParam.ioCompletion = NULL;
		pb.ioParam.ioRefNum = hpb.ioParam.ioRefNum;
		pb.ioParam.ioMisc = NULL;
		err = PBSetEOFSync(&pb);
		if (err != noErr)
			goto ErrorExit;
	} else if (flags & PR_APPEND) {
		pb.ioParam.ioCompletion = NULL;
		pb.ioParam.ioRefNum = hpb.ioParam.ioRefNum;
		pb.ioParam.ioPosMode = fsFromLEOF;
		pb.ioParam.ioPosOffset = 0;
		err = PBSetFPosSync(&pb);
		if (err != noErr)
			goto ErrorExit;
	}
	return hpb.ioParam.ioRefNum;
		
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

/* _MD_CLOSE_FILE, _MD_READ, _MD_WRITE, _MD_GET_FILE_ERROR are defined in _macos.h */

PROffset32 _MD_LSeek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence how)
{
	PRInt32 refNum = fd->secret->md.osfd;
	OSErr 	err = noErr;
	long	curPos, endPos;

	/* compute new mark */
	switch (how) {
		case PR_SEEK_SET:
			endPos = offset;
			break;
		
		case PR_SEEK_CUR:
			err = GetFPos(refNum, &curPos);
			endPos = curPos + offset;
			break;
		
		case PR_SEEK_END:
			err = GetEOF(refNum, &curPos);
			endPos = curPos + offset;
			break;

		default:
			err = paramErr;
			break;
	}

	/* set the new mark and extend the file if seeking beyond current EOF */
	/* making sure to set the mark after any required extend */
	if (err == noErr) {
		err = SetFPos(refNum, fsFromStart, endPos);
		if (err == eofErr) {
			err = SetEOF(refNum, endPos);
			if (err == noErr) {
				err = SetFPos(refNum, fsFromStart, endPos);
			}
		}
	}

	if (err == noErr) {
		return endPos;
	} else {
		_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	    _MD_SetError(err);
		return -1;
	}
}

PRInt32 _MD_FSync(PRFileDesc *fd)
{
	PRInt32 refNum = fd->secret->md.osfd;
	OSErr 	err;
 	ParamBlockRec 		pb;

	pb.ioParam.ioCompletion		= NULL;
	pb.ioParam.ioRefNum 		= refNum;

	err = PBFlushFileSync(&pb);
	if (err != noErr)
		goto ErrorExit;
		
	return 0;

ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;	
}

#include "plstr.h"

PRStatus _MD_OpenDir(_MDDir *mdDir,const char *name)
{
	// Emulate the Unix opendir() routine.

	OSErr 				err;
 	CInfoPBRec 			pb;
	char				*macDirName = NULL;
	char				*position = NULL;
	char				volumeName[32];
	Str255				pascalName;

	// Get the Macintosh path
	err = ConvertUnixPathToMacPath(name, &macDirName);
	if (err != noErr)
		goto ErrorExit;
	
	// Get the vRefNum
	position = PL_strchr(macDirName, PR_PATH_SEPARATOR);
	if ((position == macDirName) || (position == NULL))
		mdDir->ioVRefNum = 0;										// Use application relative searching
	else {
		memset(volumeName, 0, sizeof(volumeName));
		strncpy(volumeName, macDirName, position-macDirName);
		mdDir->ioVRefNum = GetVolumeRefNumFromName(volumeName);
	}

	// Get info about the object.
	PStrFromCStr(macDirName, pascalName);
	PR_DELETE(macDirName);
	
	pb.dirInfo.ioNamePtr = pascalName;
	pb.dirInfo.ioVRefNum = mdDir->ioVRefNum;
	pb.dirInfo.ioDrDirID = 0;
	pb.dirInfo.ioFDirIndex = 0;
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto ErrorExit;
		
	// Are we dealing with a directory?
	if ((pb.dirInfo.ioFlAttrib & ioDirMask) == 0) {
		err = dirNFErr;
		goto ErrorExit;
	}
	
	/* This is a directory, store away the pertinent information.
	** We post increment.  I.e. index is always the nth. item we 
	** should get on the next call
	*/
	mdDir->ioDirID = pb.dirInfo.ioDrDirID;
	mdDir->currentEntryName = NULL;
	mdDir->ioFDirIndex = 1;
	return PR_SUCCESS;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return PR_FAILURE;
}

char *_MD_ReadDir(_MDDir *mdDir, PRIntn flags)
{
	// Emulate the Unix readdir() routine.

	// Mac doesn’t have the concept of .(PR_SKIP_DOT) & ..(PR_SKIP_DOT_DOT)

	OSErr 				err;
	CInfoPBRec			pb;
	char				*returnedCStr;
	Str255				pascalName = "\p";
	PRBool				foundEntry;
	
	PR_ASSERT(mdDir != NULL);

	do {

	// Release the last name read.
	PR_DELETE(mdDir->currentEntryName);
	mdDir->currentEntryName = NULL;
		
	// We’ve got all the info we need, just get info about this guy.
	pb.hFileInfo.ioNamePtr = pascalName;
	pb.hFileInfo.ioVRefNum = mdDir->ioVRefNum;
	pb.hFileInfo.ioFDirIndex = mdDir->ioFDirIndex;
	pb.hFileInfo.ioDirID = mdDir->ioDirID;
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto ErrorExit;
	
	// Convert the Pascal string to a C string (actual allocation occurs in CStrFromPStr)
	CStrFromPStr(pascalName, &returnedCStr);
	
	mdDir->currentEntryName = returnedCStr;
	mdDir->ioFDirIndex++;
	
	// If it is not a hidden file and the flags did not specify skipping, we are done.
	if ((flags & PR_SKIP_HIDDEN) && (pb.hFileInfo.ioFlFndrInfo.fdFlags & fInvisible))
		foundEntry = PR_FALSE;
	else
		foundEntry = PR_TRUE;	
	
	} while (!foundEntry);
	
	return (mdDir->currentEntryName);

ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return NULL;
}


void _MD_CloseDir(_MDDir *mdDir)
{
	// Emulate the Unix closedir() routine

	PR_DELETE(mdDir->currentEntryName);
}

PRInt32 _MD_MkDir(char *unixPath, PRIntn mode)
{
	HFileParam		fpb;
	Str255			pascalName = "\p";
	char			*cMacPath = NULL;
	OSErr			err;

	#pragma unused (mode)	// Mode is ignored on the Mac

	if (unixPath) {
    	err = ConvertUnixPathToMacPath(unixPath, &cMacPath);
		if (err != noErr)
			goto ErrorExit;

    	PStrFromCStr(cMacPath, pascalName);
		PR_DELETE(cMacPath);
		fpb.ioNamePtr = pascalName;
		fpb.ioVRefNum = 0;
		fpb.ioDirID = 0L;

		err = PBDirCreateSync((HParmBlkPtr)&fpb);
		if (err != noErr)
			goto ErrorExit;
	}

	return 0;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

PRInt32 _MD_Delete(char *unixPath)
{
	HFileParam		fpb;
	Str255			pascalName = "\p";
	char			*cMacPath = NULL;
	OSErr			err;

	if (unixPath) {
    	err = ConvertUnixPathToMacPath(unixPath, &cMacPath);
		if (err != noErr)
			goto ErrorExit;

    	PStrFromCStr(cMacPath, pascalName);
		PR_DELETE(cMacPath);
		fpb.ioNamePtr = pascalName;
		fpb.ioVRefNum = 0;
		fpb.ioDirID = 0L;

		err = PBHDeleteSync((HParmBlkPtr)&fpb);
		if (err != noErr)
			goto ErrorExit;
	}

	return 0;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

PRInt32 _MD_Rename(char *fromUnixPath, char *toUnixPath)
{
	OSErr			err;
	FSSpec			fromSpec;
	FSSpec			toSpec;
	FSSpec			destDirSpec;
	FSSpec			beforeRenameSpec;

	if (fromUnixPath && toUnixPath) {
    	err = ConvertUnixPathToFSSpec(fromUnixPath, &fromSpec);
		if (err != noErr)
			goto ErrorExit;

    	err = ConvertUnixPathToFSSpec(toUnixPath, &toSpec);
		if (err != noErr && err != fnfErr)
			goto ErrorExit;

    	/* make an FSSpec for the destination directory */
		err = FSMakeFSSpec(toSpec.vRefNum, toSpec.parID, nil, &destDirSpec);
		if (err != noErr) /* parent directory must exist */
			goto ErrorExit;

		// move it to the directory specified
    	err = FSpCatMove(&fromSpec, &destDirSpec);
		if (err != noErr)
			goto ErrorExit;
	    
	    // make a new FSSpec for the file or directory in its new location	
		err = FSMakeFSSpec(toSpec.vRefNum, toSpec.parID, fromSpec.name, &beforeRenameSpec);
		if (err != noErr)
			goto ErrorExit;
    	
    	// rename the file or directory
    	err = FSpRename(&beforeRenameSpec, toSpec.name);
		if (err != noErr)
			goto ErrorExit;

	} else {
		err = paramErr;
		goto ErrorExit;
	}

	return 0;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

#define kWriteAccessAllowed (0x100)
PRInt32 _MD_Access(char *unixPath, int amode)
{
	//
	// Emulate the Unix access routine
	//
	
	OSErr			err;
	CInfoPBRec		pb;
	FCBPBRec		fcbpb;
	char			*cMacPath = NULL;
	Str255			pascalMacPath;
	struct stat		info;
	
	// Convert to a Mac style path
	err = ConvertUnixPathToMacPath(unixPath, &cMacPath);
	if (err != noErr)
		goto ErrorExit;
	
	err = stat(cMacPath, &info);
	if (err != noErr)
		goto ErrorExit;
	
	
	// If all we’re doing is checking for the existence of the file, we’re out of here.
	// On the Mac, if a file exists, you can read from it.
	// This doesn’t handle remote AppleShare volumes.  Does it need to?
	if ((amode == PR_ACCESS_EXISTS) || (amode == PR_ACCESS_READ_OK)) {
		goto success;
	}
	
	PStrFromCStr(cMacPath, pascalMacPath);
	
	pb.hFileInfo.ioNamePtr = pascalMacPath;
	pb.hFileInfo.ioVRefNum = info.st_dev;
	pb.hFileInfo.ioDirID = 0;
	pb.hFileInfo.ioFDirIndex = 0;
	
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto ErrorExit;
	// Check out all the access permissions.
	
	if (amode == PR_ACCESS_WRITE_OK) {
		fcbpb.ioNamePtr = NULL;
		fcbpb.ioVRefNum = pb.hFileInfo.ioVRefNum;
		fcbpb.ioRefNum = pb.hFileInfo.ioFRefNum;
		fcbpb.ioFCBIndx = 0;
	
		err = PBGetFCBInfoSync(&fcbpb);
		if (err != noErr)
			goto ErrorExit;
	
		/* Look at Inside Mac IV-180 */
		if ((fcbpb.ioFCBFlags & kWriteAccessAllowed) == 0) {
			err = permErr;
			goto ErrorExit;
		}
	}
	
success:
	PR_DELETE(cMacPath);
	return 0;
	
ErrorExit:
	if (cMacPath != NULL)
		PR_DELETE(cMacPath);
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

PRInt32 _MD_GetFileInfo(char *unixPath, PRFileInfo *info)
{
	CInfoPBRec		pb;
	OSErr			err;
	char			*cMacPath = NULL;
	Str255			pascalMacPath;
	PRTime			oneMillion, dateInMicroSeconds;
	
	// Convert to a Mac style path
	err = ConvertUnixPathToMacPath(unixPath, &cMacPath);
	if (err != noErr)
		goto ErrorExit;
	
	PStrFromCStr(cMacPath, pascalMacPath);
	PR_DELETE(cMacPath);
	
	pb.hFileInfo.ioNamePtr = pascalMacPath;
	pb.hFileInfo.ioVRefNum = 0;
	pb.hFileInfo.ioDirID = 0;
	pb.hFileInfo.ioFDirIndex = 0;
	
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto ErrorExit;
	
	if (pb.hFileInfo.ioFlAttrib & ioDirMask) {
		info->type = PR_FILE_DIRECTORY;
		info->size = 0;
	} else {
		info->type = PR_FILE_FILE;
		info->size = pb.hFileInfo.ioFlLgLen + pb.hFileInfo.ioFlRLgLen;
	}

	pb.hFileInfo.ioFlCrDat -= gJanuaryFirst1970Seconds;
	LL_I2L(dateInMicroSeconds, pb.hFileInfo.ioFlCrDat);
	LL_I2L(oneMillion, PR_USEC_PER_SEC);
	LL_MUL(info->creationTime, oneMillion, dateInMicroSeconds);

	pb.hFileInfo.ioFlMdDat -= gJanuaryFirst1970Seconds;
	LL_I2L(dateInMicroSeconds, pb.hFileInfo.ioFlMdDat);
	LL_MUL(info->modifyTime, oneMillion, dateInMicroSeconds);

	return 0;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

PRInt32 _MD_GetOpenFileInfo(const PRFileDesc *fd, PRFileInfo *info)
{
	OSErr			err;
	FCBPBRec		fcbpb;
	CInfoPBRec		pb;
	Str255			pascalMacPath;
	PRTime			oneMillion, dateInMicroSeconds;
	
	fcbpb.ioNamePtr = pascalMacPath;
	fcbpb.ioVRefNum = 0;
	fcbpb.ioRefNum = fd->secret->md.osfd;
	fcbpb.ioFCBIndx = 0;
	
	err = PBGetFCBInfoSync(&fcbpb);
	if (err != noErr)
		goto ErrorExit;
	
	info->type = PR_FILE_FILE;
	info->size = fcbpb.ioFCBEOF;

	pb.hFileInfo.ioNamePtr = pascalMacPath;
	pb.hFileInfo.ioVRefNum = fcbpb.ioFCBVRefNum;
	pb.hFileInfo.ioDirID = fcbpb.ioFCBParID;
	pb.hFileInfo.ioFDirIndex = 0;
	
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto ErrorExit;
	
	pb.hFileInfo.ioFlCrDat -= gJanuaryFirst1970Seconds;
	LL_I2L(dateInMicroSeconds, pb.hFileInfo.ioFlCrDat);
	LL_I2L(oneMillion, PR_USEC_PER_SEC);
	LL_MUL(info->creationTime, oneMillion, dateInMicroSeconds);

	pb.hFileInfo.ioFlMdDat -= gJanuaryFirst1970Seconds;
	LL_I2L(dateInMicroSeconds, pb.hFileInfo.ioFlMdDat);
	LL_MUL(info->modifyTime, oneMillion, dateInMicroSeconds);

	return 0;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

PRInt32 _MD_Stat(const char *path, struct stat *buf)
{
	OSErr	err;
	char	*macFileName = NULL;
	
    err = ConvertUnixPathToMacPath(path, &macFileName);
	if (err != noErr)
		goto ErrorExit;
	
	err = stat(macFileName, buf);
	if (err != noErr)
		goto ErrorExit;
		
	PR_DELETE(macFileName);
	
	return 0;

ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return -1;
}

PRStatus _MD_LockFile(PRInt32 fd)
{
	OSErr			err;
	FCBPBRec		fcbpb;
	HFileParam		fpb;
	Str255			pascalName;
	
	fcbpb.ioNamePtr = pascalName;
	fcbpb.ioVRefNum = 0;
	fcbpb.ioRefNum = fd;
	fcbpb.ioFCBIndx = 0;
	
	err = PBGetFCBInfoSync(&fcbpb);
	if (err != noErr)
		goto ErrorExit;
	
	fpb.ioCompletion = NULL;
	fpb.ioNamePtr = pascalName;
	fpb.ioVRefNum = fcbpb.ioFCBVRefNum;
	fpb.ioDirID = fcbpb.ioFCBParID;

	err = PBHSetFLockSync((HParmBlkPtr)&fpb);
	if (err != noErr)
		goto ErrorExit;
	
	return PR_SUCCESS;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return PR_FAILURE;
}

PRStatus _MD_TLockFile(PRInt32 fd)
{
	return (_MD_LockFile(fd));
}

PRStatus _MD_UnlockFile(PRInt32 fd)
{
	OSErr			err;
	FCBPBRec		fcbpb;
	HFileParam		fpb;
	Str255			pascalName;
	
	fcbpb.ioNamePtr = pascalName;
	fcbpb.ioVRefNum = 0;
	fcbpb.ioRefNum = fd;
	fcbpb.ioFCBIndx = 0;
	
	err = PBGetFCBInfoSync(&fcbpb);
	if (err != noErr)
		goto ErrorExit;
	
	fpb.ioCompletion = NULL;
	fpb.ioNamePtr = pascalName;
	fpb.ioVRefNum = fcbpb.ioFCBVRefNum;
	fpb.ioDirID = fcbpb.ioFCBParID;

	err = PBHRstFLockSync((HParmBlkPtr)&fpb);
	if (err != noErr)
		goto ErrorExit;
	
	return PR_SUCCESS;
	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return PR_FAILURE;
}

void SetLogFileTypeCreator(const char *logFile)
{
	HParamBlockRec pb;
	OSErr err;
	Str31 pName;

	PStrFromCStr(logFile, pName);
	pb.fileParam.ioCompletion = nil;
	pb.fileParam.ioNamePtr = pName;
	pb.fileParam.ioVRefNum = 0;
	pb.fileParam.ioFDirIndex = 0;
	pb.fileParam.ioDirID = 0;
	err = PBHGetFInfoSync(&pb);
	PR_ASSERT(err == noErr);

	pb.fileParam.ioDirID = 0;
	pb.fileParam.ioFlFndrInfo.fdType = 'TEXT';
	pb.fileParam.ioFlFndrInfo.fdCreator = 'ttxt';
	err = PBHSetFInfoSync(&pb);
	PR_ASSERT(err == noErr);
}

#if DEVELOPER_DEBUG
PR_IMPLEMENT (void)
SetupMacPrintfLog(char *logFile)
{
	/*
	 * We do _PR_InitLog() twice.  The first to force the implicit initialization which
	 * will set logging to highest levels in _MD_EARLY_INIT.  Then, change the env variable
	 * to disable kernel logging and call _PR_InitLog() again to make it effective.  Since
	 * we are using logging to log test program output, we disable kernel logging to avoid
	 * all Kernel logging output.
	 */
#ifdef PR_INTERNAL_LOGGING
	_PR_InitLog();
	_MD_PutEnv("NSPR_LOG_MODULES=clock:0,cmon:0,io:0,mon:0,linker:0,cvar:0,sched:0,thread:0");
	_PR_InitLog();
#endif
	PR_ASSERT(PR_SetLogFile(logFile) == PR_TRUE);
	
	SetLogFileTypeCreator(logFile);
}
#endif


/*
********************** Old name related stuff that is unchanged. **********************
*/

#if !defined(MAC_NSPR_STANDALONE)

short GetVolumeRefNumFromName(const char *cTgtVolName)
{
	OSErr				err;
	Str32				pVolName;
	char				*cVolName = NULL;
	HParamBlockRec		hPB;
	short				refNum = 0;
	
	hPB.volumeParam.ioVolIndex = 0;
	hPB.volumeParam.ioNamePtr = pVolName;
	do {
		hPB.volumeParam.ioVolIndex++;
		err = PBHGetVInfoSync(&hPB);
		CStrFromPStr(pVolName, &cVolName);
		if (strcmp(cTgtVolName, cVolName) == 0) {
			refNum =  hPB.volumeParam.ioVRefNum;
			PR_DELETE(cVolName);
			break;
		}
		PR_DELETE(cVolName);
	} while (err == noErr);
	
	return refNum;
}

static OSErr CreateMacPathFromUnixPath(const char *unixPath, char **macPath)
{
	// Given a Unix style path with '/' directory separators, this allocates 
	// a path with Mac style directory separators in the path.
	//
	// It does not do any special directory translation; use ConvertUnixPathToMacPath
	// for that.
	
	const char	*src;
	char		*tgt;
	OSErr		err = noErr;

	PR_ASSERT(unixPath != nil);
	if (nil == unixPath) {
		err = paramErr;
		goto exit;
	}

	// If unixPath is a zero-length string, we copy ":" into
	// macPath, so we need a minimum of two bytes to handle
	// the case of ":". 
	*macPath = malloc(strlen(unixPath) + 2);	// Will be enough extra space.
	require_action (*macPath != NULL, exit, err = memFullErr;);

	src = unixPath;
	tgt = *macPath;
	
	if (PL_strchr(src, PR_DIRECTORY_SEPARATOR) == src)				// If we’re dealing with an absolute
		src++;													// path, skip the separator
	else
		*(tgt++) = PR_PATH_SEPARATOR;	
		
	if (PL_strstr(src, UNIX_THIS_DIRECTORY_STR) == src)			// If it starts with /
		src += 2;												// skip it.
		
	while (*src) 
	{				// deal with the rest of the path
		if (PL_strstr(src, UNIX_PARENT_DIRECTORY_STR) == src) {	// Going up?
			*(tgt++) = PR_PATH_SEPARATOR;						// simply add an extra colon.
			src +=3;
		}
		else if (*src == PR_DIRECTORY_SEPARATOR) {					// Change the separator
			*(tgt++) = PR_PATH_SEPARATOR;
			src++;
		}
		else
			*(tgt++) = *(src++);
	}
	
	*tgt = NULL;							// make sure it’s null terminated.

exit:
	return err;
}


static ProcessInfoRec gNavigatorProcInfo;
static FSSpec gGutsFolder;
static FSSpec gNetscapeFolder;

static OSErr SetupRequiredFSSpecs(void)
{
	OSErr err;
	CInfoPBRec pb;
	ProcessSerialNumber curPSN = {0, kCurrentProcess};	
	
	gNavigatorProcInfo.processInfoLength = sizeof(ProcessInfoRec);
	gNavigatorProcInfo.processName = NULL;
	gNavigatorProcInfo.processAppSpec = &gNetscapeFolder;

	err = GetProcessInformation (&curPSN, &gNavigatorProcInfo);
	if (err != noErr)
		goto ErrorExit;

	/* guts folder resides at the same place as the app file itself */
	gGutsFolder = gNetscapeFolder;
	/* How else do we do this hack??? 
	 * Should NSPR have a string resource for this ?
	 */
	GetIndString( gGutsFolder.name, 300, 34);

	/* 
	 * vRefNum and parentDirID are now set up correctly for the app file itself. 
	 * parentDirID is the Netscape Folder's ID.  Then Find it's parent ID to
	 * set up the FSSpec and its own name.
	 */

	pb.dirInfo.ioCompletion = NULL;
	pb.dirInfo.ioNamePtr = gNetscapeFolder.name;
	pb.dirInfo.ioVRefNum = gNetscapeFolder.vRefNum;
	pb.dirInfo.ioFDirIndex = -1;
	pb.dirInfo.ioDrDirID = gNetscapeFolder.parID;
	
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto ErrorExit;
	
	gNetscapeFolder.parID = pb.dirInfo.ioDrParID;
	
	return noErr;	

ErrorExit:
	return err;
}

static OSErr FindGutsFolder(FSSpec *foundSpec)
{
	OSErr err;
	
	if (gNavigatorProcInfo.processInfoLength == 0) { /* Uninitialized? */
		err = SetupRequiredFSSpecs();
		if (err != noErr)
			goto ErrorExit;
	} 
	
	*foundSpec = gGutsFolder;
	
	return noErr;	

ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
    return err;	
}

static OSErr FindNetscapeFolder(FSSpec *foundSpec)
{
	OSErr err;
	
	if (gNavigatorProcInfo.processInfoLength == 0) { /* Uninitialized? */
		err = SetupRequiredFSSpecs();
		if (err != noErr)
			goto ErrorExit;
	} 
	
	*foundSpec = gNetscapeFolder;
	
	return noErr;	

ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
    return err;	
}


PR_IMPLEMENT (OSErr)
ConvertUnixPathToMacPath(const char *unixPath, char **macPath)
{	
		OSErr		err = noErr;
		
	//	******** HACK ALERT ********
	//
	//	Java really wants long file names (>31 chars).  We truncate file names 
	//	greater than 31 characters long.  Truncation is from the middle.
	//
	//	Convert UNIX style path names (with . and / separators) into a Macintosh
	//	style path (with :).
	//
	// There are also a couple of special paths that need to be dealt with
	// by translating them to the appropriate Mac special folders.  These include:
	//
	//			/usr/tmp/file  =>  {TempFolder}file
	//
	// The file conversions we need to do are as follows:
	//
	//			file			=>		file
	//			dir/file		=>		:dir:file
	//			./file			=>		file
	//			../file			=>		::file
	//			../dir/file		=>		::dir:file
	//			/file			=>		::BootDrive:file
	//			/dir/file		=>		::BootDrive:dir:file
	
	
	if (!strcmp(unixPath, "."))
	{
		*macPath = malloc(sizeof(":"));
		if (*macPath == NULL)
			err = memFullErr;
		(*macPath)[0] = ':';
		(*macPath)[1] = '\0';
	}
	else
	
	if (*unixPath != PR_DIRECTORY_SEPARATOR) {				// Not root relative, just convert it.
		err = CreateMacPathFromUnixPath(unixPath, macPath);
	}
	
	else {
		// We’re root-relative.  This is either a special Unix directory, or a 
		// full path (which we’ll support on the Mac since they might be generated).
		// This is not condoning the use of full-paths on the Macintosh for file 
		// specification.
		
		FSSpec		foundSpec;
		short		pathBufferSize;
		char		*temp;
		int		tempLen;

		// Are we dealing with the temp folder?
		if ((strncmp(unixPath, "/usr/tmp", strlen("/usr/tmp")) == 0) || 
			((strncmp(unixPath, "/tmp", strlen("/tmp")) == 0))) {
		    CInfoPBRec pb;

			unixPath = PL_strchr(unixPath, PR_DIRECTORY_SEPARATOR);
			if (strncmp(unixPath, "/tmp", strlen("/tmp")) == 0) // skip past temp spec
				unixPath += 5;	
			else
				unixPath += 9;	
							
			err = FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder,	// Create if needed
								&foundSpec.vRefNum, &foundSpec.parID);
			if (err == noErr) {
				pb.dirInfo.ioCompletion = NULL;
				pb.dirInfo.ioNamePtr = foundSpec.name;
				pb.dirInfo.ioVRefNum = foundSpec.vRefNum;
				pb.dirInfo.ioFDirIndex = -1;
				pb.dirInfo.ioDrDirID = foundSpec.parID;
				
				err = PBGetCatInfoSync(&pb);
				foundSpec.parID = pb.dirInfo.ioDrParID;
			}
		}
		
		else if (!strncmp(unixPath, "/usr/local/netscape/", (tempLen = strlen("/usr/local/netscape/")))) {
			
			unixPath += tempLen;
			
			if (!strncmp(unixPath, "RequiredGuts/", (tempLen = strlen("RequiredGuts/"))))
			{
				unixPath += tempLen;
				err = FindGutsFolder(&foundSpec);
			}
			else if (!strncmp(unixPath, "bin/", (tempLen = strlen("bin/"))))
			{
				unixPath += tempLen;
				err = FindNetscapeFolder(&foundSpec);
			}			
			else if (*unixPath == '\0')
			{
				// it's /usr/local/netscape
				err = FindGutsFolder(&foundSpec);
			}

		}
		
		else {
			// This is a root relative directory, we’ll just convert the whole thing.
			err = CreateMacPathFromUnixPath(unixPath, macPath);
			goto Exit_ConvertUnixPathToMacPath;
		}
	

		
		// We’re dealing with a special folder
		if (err == noErr)
		{
			Handle	hPathStr;
			// Get the path to the root-relative directory
			err = FSpGetFullPath(&foundSpec, &pathBufferSize, &hPathStr);		// NewHandle's hPathStr
			 
			if (noErr == err)
			{
				// convert handle to c-string
				// add one for NULL termination
				// pathBufferSize is now one greater than the length of the string
				pathBufferSize++;	
				
				*macPath = (char*) malloc(sizeof(char) * pathBufferSize);
				(*macPath)[pathBufferSize - 1] = '\0';
				BlockMoveData(*hPathStr, *macPath, pathBufferSize - 1);
			
				DisposeHandle(hPathStr);
			}
		}
		
		if (err == noErr)
		{
			UInt32	unixPathLeft;
			UInt32	macPathLen;

			unixPathLeft =  strlen(unixPath);
			macPathLen = strlen(*macPath);
			

			// copy over the remaining file name, converting
			if (pathBufferSize - 1 < macPathLen + unixPathLeft) 
			{
				// need to grow string
				*macPath = realloc(*macPath, macPathLen + unixPathLeft + 1);
				err = (*macPath == NULL ? memFullErr : noErr);
			}
			
			if (err == noErr)
			{
				// carefully remove the '/''s out of the unix path.  If we see an "escaped" /
				// we will leave it in there, otherwise we take it out and replace it with a :
				// we have to do this before we convert to a mac-path, so we can tell what is
				// really a path separator and what is in the name of a file or directory
				// Make sure that all of the /’s are :’s in the final pathname
				// effectively we do a
				// strcat(*macPath, unixPath); while replace all occurrences of / with : in unixPath
				char*		dp;
				const char*	sp;
				
				sp = unixPath;
				dp = *macPath + macPathLen;
				
				for (;*sp != '\0'; sp++, dp++) 
				{
					if (*sp == PR_DIRECTORY_SEPARATOR)
					{
						// if we can look at the previous character
						if (sp > unixPath)					
						{
							// check to see if previous character is an escape
							if (sp[-1] == '\\')
							{
								// leave it in, and cycle
								continue;
							}
							else
							{
								*dp = PR_PATH_SEPARATOR;
							}
						}
						else				
							*dp = PR_PATH_SEPARATOR;
					}
					else
					{
						// just copy;
						*dp = *sp;
					}
				}
				
				*dp = '\0';   // NULL terminate *macPath
			}
#if DEBUG	
			// we used to check here, now we check above, we leave this in
			// the debug build to make sure we didn't screw up
			// Make sure that all of the /’s are :’s in the final pathname
			for (temp = *macPath + strlen(*macPath) - strlen(unixPath); *temp != '\0'; temp++) {

				if (*temp == PR_DIRECTORY_SEPARATOR)
				{
					DebugStr("\pFound a slash");	
					*temp = PR_PATH_SEPARATOR;
				}
			}
#endif
		}
	}
	
	
Exit_ConvertUnixPathToMacPath:

	return err;
}

// Hey! Before you delete this "hack" you should look at how it's being
// used by sun-java/netscape/applet/appletStubs.c.
PR_IMPLEMENT (OSErr)
ConvertMacPathToUnixPath(const char *macPath, char **unixPath) 
{
	// *** HACK ***
	// Get minimal version working
	
	char		*unixPathPtr;
	
	*unixPath = malloc(strlen(macPath) + 2);	// Add one for the front slash, one for null
	if (*unixPath == NULL)
		return (memFullErr);
		
	unixPathPtr = *unixPath;
	
	*unixPathPtr++ = PR_DIRECTORY_SEPARATOR;
	
	do {
		// Translate all colons to slashes
		if (*macPath == PR_PATH_SEPARATOR)
			*unixPathPtr = PR_DIRECTORY_SEPARATOR;
		else
			*unixPathPtr = *macPath;

		unixPathPtr++;
		macPath++;
	} while (*macPath != NULL);
	
	// Terminate the string
	*unixPathPtr = '\0';
	
	return (noErr);
}

OSErr
ConvertUnixPathToFSSpec(const char *unixPath, FSSpec *fileSpec)
{
    char*                   macPath;
    OSErr                   convertError;
    int                             len;
    
    convertError = ConvertUnixPathToMacPath(unixPath, &macPath);
    if (convertError != noErr)
    	return convertError;

    len = strlen(macPath);

    if (*macPath == PR_PATH_SEPARATOR)
    {
        if (len < sizeof(Str255))
        {
            short   vRefNum;
            long    dirID;
            Str255  pascalMacPath;
            
            convertError = HGetVol(NULL, &vRefNum, &dirID);
            if (convertError == noErr)
            {
                PStrFromCStr(macPath, pascalMacPath);
                convertError = FSMakeFSSpec(vRefNum, dirID, pascalMacPath, fileSpec);
            }
        }
        else
            convertError = paramErr;
    }
    else
    {
    	convertError = FSpLocationFromFullPath(len, macPath, fileSpec);
	    if (convertError == fnfErr)
	    {
			CInfoPBRec		pb;
			Str255	pascalMacPath;
			OSErr err;
			
			PStrFromCStr(macPath, pascalMacPath);
			/* 
			FSpLocationFromFullPath does not work for directories unless there is
			a ":" at the end.  We will make sure of an existence of a directory.
			If so, the returned fileSpec is valid from FSpLocationFromFullPath eventhough
			it returned an error.
			*/
			pb.hFileInfo.ioNamePtr = pascalMacPath;
			pb.hFileInfo.ioVRefNum = 0;
			pb.hFileInfo.ioDirID = 0;
			pb.hFileInfo.ioFDirIndex = 0;
			
			err = PBGetCatInfoSync(&pb);
			if (err == noErr)
				convertError = noErr;
		}
    }
    
    free(macPath);
    
    return (convertError);
}
 

FILE *_OS_FOPEN(const char *filename, const char *mode) 
{
	OSErr	err = noErr;
	char	*macFileName = NULL;
	FILE	*result;
	
    err = ConvertUnixPathToMacPath(filename, &macFileName);
	if (err != noErr)
		goto ErrorExit;
	
	result = fopen(macFileName, mode);
		
	PR_DELETE(macFileName);
	
	return result;

ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = err;
	_MD_SetError(err);
    return NULL;
}

#else

short GetVolumeRefNumFromName(const char *cTgtVolName)
{
	OSErr				err;
	Str32				pVolName;
	char				*cVolName = NULL;
	HParamBlockRec		hPB;
	short				refNum = 0;
	
	hPB.volumeParam.ioVolIndex = 0;
	hPB.volumeParam.ioNamePtr = pVolName;
	do {
		hPB.volumeParam.ioVolIndex++;
		err = PBHGetVInfoSync(&hPB);
		CStrFromPStr(pVolName, &cVolName);
		if (strcmp(cTgtVolName, cVolName) == 0) {
			refNum =  hPB.volumeParam.ioVRefNum;
			PR_DELETE(cVolName);
			break;
		}
		PR_DELETE(cVolName);
	} while (err == noErr);
	
	return refNum;
}



static OSErr GetFullPath(short vRefNum, long dirID, char **fullPath, int *strSize)
{
	Str255			pascalDirName;
	char			cDirName[256];
	char			*tmpPath = NULL;						// needed since sprintf isn’t safe
	CInfoPBRec		myPB;
	OSErr			err = noErr;
	
	
	// get the full path of the temp folder.
	*strSize = 256;
	*fullPath = NULL;
	*fullPath = malloc(*strSize);	// How big should this thing be?
	require_action (*fullPath != NULL, errorExit, err = memFullErr;);
		
	tmpPath = malloc(*strSize);
	require_action (tmpPath != NULL, errorExit, err = memFullErr;);

	strcpy(*fullPath, "");				// Clear C result
	strcpy(tmpPath, "");
	pascalDirName[0] = 0;				// Clear Pascal intermediate string
	
	myPB.dirInfo.ioNamePtr = &pascalDirName[0];
	myPB.dirInfo.ioVRefNum = vRefNum;
	myPB.dirInfo.ioDrParID = dirID;
	myPB.dirInfo.ioFDirIndex = -1;				// Getting info about
	
	do {
		myPB.dirInfo.ioDrDirID = myPB.dirInfo.ioDrParID;

		err = PBGetCatInfoSync(&myPB);
		require(err == noErr, errorExit);
			
		// Move the name into C domain
		memcpy(&cDirName, &pascalDirName, 256);
		p2cstr((unsigned char *)&cDirName);							// Changes in place!
		
		if ((strlen(cDirName) + strlen(*fullPath)) > *strSize) {
			// We need to grow the string, do it in 256 byte chunks
			(*strSize) += 256;										
			*fullPath = PR_REALLOC(*fullPath, *strSize);
			require_action (*fullPath != NULL, errorExit, err = memFullErr;);

			tmpPath = PR_REALLOC(tmpPath, *strSize);
			require_action (tmpPath != NULL, errorExit, err = memFullErr;);
		}
		sprintf(tmpPath, "%s:%s", cDirName, *fullPath);
		strcpy(*fullPath, tmpPath);
	} while (myPB.dirInfo.ioDrDirID != fsRtDirID);
	
	PR_DELETE(tmpPath);
	
	return noErr;
	
	
errorExit:
	PR_DELETE(*fullPath);
	PR_DELETE(tmpPath);
	
	return err;

}

static OSErr CreateMacPathFromUnixPath(const char *unixPath, char **macPath)
{
	// Given a Unix style path with '/' directory separators, this allocates 
	// a path with Mac style directory separators in the path.
	//
	// It does not do any special directory translation; use ConvertUnixPathToMacPath
	// for that.
	
	const char	*src;
	char		*tgt;
	OSErr		err = noErr;

	PR_ASSERT(unixPath != nil);
	if (nil == unixPath) {
		err = paramErr;
		goto exit;
	}

	// If unixPath is a zero-length string, we copy ":" into
	// macPath, so we need a minimum of two bytes to handle
	// the case of ":". 
	*macPath = malloc(strlen(unixPath) + 2);	// Will be enough extra space.
	require_action (*macPath != NULL, exit, err = memFullErr;);

	src = unixPath;
	tgt = *macPath;
	
	if (PL_strchr(src, PR_DIRECTORY_SEPARATOR) == src)				// If we’re dealing with an absolute
		src++;													// path, skip the separator
	else
		*(tgt++) = PR_PATH_SEPARATOR;	
		
	if (PL_strstr(src, UNIX_THIS_DIRECTORY_STR) == src)			// If it starts with ./
		src += 2;												// skip it.
		
	while (*src) 
	{				// deal with the rest of the path
		if (PL_strstr(src, UNIX_PARENT_DIRECTORY_STR) == src) {	// Going up?
			*(tgt++) = PR_PATH_SEPARATOR;						// simply add an extra colon.
			src +=3;
		}
		else if (*src == PR_DIRECTORY_SEPARATOR) {					// Change the separator
			*(tgt++) = PR_PATH_SEPARATOR;
			src++;
		}
		else
			*(tgt++) = *(src++);
	}
	
	*tgt = NULL;							// make sure it’s null terminated.

exit:
	return err;
}

static OSErr ConvertUnixPathToMacPath(const char *unixPath, char **macPath)
{	
		OSErr		err = noErr;
		

	//
	//	Convert UNIX style path names (with . and / separators) into a Macintosh
	//	style path (with :).
	//
	// There are also a couple of special paths that need to be dealt with
	// by translating them to the appropriate Mac special folders.  These include:
	//
	//			/usr/tmp/file  =>  {TempFolder}file
	//
	// The file conversions we need to do are as follows:
	//
	//			file			=>		file
	//			dir/file		=>		:dir:file
	//			./file			=>		file
	//			../file			=>		::file
	//			../dir/file		=>		::dir:file
	//			/file			=>		::BootDrive:file
	//			/dir/file		=>		::BootDrive:dir:file
	
	
	if (*unixPath != PR_DIRECTORY_SEPARATOR) {				// Not root relative, just convert it.
		err = CreateMacPathFromUnixPath(unixPath, macPath);
	}
	
	else {
		// We’re root-relative.  This is either a special Unix directory, or a 
		// full path (which we’ll support on the Mac since they might be generated).
		// This is not condoning the use of full-paths on the Macintosh for file 
		// specification.
		
		short		foundVRefNum;
		long		foundDirID;
		int			pathBufferSize;
		char		*temp;
		char		isNetscapeDir = false;

		// Are we dealing with the temp folder?
		if (strncmp(unixPath, "/usr/tmp", strlen("/usr/tmp")) == 0){
			unixPath += 8;
			if (*unixPath == PR_DIRECTORY_SEPARATOR)
				unixPath++;														// Skip the slash
			err = FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder,	// Create if needed
								&foundVRefNum, &foundDirID);
		}
		
		if (strncmp(unixPath, "/tmp", strlen("/tmp")) == 0) {
			unixPath += 4;															// Skip the slash
			if (*unixPath == PR_DIRECTORY_SEPARATOR)
				unixPath++;														// Skip the slash
			err = FindFolder(kOnSystemDisk, kTemporaryFolderType, kCreateFolder,	// Create if needed
								&foundVRefNum, &foundDirID);
		}
		
		else if (strncmp(unixPath, "/usr", strlen("/usr")) == 0) {
		
			int		usrNetscapePathLen;
			
			usrNetscapePathLen = strlen("/usr/local/netscape/");
		
			if (strncmp(unixPath, "/usr/local/netscape/", usrNetscapePathLen) == 0) {
				unixPath += usrNetscapePathLen;				
//				err = FindPreferencesFolder(&foundVRefNum, &foundDirID);
				err = paramErr;
				isNetscapeDir = true;
			}
			
			else {
				dprintf("Unable to translate Unix file path %s to Mac path\n", unixPath);
				err = -1;
				goto Exit_ConvertUnixPathToMacPath;
			}

		}
		
		else {
			// This is a root relative directory, we’ll just convert the whole thing.
			err = CreateMacPathFromUnixPath(unixPath, macPath);
			goto Exit_ConvertUnixPathToMacPath;
		}
	
		// We’re dealing with a special folder
		if (err == noErr)
			// Get the path to the root-relative directory
			err = GetFullPath(foundVRefNum, foundDirID, macPath, &pathBufferSize);		// mallocs macPath
		
		if (err == noErr){
			
			// copy over the remaining file name, converting
			if (pathBufferSize < (strlen(*macPath) + strlen(unixPath))) {
				// need to grow string
				*macPath = PR_REALLOC(*macPath, (strlen(*macPath) + strlen(unixPath) + 
					(isNetscapeDir ? strlen("Netscape ƒ:") : 0)));
				err = (*macPath == NULL ? memFullErr : noErr);
			}
			
			if (isNetscapeDir)
				strcat(*macPath, "Netscape ƒ:");
		
			if (err == noErr)
				strcat(*macPath, unixPath);
			
			//	Make sure that all of the /’s are :’s in the final pathname
				
			for (temp = *macPath + strlen(*macPath) - strlen(unixPath); *temp != '\0'; temp++) {
				if (*temp == PR_DIRECTORY_SEPARATOR)
					*temp = PR_PATH_SEPARATOR;
			}

		}
	}
	
	
Exit_ConvertUnixPathToMacPath:

	return err;
}

OSErr
ConvertUnixPathToFSSpec(const char *unixPath, FSSpec *fileSpec)
{
    char*                   macPath;
    OSErr                   convertError;
    int                             len;
    
    convertError = ConvertUnixPathToMacPath(unixPath, &macPath);
    if (convertError != noErr)
    	return convertError;

    len = strlen(macPath);

    if (*macPath == PR_PATH_SEPARATOR)
    {
        if (len < sizeof(Str255))
        {
            short   vRefNum;
            long    dirID;
            Str255  pascalMacPath;
            
            convertError = HGetVol(NULL, &vRefNum, &dirID);
            if (convertError == noErr)
            {
                PStrFromCStr(macPath, pascalMacPath);
                convertError = FSMakeFSSpec(vRefNum, dirID, pascalMacPath, fileSpec);
            }
        }
        else
            convertError = paramErr;
    }
    else
    {
    	convertError = FSpLocationFromFullPath(len, macPath, fileSpec);
    }
    
    free(macPath);
    
    return (convertError);
}
 

#endif

/*
 **********************************************************************
 *
 * Memory-mapped files are not implementable on the Mac.
 *
 **********************************************************************
 */

PRStatus _MD_CreateFileMap(PRFileMap *fmap, PRInt64 size)
{
#pragma unused (fmap, size)

    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 _MD_GetMemMapAlignment(void)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
}

void * _MD_MemMap(
    PRFileMap *fmap,
    PROffset64 offset,
    PRUint32 len)
{
#pragma unused (fmap, offset, len)

    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PRStatus _MD_MemUnmap(void *addr, PRUint32 len)
{
#pragma unused (addr, len)

    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _MD_CloseFileMap(PRFileMap *fmap)
{
#pragma unused (fmap)

    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}
