/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Bragg <dbragg@netscape.com>
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <MacTypes.h>
#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "FullPath.h"  
#include <AppleEvents.h>
#include <Gestalt.h>
#include <TextUtils.h>
#include <Folders.h>
#include <Processes.h>
#include <Resources.h>
#include <Aliases.h>

#include "InstallCleanup.h"
#include "InstallCleanupDefines.h"

#define kSleepMax 60  // sleep 1 second

Boolean gAppleEventsFlag, gQuitFlag;
long gSleepVal;


int   strcasecmp(const char *str1, const char *str2);
OSErr GetFSSpecFromPath(char *aPath, FSSpec *aSpec);
void  my_c2pstrcpy(Str255 aDstPStr, const char *aSrcCStr);
OSErr GetCWD(short *aVRefNum, long *aDirID);
OSErr GetCleanupReg(FSSpec *aCleanupReg);

int strcasecmp(const char *str1, const char *str2)
{
    char    currentChar1, currentChar2;

    while (1) {
    
        currentChar1 = *str1;
        currentChar2 = *str2;
        
        if ((currentChar1 >= 'a') && (currentChar1 <= 'z'))
            currentChar1 += ('A' - 'a');
        
        if ((currentChar2 >= 'a') && (currentChar2 <= 'z'))
            currentChar2 += ('A' - 'a');
                
        if (currentChar1 == '\0')
            break;
    
        if (currentChar1 != currentChar2)
            return currentChar1 - currentChar2;
            
        str1++;
        str2++;
    
    }
    
    return currentChar1 - currentChar2;
}

OSErr GetFSSpecFromPath(const char *aPath, FSSpec *aSpec)
{
    if (!aPath || !aSpec)
        return paramErr;
        
    // 1> verify path is not an empty string
    // 2> verify path has leaf
    // 3> verify path does not start with leading ':'
    
    if ((!*aPath) || 
       (*(aPath + strlen(aPath) - 1) == ':') ||
       (*aPath == ':'))
    {
       return paramErr;
    }
    
    // path is kosher: get FSSpec for it
    return FSpLocationFromFullPath(strlen(aPath), (const void *) aPath, aSpec);
}

void
my_c2pstrcpy(Str255 aDstPStr, const char *aSrcCStr)
{
    if (!aSrcCStr)
        return;
    
    memcpy(&aDstPStr[1], aSrcCStr, strlen(aSrcCStr) > 255 ? 255 : strlen(aSrcCStr));
    aDstPStr[0] = strlen(aSrcCStr);
}

OSErr
GetCWD(short *aVRefNum, long *aDirID)
{
    OSErr               err = noErr;
    ProcessSerialNumber psn;
    ProcessInfoRec      pInfo;
    FSSpec              tmp;
        
    if (!aVRefNum || !aDirID)
        return paramErr;
    
    *aVRefNum = 0;
    *aDirID = 0;
    
    /* get cwd based on curr ps info */
    if (!(err = GetCurrentProcess(&psn))) 
    {
        pInfo.processName = nil;
        pInfo.processAppSpec = &tmp;
        pInfo.processInfoLength = (sizeof(ProcessInfoRec));
             
        if(!(err = GetProcessInformation(&psn, &pInfo)))
        {   
            *aVRefNum = pInfo.processAppSpec->vRefNum;
            *aDirID = pInfo.processAppSpec->parID; 
        }
    }
      
    return err;
}

OSErr
GetCleanupReg(FSSpec *aCleanupReg)
{
    OSErr err = noErr;
    short efVRefNum = 0;
    long efDirID = 0;
    
    if (!aCleanupReg)
        return paramErr;
        
    err = GetCWD(&efVRefNum, &efDirID);
    if (err == noErr)
    {
        Str255 pCleanupReg;
        my_c2pstrcpy(pCleanupReg, CLEANUP_REGISTRY);
        err = FSMakeFSSpec(efVRefNum, efDirID, pCleanupReg, aCleanupReg);
    }
    
    return err;
}


#pragma mark -

//----------------------------------------------------------------------------
// Native Mac file deletion function
//----------------------------------------------------------------------------
int NativeDeleteFile(const char* aFileToDelete)
{
    OSErr err;
    FSSpec delSpec;
    
    if (!aFileToDelete)
        return DONE;
        
    // stat the file
    err = GetFSSpecFromPath(aFileToDelete, &delSpec);
    if (err != noErr)
    {
        // return fine if it doesn't exist
        return DONE;
    }
        
    // else try to delete it
    err = FSpDelete(&delSpec);
    if (err != noErr)
    {
        // tell user to try again later if deletion failed
        return TRY_LATER;
    }

    return DONE;
}

