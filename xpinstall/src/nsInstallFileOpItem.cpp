/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nspr.h"
#include "nsInstall.h"
#include "nsInstallFileOpEnums.h"
#include "nsInstallFileOpItem.h"
#include "ScheduledTasks.h"

#ifdef _WINDOWS
#include "nsWinShortcut.h"
#endif

#ifdef XP_MAC
#include "Aliases.h"
#include "Gestalt.h"
#include "Resources.h"
#include "script.h"
#endif

/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsInstallFileOpItem);

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         PRInt32        aFlags,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
    MOZ_COUNT_CTOR(nsInstallFileOpItem);

    *aReturn    = nsInstall::SUCCESS;
    mIObj       = aInstallObj;
    mCommand    = aCommand;
    mFlags      = aFlags;
    mSrc        = nsnull;
    mParams     = nsnull;
    mStrTarget  = nsnull;
    mShortcutPath = nsnull;
    mDescription  = nsnull;
    mWorkingPath  = nsnull;
    mParams       = nsnull;
    mIcon         = nsnull;

    mTarget     = new nsFileSpec(aTarget);

    if(mTarget == nsnull)
        *aReturn = nsInstall::OUT_OF_MEMORY;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aSrc,
                                         nsFileSpec&    aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
  MOZ_COUNT_CTOR(nsInstallFileOpItem);

  *aReturn    = nsInstall::SUCCESS;
  mIObj       = aInstallObj;
  mCommand    = aCommand;
  mFlags      = 0;
  mParams     = nsnull;
  mStrTarget  = nsnull;
  mAction     = ACTION_NONE;
  mShortcutPath = nsnull;
  mDescription  = nsnull;
  mWorkingPath  = nsnull;
  mParams       = nsnull;
  mIcon         = nsnull;

  mSrc        = new nsFileSpec(aSrc);
  mTarget     = new nsFileSpec(aTarget);

  if(mTarget == nsnull  || mSrc == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
  MOZ_COUNT_CTOR(nsInstallFileOpItem);

  *aReturn    = nsInstall::SUCCESS;
  mIObj       = aInstallObj;
	mCommand    = aCommand;
	mFlags      = 0;
	mSrc        = nsnull;
  mParams     = nsnull;
	mStrTarget  = nsnull;
  mAction     = ACTION_NONE;
  mShortcutPath = nsnull;
  mDescription  = nsnull;
  mWorkingPath  = nsnull;
  mParams       = nsnull;
  mIcon         = nsnull;

  mTarget = new nsFileSpec(aTarget);
  if(mTarget == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    a1,
                                         nsString&      a2,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
    MOZ_COUNT_CTOR(nsInstallFileOpItem);

    *aReturn      = nsInstall::SUCCESS;
    mIObj         = aInstallObj;
    mCommand      = aCommand;
    mFlags        = 0;
    mAction       = ACTION_NONE;
    mShortcutPath = nsnull;
    mDescription  = nsnull;
    mWorkingPath  = nsnull;
    mParams       = nsnull;
    mIcon         = nsnull;

    switch(mCommand)
    {
        case NS_FOP_DIR_RENAME:
        case NS_FOP_FILE_RENAME:
            mSrc        = new nsFileSpec(a1);
            mTarget     = nsnull;
            mParams     = nsnull;
            mStrTarget  = new nsString(a2);

            if (mSrc == nsnull || mStrTarget == nsnull)
                *aReturn = nsInstall::OUT_OF_MEMORY;

            break;

        case NS_FOP_FILE_EXECUTE:
        default:
            mSrc        = nsnull;
            mTarget     = new nsFileSpec(a1);
            mParams     = new nsString(a2);
            mStrTarget  = nsnull;

            if (mTarget == nsnull || mParams == nsnull)
                *aReturn = nsInstall::OUT_OF_MEMORY;
            break;
    }
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         nsFileSpec&    aShortcutPath,
                                         nsString&      aDescription,
                                         nsFileSpec&    aWorkingPath,
                                         nsString&      aParams,
                                         nsFileSpec&    aIcon,
                                         PRInt32        aIconId,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
    MOZ_COUNT_CTOR(nsInstallFileOpItem);

  *aReturn    = nsInstall::SUCCESS;
  mIObj       = aInstallObj;
  mCommand    = aCommand;
  mIconId     = aIconId;
  mFlags      = 0;
  mSrc        = nsnull;
  mStrTarget  = nsnull;
  mAction     = ACTION_NONE;

  mTarget = new nsFileSpec(aTarget);
  if(mTarget == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

  mShortcutPath = new nsFileSpec(aShortcutPath);
  if(mShortcutPath == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

  mDescription = new nsString(aDescription);
  if(mDescription == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

  mWorkingPath = new nsFileSpec(aWorkingPath);
  if(mWorkingPath == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

  mParams = new nsString(aParams);
  if(mParams == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

  mIcon = new nsFileSpec(aIcon);
  if(mIcon == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;
}

nsInstallFileOpItem::~nsInstallFileOpItem()
{
  if(mSrc)
    delete mSrc;
  if(mTarget)
    delete mTarget;
  if(mStrTarget)
    delete mStrTarget;
  if(mParams)
    delete mParams;
  if(mShortcutPath)
    delete mShortcutPath;
  if(mDescription)
    delete mDescription;
  if(mWorkingPath)
    delete mWorkingPath;
  if(mIcon)
    delete mIcon;

  MOZ_COUNT_DTOR(nsInstallFileOpItem);
}

#ifdef XP_MAC
#pragma mark -
#endif

PRInt32 nsInstallFileOpItem::Complete()
{
  PRInt32 ret = nsInstall::SUCCESS;

  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      ret = NativeFileOpFileCopyComplete();
      break;
    case NS_FOP_FILE_DELETE:
      ret = NativeFileOpFileDeleteComplete(mTarget);
      break;
    case NS_FOP_FILE_EXECUTE:
      ret = NativeFileOpFileExecuteComplete();
      break;
    case NS_FOP_FILE_MOVE:
      ret = NativeFileOpFileMoveComplete();
      break;
    case NS_FOP_FILE_RENAME:
      ret = NativeFileOpFileRenameComplete();
      break;
    case NS_FOP_DIR_CREATE:
      // operation is done in the prepare phase
      break;
    case NS_FOP_DIR_REMOVE:
      ret = NativeFileOpDirRemoveComplete();
      break;
    case NS_FOP_DIR_RENAME:
      ret = NativeFileOpDirRenameComplete();
      break;
    case NS_FOP_WIN_SHORTCUT:
      ret = NativeFileOpWindowsShortcutComplete();
      break;
    case NS_FOP_MAC_ALIAS:
      ret = NativeFileOpMacAliasComplete();
      break;
    case NS_FOP_UNIX_LINK:
      ret = NativeFileOpUnixLink();
      break;
  }

  if ( (ret != nsInstall::SUCCESS) && (ret < nsInstall::GESTALT_INVALID_ARGUMENT || ret > nsInstall::REBOOT_NEEDED) )
    ret = nsInstall::UNEXPECTED_ERROR; /* translate to XPInstall error */
  	
  return ret;
}
  
char* nsInstallFileOpItem::toString()
{
  nsString result;
  char*    resultCString;

  // XXX these hardcoded strings should be replaced by nsInstall::GetResourcedString(id)

    // STRING USE WARNING: perhaps |result| should be an |nsCAutoString| to avoid all this double converting
  
  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      result.AssignWithConversion("Copy File: ");
      result.AppendWithConversion(mSrc->GetNativePathCString());
      result.AppendWithConversion(" to ");
      result.AppendWithConversion(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_DELETE:
      result.AssignWithConversion("Delete File: ");
      result.AppendWithConversion(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_EXECUTE:
      result.AssignWithConversion("Execute File: ");
      result.AppendWithConversion(mTarget->GetNativePathCString());
      result.AppendWithConversion(" ");
      result.Append(*mParams);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_MOVE:
      result.AssignWithConversion("Move File: ");
      result.AppendWithConversion(mSrc->GetNativePathCString());
      result.AppendWithConversion(" to ");
      result.AppendWithConversion(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_RENAME:
      result.AssignWithConversion("Rename File: ");
      result.Append(*mStrTarget);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_DIR_CREATE:
      result.AssignWithConversion("Create Folder: ");
      result.AppendWithConversion(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_DIR_REMOVE:
      result.AssignWithConversion("Remove Folder: ");
      result.AppendWithConversion(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_DIR_RENAME:
      result.AssignWithConversion("Rename Dir: ");
      result.Append(*mStrTarget);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_WIN_SHORTCUT:
      result.AssignWithConversion("Windows Shortcut: ");
      result.AppendWithConversion(*mShortcutPath);
      result.AppendWithConversion("\\");
      result.Append(*mDescription);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_MAC_ALIAS:
      result.AssignWithConversion("Mac Alias: ");
      result.AppendWithConversion(mSrc->GetCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_UNIX_LINK:
      break;
    default:
      result.AssignWithConversion("Unkown file operation command!");
      resultCString = result.ToNewCString();
      break;
  }
  return resultCString;
}

PRInt32 nsInstallFileOpItem::Prepare()
{
  PRInt32 ret = nsInstall::SUCCESS;

  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      ret = NativeFileOpFileCopyPrepare();
      break;
    case NS_FOP_FILE_DELETE:
      ret = NativeFileOpFileDeletePrepare();
      break;
    case NS_FOP_FILE_EXECUTE:
      ret = NativeFileOpFileExecutePrepare();
      break;
    case NS_FOP_FILE_MOVE:
      ret = NativeFileOpFileMovePrepare();
      break;
    case NS_FOP_FILE_RENAME:
      ret = NativeFileOpFileRenamePrepare();
      break;
    case NS_FOP_DIR_CREATE:
      ret = NativeFileOpDirCreatePrepare();
      break;
    case NS_FOP_DIR_REMOVE:
      ret = NativeFileOpDirRemovePrepare();
      break;
    case NS_FOP_DIR_RENAME:
      ret = NativeFileOpDirRenamePrepare();
      break;
    case NS_FOP_WIN_SHORTCUT:
      break;
    case NS_FOP_MAC_ALIAS:
      break;
    case NS_FOP_UNIX_LINK:
      break;
    default:
      break;
  }

  if ( (ret != nsInstall::SUCCESS) && (ret < nsInstall::GESTALT_INVALID_ARGUMENT || ret > nsInstall::REBOOT_NEEDED) )
    ret = nsInstall::UNEXPECTED_ERROR; /* translate to XPInstall error */

  return(ret);
}

void nsInstallFileOpItem::Abort()
{
  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      NativeFileOpFileCopyAbort();
      break;
    case NS_FOP_FILE_DELETE:
      // does nothing
      break;
    case NS_FOP_FILE_EXECUTE:
      // does nothing
      break;
    case NS_FOP_FILE_MOVE:
      NativeFileOpFileMoveAbort();
      break;
    case NS_FOP_FILE_RENAME:
      NativeFileOpFileRenameAbort();
      break;
    case NS_FOP_DIR_CREATE:
      NativeFileOpDirCreateAbort();
      break;
    case NS_FOP_DIR_REMOVE:
      break;
    case NS_FOP_DIR_RENAME:
      NativeFileOpDirRenameAbort();
      break;
    case NS_FOP_WIN_SHORTCUT:
      NativeFileOpWindowsShortcutAbort();
      break;
    case NS_FOP_MAC_ALIAS:
      NativeFileOpMacAliasAbort();
      break;
    case NS_FOP_UNIX_LINK:
      break;
  }
}

/* Private Methods */

/* CanUninstall
* InstallFileOpItem() does not install any files which can be uninstalled,
* hence this function returns false. 
*/
PRBool
nsInstallFileOpItem::CanUninstall()
{
    return PR_FALSE;
}

/* RegisterPackageNode
* InstallFileOpItem() does notinstall files which need to be registered,
* hence this function returns false.
*/
PRBool
nsInstallFileOpItem::RegisterPackageNode()
{
    return PR_FALSE;
}

#ifdef XP_MAC
#pragma mark -
#endif

//
// File operation functions begin here
//
PRInt32
nsInstallFileOpItem::NativeFileOpDirCreatePrepare()
{
  PRInt32 ret = nsInstall::ALREADY_EXISTS;
  PRBool  flagPreExist;

  mAction = nsInstallFileOpItem::ACTION_FAILED;

  if(mTarget->Exists())
    flagPreExist = PR_TRUE;
  else
    flagPreExist = PR_FALSE;

  mTarget->CreateDirectory();

  if(mTarget->Exists() && (PR_FALSE == flagPreExist))
  {
    mAction = nsInstallFileOpItem::ACTION_SUCCESS;
    ret     = nsInstall::SUCCESS;
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirCreateAbort()
{
  if(nsInstallFileOpItem::ACTION_SUCCESS == mAction)
    mTarget->Delete(PR_FALSE);

  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRemovePrepare()
{
  if(mTarget->Exists())
  {
    if(!mTarget->IsFile())
      return nsInstall::SUCCESS;
    else
      return nsInstall::IS_FILE;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRemoveComplete()
{
  mTarget->Delete(mFlags);
  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileRenamePrepare()
{
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  if(mSrc->Exists())
  {
    if(mSrc->IsFile())
    {
      nsFileSpec target;

      mSrc->GetParent(target);
      target += *mStrTarget;

      if(target.Exists())
        return nsInstall::ALREADY_EXISTS;
      else
        return nsInstall::SUCCESS;
    }
    else
      return nsInstall::SOURCE_IS_DIRECTORY;
  }
    
  return nsInstall::SOURCE_DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileRenameComplete()
{
  PRInt32 ret = nsInstall::SUCCESS;

  if(mSrc->Exists())
  {
    if(mSrc->IsFile())
    {
      nsFileSpec target;

      mSrc->GetParent(target);
      target += *mStrTarget;

      if(!target.Exists())
      {
        char* cStrTarget = mStrTarget->ToNewCString();
        if(!cStrTarget)
          return nsInstall::OUT_OF_MEMORY;

        ret = mSrc->Rename(cStrTarget);

        if (cStrTarget)
          Recycle(cStrTarget);
      }
      else
        return nsInstall::ALREADY_EXISTS;
    }
    else
      ret = nsInstall::SOURCE_IS_DIRECTORY;
  }
  else  
    ret = nsInstall::SOURCE_DOES_NOT_EXIST;

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileRenameAbort()
{
  PRInt32    ret  = nsInstall::SUCCESS;
  char*      leafName;
  nsFileSpec newFilename;

  if(!mSrc->Exists())
  {
    mSrc->GetParent(newFilename);
    newFilename += *mStrTarget;
    leafName     = mSrc->GetLeafName();

    ret = newFilename.Rename(leafName);
    
    if(leafName)
      nsCRT::free(leafName);
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileCopyPrepare()
{
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  if(mSrc->Exists())
  {
    if(mSrc->IsFile())
    {
      if(!mTarget->Exists())
        return nsInstall::DOES_NOT_EXIST;
      else if(mTarget->IsFile())
        return nsInstall::IS_FILE;
      else
      {
        nsFileSpec tempVar;

        tempVar = *mTarget;
        tempVar += mSrc->GetLeafName();

        if(tempVar.Exists())
          return nsInstall::ALREADY_EXISTS;
      }

      return nsInstall::SUCCESS;
    }
    else
      return nsInstall::SOURCE_IS_DIRECTORY;
  }
    
  return nsInstall::SOURCE_DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileCopyComplete()
{
  PRInt32 ret;

  mAction = nsInstallFileOpItem::ACTION_FAILED;
  ret     = mSrc->CopyToDir(*mTarget);
  if(nsInstall::SUCCESS == ret)
    mAction = nsInstallFileOpItem::ACTION_SUCCESS;

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileCopyAbort()
{
  nsFileSpec fullTarget = *mTarget;
  PRInt32    ret        = nsInstall::SUCCESS;

  if(nsInstallFileOpItem::ACTION_SUCCESS == mAction)
  {
    fullTarget += mSrc->GetLeafName();
    fullTarget.Delete(PR_FALSE);
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileDeletePrepare()
{
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  if(mTarget->Exists())
  {
    if(mTarget->IsFile())
      return nsInstall::SUCCESS;
    else
      return nsInstall::IS_DIRECTORY;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileDeleteComplete(nsFileSpec *aTarget)
{
  if(aTarget->Exists())
  {
    if(aTarget->IsFile())
      return DeleteFileNowOrSchedule(*aTarget);
    else
      return nsInstall::IS_DIRECTORY;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileExecutePrepare()
{
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  // Also, an absolute path (with filename) must be
  // used.  Xpinstall does not assume files are on the path.
  if(mTarget->Exists())
  {
    if(mTarget->IsFile())
      return nsInstall::SUCCESS;
    else
      return nsInstall::IS_DIRECTORY;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileExecuteComplete()
{
  mTarget->Execute(*mParams);

  // We don't care if it succeeded or not since we
  // don't wait for the process to end anyways.
  // If the file doesn't exist, it was already detected
  // during the prepare phase.
  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMovePrepare()
{
  if(mSrc->Exists())
  {
    if(!mTarget->Exists())
      return nsInstall::DOES_NOT_EXIST;
    else if(mTarget->IsFile())
      return nsInstall::IS_FILE;
    else
    {
      nsFileSpec tempVar;

      tempVar = *mTarget;
      tempVar += mSrc->GetLeafName();

      if(tempVar.Exists())
        return nsInstall::ALREADY_EXISTS;
      else
        return NativeFileOpFileCopyPrepare();
    }
  }

  return nsInstall::SOURCE_DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMoveComplete()
{
  PRInt32 ret = nsInstall::SUCCESS;

  mAction = nsInstallFileOpItem::ACTION_FAILED;
  if(mSrc->Exists())
  {
    if(!mTarget->Exists())
      ret = nsInstall::DOES_NOT_EXIST;
    else
    {
      PRInt32 ret2 = nsInstall::SUCCESS;

      ret = NativeFileOpFileCopyComplete();
      if(nsInstall::SUCCESS == ret)
      {
        mAction = nsInstallFileOpItem::ACTION_SUCCESS;
        ret2    = NativeFileOpFileDeleteComplete(mSrc);

        // We don't care if the value of ret2 is other than
        // REBOOT_NEEDED.  ret takes precedence otherwise.
        if(nsInstall::REBOOT_NEEDED == ret2)
          ret = ret2;
      }
    }
  }
  else
    ret = nsInstall::SOURCE_DOES_NOT_EXIST;

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMoveAbort()
{
  PRInt32 ret = nsInstall::SUCCESS;

  if(nsInstallFileOpItem::ACTION_SUCCESS == mAction)
  {
    if(mSrc->Exists())
      ret = NativeFileOpFileDeleteComplete(mTarget);
    else if(mTarget->Exists())
    {
      nsFileSpec tempVar;
      PRInt32    ret2 = nsInstall::SUCCESS;

      // switch the values of mSrc and mTarget
      // so the original state can be restored.
      // NativeFileOpFileCopyComplete() copies from
      // mSrc to mTarget by default.
      tempVar  = *mTarget;
      *mTarget = *mSrc;
      *mSrc    = tempVar;

      ret = NativeFileOpFileCopyComplete();
      if(nsInstall::SUCCESS == ret)
      {
        ret2 = NativeFileOpFileDeleteComplete(mSrc);

        // We don't care if the value of ret2 is other than
        // REBOOT_NEEDED.  ret takes precedence otherwise.
        if(nsInstall::REBOOT_NEEDED == ret2)
          ret = ret2;
      }
    }
    else
      ret = nsInstall::DOES_NOT_EXIST;
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRenamePrepare()
{
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  if(mSrc->Exists())
  {
    if(!mSrc->IsFile())
    {
      nsFileSpec target;

      mSrc->GetParent(target);
      target += *mStrTarget;

      if(target.Exists())
        return nsInstall::ALREADY_EXISTS;
      else
        return nsInstall::SUCCESS;
    }
    else
      return nsInstall::IS_FILE;
  }
    
  return nsInstall::SOURCE_DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRenameComplete()
{
  PRInt32 ret = nsInstall::SUCCESS;

  if(mSrc->Exists())
  {
    if(!mSrc->IsFile())
    {
      nsFileSpec target;

      mSrc->GetParent(target);
      target += *mStrTarget;

      if(!target.Exists())
      {
        char* cStrTarget = mStrTarget->ToNewCString();
        if(!cStrTarget)
          return nsInstall::OUT_OF_MEMORY;

        ret = mSrc->Rename(cStrTarget);

        if(cStrTarget)
          Recycle(cStrTarget);
      }
      else
        return nsInstall::ALREADY_EXISTS;
    }
    else
      ret = nsInstall::SOURCE_IS_FILE;
  }
  else  
    ret = nsInstall::SOURCE_DOES_NOT_EXIST;

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRenameAbort()
{
  PRInt32    ret = nsInstall::SUCCESS;
  char*      leafName;
  nsFileSpec newDirName;

  if(!mSrc->Exists())
  {
    mSrc->GetParent(newDirName);
    newDirName += *mStrTarget;
    leafName    = mSrc->GetLeafName();

    ret = newDirName.Rename(leafName);
    
    if(leafName)
      nsCRT::free(leafName);
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsShortcutComplete()
{
  PRInt32 ret = nsInstall::SUCCESS;

#ifdef _WINDOWS
  char *cDescription  = mDescription->ToNewCString();
  char *cParams       = mParams->ToNewCString();

  if((cDescription == nsnull) || (cParams == nsnull))
    ret = nsInstall::OUT_OF_MEMORY;
  else
  {
    ret = CreateALink(mTarget->GetNativePathCString(),
                      mShortcutPath->GetNativePathCString(),
                      cDescription,
                      mWorkingPath->GetNativePathCString(),
                      cParams,
                      mIcon->GetNativePathCString(),
                      mIconId);

    if(nsInstall::SUCCESS == ret)
      mAction = nsInstallFileOpItem::ACTION_SUCCESS;
  }

  if(cDescription)
    delete(cDescription);
  if(cParams)
    delete(cParams);
#endif

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsShortcutAbort()
{
#ifdef _WINDOWS
  nsString   shortcutDescription;
  nsFileSpec shortcutTarget;

  shortcutDescription = *mDescription;
  shortcutDescription.Append(".lnk");
  shortcutTarget  = *mShortcutPath;
  shortcutTarget += shortcutDescription;

  NativeFileOpFileDeleteComplete(&shortcutTarget);
#endif

  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpMacAliasComplete()
{

#ifdef XP_MAC
  // XXX gestalt to see if alias manager is around
  
  FSSpec        *fsPtrAlias = mTarget->GetFSSpecPtr();
  AliasHandle   aliasH;
  FInfo         info;
  OSErr         err = noErr;
  
  err = NewAliasMinimal( mSrc->GetFSSpecPtr(), &aliasH );
  if (err != noErr)  // bubble up Alias Manager error
  	return err;
  	
  // create the alias file
  FSpGetFInfo(mSrc->GetFSSpecPtr(), &info);
  FSpCreateResFile(fsPtrAlias, info.fdCreator, info.fdType, smRoman);
  short refNum = FSpOpenResFile(fsPtrAlias, fsRdWrPerm);
  if (refNum != -1)
  {
    UseResFile(refNum);
    AddResource((Handle)aliasH, rAliasType, 0, fsPtrAlias->name);
    ReleaseResource((Handle)aliasH);
    UpdateResFile(refNum);
    CloseResFile(refNum);
  }
  else
    return nsInstall::SUCCESS;
  
  // mark newly created file as an alias file
  FSpGetFInfo(fsPtrAlias, &info);
  info.fdFlags |= kIsAlias;
  FSpSetFInfo(fsPtrAlias, &info);
#endif

  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpMacAliasAbort()
{  
#ifdef XP_MAC
  NativeFileOpFileDeleteComplete(mTarget);
#endif 

  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpUnixLink()
{
  return nsInstall::SUCCESS;
}