//----------------------------------------------------------------------------
// Native Mac file replacement function
//----------------------------------------------------------------------------
int NativeReplaceFile(const char* aReplacementFile, const char* aDoomedFile )
{
    OSErr err;
    FSSpec replSpec, doomSpec, tgtDirSpec;
    long dirID;
    Boolean isDir;
    
    if (!aReplacementFile || !aDoomedFile)
        return DONE;
        
    err = GetFSSpecFromPath(aReplacementFile, &replSpec);
    if (err != noErr)
        return DONE;
                      
    // stat replacement file
    err = FSpGetDirectoryID(&replSpec, &dirID, &isDir);
    if (err != noErr || isDir)
    {
        // return fine if it doesn't exist
        return DONE;
    }
        
    // check if the replacement file and doomed file are the same
    if (strcasecmp(aReplacementFile, aDoomedFile) == 0)
    {
        // return fine if they are the same
        return DONE;
    }
        
    // try and delete doomed file (NOTE: this call also stats)
    err = GetFSSpecFromPath(aDoomedFile, &doomSpec); 
    if (err == noErr)
    { 
        // (will even try to delete a dir with this name)
        err = FSpDelete(&doomSpec);
        
        // if deletion failed tell user to try again later
        if (err != noErr)
            return TRY_LATER;
    }
    
    // get the target dir spec (parent directory of doomed file)
    err = FSMakeFSSpec(doomSpec.vRefNum, doomSpec.parID, "\p", &tgtDirSpec);
    if (err == noErr)
    {
        // now try and move replacment file to path of doomed file
        err = FSpMoveRename(&replSpec, &tgtDirSpec, doomSpec.name);
        if (err != noErr)
        {
            // if move failed tell user to try agian later
            return TRY_LATER;
        }
    }
        
    return DONE;
}


#pragma mark -

//----------------------------------------------------------------------------
// Routines for recovery on reboot
//----------------------------------------------------------------------------
OSErr
GetProgramSpec(FSSpecPtr aProgSpec)
{
	OSErr 				err = noErr;
	ProcessSerialNumber	psn;
	ProcessInfoRec		pInfo;
	
	if (!aProgSpec)
	    return paramErr;
	    
	/* get cwd based on curr ps info */
	if (!(err = GetCurrentProcess(&psn))) 
	{
		pInfo.processName = nil;
		pInfo.processAppSpec = aProgSpec;
		pInfo.processInfoLength = (sizeof(ProcessInfoRec));
		
		err = GetProcessInformation(&psn, &pInfo);
	}
	
	return err;
}

void
PutAliasInStartupItems(FSSpecPtr aAlias)
{
    OSErr err;
    FSSpec fsProg, fsAlias;
    long strtDirID = 0;
    short strtVRefNum = 0;
    FInfo info;
    AliasHandle aliasH;

    if (!aAlias)
        return;
        
    // find cwd
    err = GetProgramSpec(&fsProg);
    if (err != noErr)
        return;  // fail silently
     
    // get startup items folder
    err = FindFolder(kOnSystemDisk, kStartupFolderType, kCreateFolder, 
                     &strtVRefNum, &strtDirID);
    if (err != noErr)
        return;
             
    // check that an alias to ourselves doesn't already
    // exist in the Startup Items folder
    err = FSMakeFSSpec(strtVRefNum, strtDirID, fsProg.name, &fsAlias);
    if (err == noErr)
    {
        // one's already there; not sure it's us so delete and recreate
        // (being super paranoid; but hey it's a mac)
        err = FSpDelete(&fsAlias);
        if (err != noErr)
            return;  // fail silently
    }
      
    // create the alias file
    err = NewAliasMinimal(&fsProg, &aliasH);
    if (err != noErr) 
        return;
        
    FSpGetFInfo(&fsProg, &info);
    FSpCreateResFile(&fsAlias, info.fdCreator, info.fdType, smRoman);
    short refNum = FSpOpenResFile(&fsAlias, fsRdWrPerm);
    if (refNum != -1)
    {
        UseResFile(refNum);
        AddResource((Handle)aliasH, rAliasType, 0, fsAlias.name);
        ReleaseResource((Handle)aliasH);
        UpdateResFile(refNum);
        CloseResFile(refNum);
    }
    else
    {
        ReleaseResource((Handle)aliasH);
        FSpDelete(&fsAlias);
        return;  // non-fatal error
    }

    // mark newly created file as an alias file
    FSpGetFInfo(&fsAlias, &info);
    info.fdFlags |= kIsAlias;
    FSpSetFInfo(&fsAlias, &info);    
    
    *aAlias = fsAlias;
}

void
RemoveAliasFromStartupItems(FSSpecPtr aAlias)
{
    // try to delete the alias
    FSpDelete(aAlias);
}


#pragma mark -

//----------------------------------------------------------------------------
// Apple event handlers to be installed
//----------------------------------------------------------------------------

static pascal OSErr DoAEOpenApplication(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
#pragma unused (theAppleEvent, replyAppleEvent, refCon)
    return noErr;
}

static pascal OSErr DoAEOpenDocuments(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
#pragma unused (theAppleEvent, replyAppleEvent, refCon)
    return errAEEventNotHandled;
}

static pascal OSErr DoAEPrintDocuments(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
#pragma unused (theAppleEvent, replyAppleEvent, refCon)
    return errAEEventNotHandled;
}

static pascal OSErr DoAEQuitApplication(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
#pragma unused (theAppleEvent, replyAppleEvent, refCon)
    gQuitFlag = true;
    return noErr;
}


//----------------------------------------------------------------------------
// install Apple event handlers
//----------------------------------------------------------------------------

static void InitAppleEventsStuff(void)
{
    OSErr retCode;

    if (gAppleEventsFlag) {

        retCode = AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
                    NewAEEventHandlerProc(DoAEOpenApplication), 0, false);

        if (retCode == noErr)
            retCode = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                    NewAEEventHandlerProc(DoAEOpenDocuments), 0, false);

        if (retCode == noErr)
            retCode = AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments,
                    NewAEEventHandlerProc(DoAEPrintDocuments), 0, false);
        if (retCode == noErr)
            retCode = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
                    NewAEEventHandlerProc(DoAEQuitApplication), 0, false);

        if (retCode != noErr) DebugStr("\pInstall event handler failed");
        // a better way to indicate an error is to post a notification
    }
}


//----------------------------------------------------------------------------
// high-level event dispatching
//----------------------------------------------------------------------------

static void DoHighLevelEvent(EventRecord * theEventRecPtr)
{
    (void) AEProcessAppleEvent(theEventRecPtr);
}


#pragma mark -

void main(void)
{
    OSErr retCode;
    long gestResponse;
    FSSpec aliasToSelf;
    FSSpec fsCleanupReg;

    EventRecord mainEventRec;
    Boolean eventFlag, bDone = false, bHaveCleanupReg = false;
    
    HREG reg;
    int rv = DONE;

    // initialize QuickDraw globals
    InitGraf(&qd.thePort);

    // initialize application globals
    gQuitFlag = false;
    gSleepVal = kSleepMax;

    // is the Apple Event Manager available?
    retCode = Gestalt(gestaltAppleEventsAttr, &gestResponse);
    if (retCode == noErr &&
        (gestResponse & (1 << gestaltAppleEventsPresent)) != 0)
        gAppleEventsFlag = true;
    else gAppleEventsFlag = false;

    // install Apple event handlers
    InitAppleEventsStuff();

    // put an alias to ourselves in the Startup Items folder
    // so that if we are shutdown before finishing we do our 
    // tasks at startup
    FSMakeFSSpec(0, 0, "\p", &aliasToSelf);  // initialize
    PutAliasInStartupItems(&aliasToSelf);
    
    if ( REGERR_OK == NR_StartupRegistry() )
    {
        char *regName = "";
        Boolean regNameAllocd = false;
        Handle pathH = 0;
        short pathLen = 0;
        
        // check if XPICleanup data file exists
        retCode = GetCleanupReg(&fsCleanupReg);
        if (retCode == noErr)
        {
            bHaveCleanupReg = true;
            
            // get full path to give to libreg open routine
            retCode = FSpGetFullPath(&fsCleanupReg, &pathLen, &pathH);
            if (retCode == noErr && pathH)
            {
                HLock(pathH);
                if (*pathH)
                {
                    regName = (char *) malloc(sizeof(char) * (pathLen + 1));
                    if (regName)
                        regNameAllocd = true;
                    else
                        retCode = memFullErr;
                    strncpy(regName, *pathH, pathLen);
                    *(regName + pathLen) = 0;
                }
                HUnlock(pathH);
                DisposeHandle(pathH);
            }
        }
            
        if ( (retCode == noErr) && (REGERR_OK == NR_RegOpen(regName, &reg)) )
        {
            // main event loop

            while (!gQuitFlag)
            {
                eventFlag = WaitNextEvent(everyEvent, &mainEventRec, gSleepVal, nil);

                if (mainEventRec.what == kHighLevelEvent)
                    DoHighLevelEvent(&mainEventRec);

                rv = PerformScheduledTasks(reg);
                if (rv == DONE)
                {
                    bDone = true;
                    gQuitFlag = true;
                }
            }
            NR_RegClose(&reg);
            NR_ShutdownRegistry();
        }
        
        if (regNameAllocd)
            free(regName);      
    }
    
    // clean up the alias to ouselves since we have 
    // completed our tasks successfully
    if (bDone)
    {
        if (bHaveCleanupReg)
            FSpDelete(&fsCleanupReg);
        RemoveAliasFromStartupItems(&aliasToSelf);
    }
}

