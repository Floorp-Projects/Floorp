/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nspr.h"
#include "nsInstall.h"
#include "nsInstallFileOpEnums.h"
#include "nsInstallFileOpItem.h"
#include "ScheduledTasks.h"
#include "nsProcess.h"
#include "nsReadableUtils.h"

#ifdef _WINDOWS
#include <windows.h>
#include "nsWinShortcut.h"
#endif

#ifdef XP_MAC
#include "Aliases.h"
#include "Gestalt.h"
#include "Resources.h"
#include "TextUtils.h"
#include "script.h"
#include "MoreFilesExtras.h"
#include "nsILocalFileMac.h"
#endif

static NS_DEFINE_CID(kIProcessCID, NS_PROCESS_CID); 

/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsInstallFileOpItem)

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsIFile*       aTarget,
                                         PRInt32        aFlags,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj),
 mTarget(aTarget)
{
    MOZ_COUNT_CTOR(nsInstallFileOpItem);

    *aReturn      = nsInstall::SUCCESS;
    mIObj         = aInstallObj;
    mCommand      = aCommand;
    mFlags        = aFlags;
    mSrc          = nsnull;
    mParams       = nsnull;
    mStrTarget    = nsnull;
    mShortcutPath = nsnull;
    mDescription  = nsnull;
    mWorkingPath  = nsnull;
    mParams       = nsnull;
    mIcon         = nsnull;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsIFile*       aSrc,
                                         nsIFile*       aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj),
 mSrc(aSrc),
 mTarget(aTarget)
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
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsIFile*       aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj),
 mTarget(aTarget)
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
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsIFile*       a1,
                                         nsString&      a2,
                                         PRBool         aBlocking,
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
            mSrc = a1;
            mTarget     = nsnull;
            mParams     = nsnull;
            mStrTarget  = new nsString(a2);

            if (mSrc == nsnull || mStrTarget == nsnull)
                *aReturn = nsInstall::OUT_OF_MEMORY;

            break;

        case NS_FOP_FILE_EXECUTE:
            mBlocking = aBlocking;
        default:
            mSrc        = nsnull;
            mTarget = a1;
            mParams     = new nsString(a2);
            mStrTarget  = nsnull;

    }
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsIFile*       aTarget,
                                         nsIFile*       aShortcutPath,
                                         nsString&      aDescription,
                                         nsIFile*       aWorkingPath,
                                         nsString&      aParams,
                                         nsIFile*       aIcon,
                                         PRInt32        aIconId,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj),
 mTarget(aTarget),
 mShortcutPath(aShortcutPath),
 mWorkingPath(aWorkingPath),
 mIcon(aIcon)
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

  mDescription = new nsString(aDescription);
  if(mDescription == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;

  mParams = new nsString(aParams);
  if(mParams == nsnull)
    *aReturn = nsInstall::OUT_OF_MEMORY;
}

nsInstallFileOpItem::~nsInstallFileOpItem()
{
  //if(mSrc)
  //  delete mSrc;
  //if(mTarget)
  //  delete mTarget;
  if(mStrTarget)
    delete mStrTarget;
  if(mParams)
    delete mParams;
  //if(mShortcutPath)
  //  delete mShortcutPath;
  if(mDescription)
    delete mDescription;
  //if(mWorkingPath)
  //  delete mWorkingPath;
  //if(mIcon)
  //  delete mIcon;

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
    case NS_FOP_WIN_REGISTER_SERVER:
      ret = NativeFileOpWindowsRegisterServerComplete();
      break;
  }

  if ( (ret != nsInstall::SUCCESS) && (ret < nsInstall::GESTALT_INVALID_ARGUMENT || ret > nsInstall::REBOOT_NEEDED) )
    ret = nsInstall::UNEXPECTED_ERROR; /* translate to XPInstall error */
  	
  return ret;
}
  
#define RESBUFSIZE 4096
char* nsInstallFileOpItem::toString()
{
  nsString result;
  char*    resultCString = new char[RESBUFSIZE];
  char*    rsrcVal = nsnull;
  char*    temp;
  char*    srcPath;
  char*    dstPath;

    // STRING USE WARNING: perhaps |result| should be an |nsCAutoString| to avoid all this double converting
  *resultCString = nsnull;
  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      if((mSrc == nsnull) || (mTarget == nsnull))
        break;

      mSrc->GetPath(&srcPath);
      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("CopyFile"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, srcPath, dstPath );
      break;

    case NS_FOP_FILE_DELETE:
      if(mTarget == nsnull)
        break;

      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("DeleteFile"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath );
      break;

    case NS_FOP_FILE_EXECUTE:
      if(mTarget == nsnull)
        break;

      mTarget->GetPath(&dstPath);

      temp = ToNewCString(*mParams);
      if((temp != nsnull) && (*temp != '\0'))
      {
        rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("ExecuteWithArgs"));
        if(rsrcVal != nsnull)
          PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath, temp);
      }
      else
      {
        rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("Execute"));
        if(rsrcVal != nsnull)
          PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath);
      }

      if(temp != nsnull)
        Recycle(temp);

      break;

    case NS_FOP_FILE_MOVE:
      if((mSrc == nsnull) || (mTarget == nsnull))
        break;

      mSrc->GetPath(&srcPath);
      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("MoveFile"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, srcPath, dstPath );
      break;

    case NS_FOP_FILE_RENAME:
      if((mSrc == nsnull) || (mTarget == nsnull))
        break;

      mSrc->GetPath(&srcPath);
      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("RenameFile"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, srcPath, dstPath );
      break;

    case NS_FOP_DIR_CREATE:
      if(mTarget == nsnull)
        break;

      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("CreateFolder"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath );
      break;

    case NS_FOP_DIR_REMOVE:
      if(mTarget == nsnull)
        break;

      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("RemoveFolder"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath );
      break;

    case NS_FOP_DIR_RENAME:
      if((mSrc == nsnull) || (mTarget == nsnull))
        break;

      mSrc->GetPath(&srcPath);
      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("RenameFolder"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, srcPath, dstPath );
      break;

    case NS_FOP_WIN_SHORTCUT:
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("WindowsShortcut"));
      if(rsrcVal != nsnull)
      {
        mShortcutPath->GetPath(&temp);
        result.AssignWithConversion(temp);
        result.AppendWithConversion("\\");
        result.Append(*mDescription);
        dstPath = ToNewCString(result);
        if(dstPath != nsnull)
        {
          PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath );
          Recycle(dstPath);
        }
      }
      break;

    case NS_FOP_MAC_ALIAS:
      if(mTarget == nsnull)
        break;

      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("MacAlias"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath );
      break;

    case NS_FOP_UNIX_LINK:
      break;

    case NS_FOP_WIN_REGISTER_SERVER:
      if(mTarget == nsnull)
        break;

      mTarget->GetPath(&dstPath);
      rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("WindowsRegisterServer"));
      if(rsrcVal != nsnull)
        PR_snprintf(resultCString, RESBUFSIZE, rsrcVal, dstPath );
      break;

    default:
      if(rsrcVal != nsnull)
        resultCString = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("UnknownFileOpCommand"));
      break;
  }

  if(rsrcVal)
    Recycle(rsrcVal);

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
      ret = NativeFileOpWindowsShortcutPrepare();
      break;
    case NS_FOP_MAC_ALIAS:
      ret = NativeFileOpMacAliasPrepare();
      break;
    case NS_FOP_UNIX_LINK:
      break;
    case NS_FOP_WIN_REGISTER_SERVER:
      ret = NativeFileOpWindowsRegisterServerPrepare();
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
    case NS_FOP_WIN_REGISTER_SERVER:
      NativeFileOpWindowsRegisterServerAbort();
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
  PRBool  flagExists;

  mAction = nsInstallFileOpItem::ACTION_FAILED;

  mTarget->Exists(&flagExists);
  if (!flagExists)
  {
      mTarget->Create(1, 0755);
      mAction = nsInstallFileOpItem::ACTION_SUCCESS;
      ret     = nsInstall::SUCCESS;
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirCreateAbort()
{
  if(nsInstallFileOpItem::ACTION_SUCCESS == mAction)
    mTarget->Remove(PR_FALSE);

  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRemovePrepare()
{
  PRBool flagExists, flagIsFile;

  mTarget->Exists(&flagExists);

  if(flagExists)
  {
    mTarget->IsFile(&flagIsFile);
    if(!flagIsFile)
      return nsInstall::SUCCESS;
    else
      return nsInstall::IS_FILE;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRemoveComplete()
{
  mTarget->Remove(mFlags);
  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileRenamePrepare()
{
  PRBool flagExists, flagIsFile;

  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  mSrc->Exists(&flagExists);
  if(flagExists)
  {
    mSrc->IsFile(&flagIsFile);
    if(flagIsFile)
    {
      nsIFile* target;

      mSrc->GetParent(&target);
      nsresult rv =
          target->Append(NS_LossyConvertUCS2toASCII(*mStrTarget).get());
      //90% of the failures during Append will be because the target wasn't in string form
      // which it must be.
      if (NS_FAILED(rv)) return nsInstall::INVALID_ARGUMENTS;

      target->Exists(&flagExists);
      if(flagExists)
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
  PRBool flagExists, flagIsFile;
  
  mSrc->Exists(&flagExists);
  if(flagExists)
  {
    mSrc->IsFile(&flagIsFile);
    if(flagIsFile)
    {
        nsCOMPtr<nsIFile> parent;
        nsCOMPtr<nsIFile> target;

        mSrc->GetParent(getter_AddRefs(parent)); //need parent seprated for use in MoveTo method
        if(parent)
        {
            mSrc->GetParent(getter_AddRefs(target)); //need target for path assembly to check if the file already exists

            if (target)
            {
                target->Append(NS_LossyConvertUCS2toASCII(*mStrTarget).get());
            }
            else
                return nsInstall::UNEXPECTED_ERROR;

            target->Exists(&flagExists);
            if(!flagExists)
            {
                mSrc->MoveTo(parent,
                             NS_LossyConvertUCS2toASCII(*mStrTarget).get());
            }
            else
                return nsInstall::ALREADY_EXISTS;
        }
        else
            return nsInstall::UNEXPECTED_ERROR;
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
  PRInt32   ret  = nsInstall::SUCCESS;
  PRBool    flagExists;
  char*     leafName;
  nsCOMPtr<nsIFile>  newFilename;
  nsCOMPtr<nsIFile>  parent;

  mSrc->Exists(&flagExists);
  if(!flagExists)
  {
    mSrc->GetParent(getter_AddRefs(newFilename));
    if(newFilename)
    {
      mSrc->GetParent(getter_AddRefs(parent));
      if(parent)
      {
        newFilename->Append(NS_LossyConvertUCS2toASCII(*mStrTarget).get());
    
        mSrc->GetLeafName(&leafName);

        newFilename->MoveTo(parent, leafName);
    
        if(leafName)
            nsCRT::free(leafName);
      }
      else
        return nsInstall::UNEXPECTED_ERROR;
    }
    else
      return nsInstall::UNEXPECTED_ERROR;
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileCopyPrepare()
{
  PRBool flagExists, flagIsFile, flagIsWritable;
  char* leafName;
  nsresult rv;
  nsCOMPtr<nsIFile> tempVar;
  nsCOMPtr<nsIFile> targetParent;

  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  
  mSrc->Exists(&flagExists);
  if(flagExists)
  {
    mSrc->IsFile(&flagIsFile);
    if(flagIsFile)
    {
      mTarget->Exists(&flagExists);
      if(!flagExists)
      {
        //Assume mTarget is a file
        //Make sure mTarget's parent exists otherwise, error.
        rv = mTarget->GetParent(getter_AddRefs(targetParent));
        if(NS_FAILED(rv)) return rv;
        rv = targetParent->Exists(&flagExists);
        if(NS_FAILED(rv)) return rv;
        if(!flagExists)
          return nsInstall::DOES_NOT_EXIST;
      }
      else
      {
        mTarget->IsFile(&flagIsFile);
        if(flagIsFile)
        {
          mTarget->IsWritable(&flagIsWritable);
          if (!flagIsWritable)
            return nsInstall::ACCESS_DENIED;
        }
        else
        {
          mTarget->Clone(getter_AddRefs(tempVar));
          mSrc->GetLeafName(&leafName);
          tempVar->Append(leafName);
          tempVar->Exists(&flagExists);
          if(flagExists)
          {
            tempVar->IsWritable(&flagIsWritable);
            if (!flagIsWritable)
              return nsInstall::ACCESS_DENIED;
          }
        }

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
  PRInt32 rv = NS_OK;
  PRBool flagIsFile, flagExists;
  char* leafName;
  nsCOMPtr<nsIFile> parent;
  nsCOMPtr<nsIFile> tempTarget;

  mAction = nsInstallFileOpItem::ACTION_FAILED;

  mTarget->Exists(&flagExists);
  if(flagExists)
  {
    mTarget->IsFile(&flagIsFile); //Target is file that is already on the system
    if (flagIsFile)
    {
      rv = mTarget->Remove(PR_FALSE);
      if (NS_FAILED(rv)) return rv;
      rv = mTarget->GetParent(getter_AddRefs(parent));
      if (NS_FAILED(rv)) return rv;
      rv = mTarget->GetLeafName(&leafName);
      if (NS_FAILED(rv)) return rv;
      rv = mSrc->CopyTo(parent, leafName);
    }
    else //Target is a directory
    {
      rv = mSrc->GetLeafName(&leafName);
      if (NS_FAILED(rv)) return rv;
      rv = mTarget->Clone(getter_AddRefs(tempTarget));
      if (NS_FAILED(rv)) return rv;
      rv = tempTarget->Append(leafName);
      if (NS_FAILED(rv)) return rv;
      tempTarget->Exists(&flagExists);
      if (flagExists)
        tempTarget->Remove(PR_FALSE);

      rv = mSrc->CopyTo(mTarget, leafName);
    }
  }
  else //Target is a file but isn't on the system
  {
    mTarget->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return rv;
    mTarget->GetLeafName(&leafName);
    if (NS_FAILED(rv)) return rv;
    rv = mSrc->CopyTo(parent, leafName);
  }

  if(nsInstall::SUCCESS == rv)
    mAction = nsInstallFileOpItem::ACTION_SUCCESS;

  return rv;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileCopyAbort()
{
  nsCOMPtr<nsIFile> fullTarget;
  PRInt32 ret = nsInstall::SUCCESS;

  mTarget->Clone(getter_AddRefs(fullTarget));
  if(nsInstallFileOpItem::ACTION_SUCCESS == mAction)
  {
    char* leafName;
    mSrc->GetLeafName(&leafName);
    fullTarget->Append(leafName);
    fullTarget->Remove(PR_FALSE);
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileDeletePrepare()
{
  PRBool flagExists, flagIsFile;

  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  
  mTarget->Exists(&flagExists);
  if(flagExists)
  {
    mTarget->IsFile(&flagIsFile);
    if(flagIsFile)
      return nsInstall::SUCCESS;
    else
      return nsInstall::IS_DIRECTORY;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileDeleteComplete(nsIFile *aTarget)
{
  PRBool flagExists, flagIsFile;

  aTarget->Exists(&flagExists);
  if(flagExists)
  {
    aTarget->IsFile(&flagIsFile);
    if(flagIsFile)
      return DeleteFileNowOrSchedule(aTarget);
    else
      return nsInstall::IS_DIRECTORY;
  }
    
  // file went away on its own, not a problem
  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileExecutePrepare()
{
  PRBool flagExists, flagIsFile;
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  // Also, an absolute path (with filename) must be
  // used.  Xpinstall does not assume files are on the path.
  mTarget->Exists(&flagExists);
  if(flagExists)
  {
    mTarget->IsFile(&flagIsFile);
    if(flagIsFile)
      return nsInstall::SUCCESS;
    else
      return nsInstall::IS_DIRECTORY;
  }
    
  return nsInstall::DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileExecuteComplete()
{
  //mTarget->Execute(*mParams);
  //mTarget->Spawn(NS_LossyConvertUCS2toASCII(*mParams).get(), 0);

  char *cParams[1];

  nsCOMPtr<nsIProcess> process = do_CreateInstance(kIProcessCID);

  cParams[0] = nsnull;
  cParams[0] = ToNewCString(*mParams);

  if(cParams[0] == nsnull)
    return nsInstall::OUT_OF_MEMORY;

  nsresult rv = process->Init(mTarget);
  if (NS_FAILED(rv))
  {
    if(cParams[0])
      Recycle(cParams[0]);
    return rv;
  }

  rv = process->Run(mBlocking, (const char **)&cParams[0], 1, nsnull);

  if(cParams[0])
    Recycle(cParams[0]);

  return rv;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMovePrepare()
{
  PRBool flagExists, flagIsFile, flagIsWritable;
  nsresult rv = NS_OK;

  mSrc->Exists(&flagExists);
  if(flagExists)
  {
    mTarget->Exists(&flagExists);
    if(!flagExists)
    {
      //Assume mTarget is a file
      //Make sure mTarget's parent exists otherwise, error.
      nsCOMPtr<nsIFile> targetParent;

      rv = mTarget->GetParent(getter_AddRefs(targetParent));
      if(NS_FAILED(rv)) return rv;
      rv = targetParent->Exists(&flagExists);
      if(NS_FAILED(rv)) return rv;
      if(!flagExists)
        return nsInstall::DOES_NOT_EXIST;
      else
        return NativeFileOpFileCopyPrepare();
    } 
    else
    {
      mTarget->IsFile(&flagIsFile);
      if(flagIsFile)
      {
        mTarget->IsWritable(&flagIsWritable);
        if (!flagIsWritable)
          return nsInstall::ACCESS_DENIED;
      }
      else
      {
        nsCOMPtr<nsIFile> tempVar;
        char* leaf;

        mTarget->Clone(getter_AddRefs(tempVar));
        mSrc->GetLeafName(&leaf);
        tempVar->Append(leaf);

        tempVar->Exists(&flagExists);
        if(flagExists)
        {
          tempVar->IsWritable(&flagIsWritable);
          if (!flagIsWritable)
            return nsInstall::ACCESS_DENIED;
        }
      }
      return NativeFileOpFileCopyPrepare();
    }
  }

  return nsInstall::SOURCE_DOES_NOT_EXIST;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMoveComplete()
{
  PRBool flagExists;
  PRInt32 ret = nsInstall::SUCCESS;

  mAction = nsInstallFileOpItem::ACTION_FAILED;
  mSrc->Exists(&flagExists);
  if(flagExists)
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
  else
    ret = nsInstall::SOURCE_DOES_NOT_EXIST;

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMoveAbort()
{
  PRBool flagExists;
  PRInt32 ret = nsInstall::SUCCESS;

  if(nsInstallFileOpItem::ACTION_SUCCESS == mAction)
  {
    mSrc->Exists(&flagExists);
    if(flagExists)
      ret = NativeFileOpFileDeleteComplete(mTarget);
    else
    {
      mTarget->Exists(&flagExists);
      if(flagExists)
      {
        nsCOMPtr<nsIFile> tempVar;
        PRInt32    ret2 = nsInstall::SUCCESS;

        // switch the values of mSrc and mTarget
        // so the original state can be restored.
        // NativeFileOpFileCopyComplete() copies from
        // mSrc to mTarget by default.
        mTarget->Clone(getter_AddRefs(tempVar));
        mSrc->Clone(getter_AddRefs(mTarget));
        tempVar->Clone(getter_AddRefs(mSrc));

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
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRenamePrepare()
{
  PRBool flagExists, flagIsFile;
  // XXX needs to check file attributes to make sure
  // user has proper permissions to delete file.
  // Waiting on dougt's fix to nsFileSpec().
  // In the meantime, check as much as possible.
  mSrc->Exists(&flagExists);
  if(flagExists)
  {
    mSrc->IsFile(&flagIsFile);
    if(!flagIsFile)
    {
      nsCOMPtr<nsIFile> target;

      mSrc->GetParent(getter_AddRefs(target));
      target->Append(NS_LossyConvertUCS2toASCII(*mStrTarget).get());

      target->Exists(&flagExists);
      if(flagExists)
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
  PRBool flagExists, flagIsFile;
  PRInt32 ret = nsInstall::SUCCESS;

  mSrc->Exists(&flagExists);
  if(flagExists)
  {
    mSrc->IsFile(&flagIsFile);
    if(!flagIsFile)
    {
      nsCOMPtr<nsIFile> target;

      mSrc->GetParent(getter_AddRefs(target));
      target->Append(NS_LossyConvertUCS2toASCII(*mStrTarget).get());

      target->Exists(&flagExists);
      if(!flagExists)
      {
        nsCOMPtr<nsIFile> parent;
        mSrc->GetParent(getter_AddRefs(parent));
        ret = mSrc->MoveTo(parent,
                           NS_LossyConvertUCS2toASCII(*mStrTarget).get());
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
  PRBool     flagExists;
  PRInt32    ret = nsInstall::SUCCESS;
  char*      leafName;
  nsCOMPtr<nsIFile> newDirName;
  nsCOMPtr<nsIFile> parent;

  mSrc->Exists(&flagExists);
  if(!flagExists)
  {
    mSrc->GetParent(getter_AddRefs(newDirName));
    newDirName->Append(NS_LossyConvertUCS2toASCII(*mStrTarget).get());
    mSrc->GetLeafName(&leafName);
    mSrc->GetParent(getter_AddRefs(parent));
    ret = newDirName->MoveTo(parent, leafName);
    
    if(leafName)
      nsCRT::free(leafName);
  }

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsShortcutPrepare()
{
  PRInt32 ret = nsInstall::SUCCESS;

#ifdef _WINDOWS
  nsCOMPtr<nsIFile> tempVar;
  PRBool            flagExists;

  if(mShortcutPath)
  {
    mShortcutPath->Clone(getter_AddRefs(tempVar));
    tempVar->Exists(&flagExists);
    if(!flagExists)
    {
      tempVar->Create(1, 0755);
      tempVar->Exists(&flagExists);
      if(!flagExists)
        ret = nsInstall::ACCESS_DENIED;
    }
    else
    {
      tempVar->Append("NSTestDir");
      tempVar->Create(1, 0755);
      tempVar->Exists(&flagExists);
      if(!flagExists)
        ret = nsInstall::ACCESS_DENIED;
      else
        tempVar->Remove(0);
    }
  }

  if(nsInstall::SUCCESS == ret)
    mAction = nsInstallFileOpItem::ACTION_SUCCESS;

#endif

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsShortcutComplete()
{
  PRInt32 ret = nsInstall::SUCCESS;

#ifdef _WINDOWS
  char *cDescription             = nsnull;
  char *cParams                  = nsnull;
  char *targetNativePathStr      = nsnull;
  char *shortcutNativePathStr    = nsnull;
  char *workingpathNativePathStr = nsnull;
  char *iconNativePathStr        = nsnull;

  if(mDescription)
    cDescription = ToNewCString(*mDescription);
  if(mParams)
    cParams = ToNewCString(*mParams);

  if((cDescription == nsnull) || (cParams == nsnull))
    ret = nsInstall::OUT_OF_MEMORY;
  else
  {
    if(mTarget)
      mTarget->GetPath(&targetNativePathStr);
    if(mShortcutPath)
      mShortcutPath->GetPath(&shortcutNativePathStr);
    if(mWorkingPath)
      mWorkingPath->GetPath(&workingpathNativePathStr);
    if(mIcon)
      mIcon->GetPath(&iconNativePathStr);

    ret = CreateALink(targetNativePathStr,
                      shortcutNativePathStr,
                      cDescription,
                      workingpathNativePathStr,
                      cParams,
                      iconNativePathStr,
                      mIconId);

    if(nsInstall::SUCCESS == ret)
      mAction = nsInstallFileOpItem::ACTION_SUCCESS;
  }

  if(cDescription)
    Recycle(cDescription);
  if(cParams)
    Recycle(cParams);
#endif

  return ret;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsShortcutAbort()
{
#ifdef _WINDOWS
  nsString   shortcutDescription;
  nsCOMPtr<nsIFile> shortcutTarget;

  shortcutDescription = *mDescription;
  shortcutDescription.AppendWithConversion(".lnk");
  mShortcutPath->Clone(getter_AddRefs(shortcutTarget));
  shortcutTarget->Append(NS_LossyConvertUCS2toASCII(shortcutDescription).get());

  NativeFileOpFileDeleteComplete(shortcutTarget);
#endif

  return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpMacAliasPrepare()
{

#ifdef XP_MAC
  nsCOMPtr<nsILocalFileMac> targetFile = do_QueryInterface(mTarget);
  nsCOMPtr<nsILocalFileMac> sourceFile = do_QueryInterface(mSrc);

  FSSpec        fsSource, fsAlias, fsAliasParent, fsCheckSource;
  nsresult      rv;
  OSErr         err;  
  long          aliasDirID;
  Boolean       isDir;
  char          *aliasLeaf;  
   
  rv = sourceFile->GetResolvedFSSpec(&fsSource);
  if (!NS_SUCCEEDED(rv)) return rv;
  rv = targetFile->GetFSSpec(&fsAliasParent); 
  if (!NS_SUCCEEDED(rv)) return rv;
  rv = targetFile->GetAppendedPath(&aliasLeaf);
  if (!NS_SUCCEEDED(rv)) return rv;

  // check if source file exists
  err = FSMakeFSSpec(fsSource.vRefNum, fsSource.parID, fsSource.name, &fsCheckSource);
  if (err != noErr)
    return err;
    
  // construct target alias FSSpec using parent and leaf path
  err = FSpGetDirectoryID(&fsAliasParent, &aliasDirID, &isDir);
  if (err != noErr) 
  {
    if (aliasLeaf)
      nsMemory::Free(aliasLeaf);
    return err;
  }
  c2pstr(aliasLeaf);
  
  // check if file/folder already exists at target
  err = FSMakeFSSpec(fsAliasParent.vRefNum, aliasDirID, (unsigned char *) aliasLeaf, &fsAlias);
  p2cstr((unsigned char *)aliasLeaf);
  if (aliasLeaf)
    nsMemory::Free(aliasLeaf);
  
  // file already exists: delete it before creating an updated alias
  if (err == noErr)
  {
    err = FSpDelete(&fsAlias);
    if (err != noErr)
        return err;
  }      
    
#endif /* XP_MAC */

    return nsInstall::SUCCESS;
}

PRInt32
nsInstallFileOpItem::NativeFileOpMacAliasComplete()
{

#ifdef XP_MAC
  // XXX gestalt to see if alias manager is around
  
  nsCOMPtr<nsILocalFileMac> localFileMacTarget = do_QueryInterface(mTarget);
  nsCOMPtr<nsILocalFileMac> localFileMacSrc = do_QueryInterface(mSrc);
  
  FSSpec        fsSource, fsAliasParent, fsAlias, fsCheckSource;
  AliasHandle   aliasH;
  FInfo         info;
  OSErr         err = noErr;
  nsresult      rv = NS_OK;
  long          aliasDirID;
  Boolean       isDir;
  char          *aliasLeaf;  
  
  // nsIFile sucks so we have to do special hackery here
  rv = localFileMacSrc->GetResolvedFSSpec(&fsSource);
  if (!NS_SUCCEEDED(rv)) return rv;
  rv = localFileMacTarget->GetFSSpec(&fsAliasParent); 
  if (!NS_SUCCEEDED(rv)) return rv;
  rv = localFileMacTarget->GetAppendedPath(&aliasLeaf);
  if (!NS_SUCCEEDED(rv)) return rv;
  
  // check if source file exists
  err = FSMakeFSSpec(fsSource.vRefNum, fsSource.parID, fsSource.name, &fsCheckSource);
  if (err != noErr)
    return err;
    
  // construct target alias FSSpec using parent and leaf path
  err = FSpGetDirectoryID(&fsAliasParent, &aliasDirID, &isDir);
  if (err != noErr) 
  {
    if (aliasLeaf)
      nsMemory::Free(aliasLeaf);
    return err;
  }
  c2pstr(aliasLeaf);
  err = FSMakeFSSpec(fsAliasParent.vRefNum, aliasDirID, (unsigned char *) aliasLeaf, &fsAlias);
  p2cstr((unsigned char *)aliasLeaf);
  if (aliasLeaf)
    nsMemory::Free(aliasLeaf);
  
  // file already exists: delete it before creating an updated alias
  if (err == noErr)
  {
    err = FSpDelete(&fsAlias);
    if (err != noErr)
        return err;
  }
  
  err = NewAliasMinimal( &fsSource, &aliasH );
  if (err != noErr)  // bubble up Alias Manager error
  	return err;
  	
  // create the alias file
  FSpGetFInfo(&fsSource, &info);
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
    return nsInstall::SUCCESS;  // non-fatal so prevent internal abort
  
  // mark newly created file as an alias file
  FSpGetFInfo(&fsAlias, &info);
  info.fdFlags |= kIsAlias;
  FSpSetFInfo(&fsAlias, &info);
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

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsRegisterServerPrepare()
{
  PRInt32 rv = nsInstall::SUCCESS;

#ifdef _WINDOWS
  char      *file = nsnull;
  FARPROC   DllReg;
  HINSTANCE hLib;

  mTarget->GetPath(&file);
  if(file != nsnull)
  {
    if((hLib = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
    {
      if((DllReg = GetProcAddress(hLib, "DllRegisterServer")) == NULL)
        rv = nsInstall::UNABLE_TO_LOCATE_LIB_FUNCTION;

      FreeLibrary(hLib);
    }
    else
      rv = nsInstall::UNABLE_TO_LOAD_LIBRARY;
  }
  else
    rv = nsInstall::UNEXPECTED_ERROR;
#endif

  return(rv);
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsRegisterServerComplete()
{
  PRInt32 rv = nsInstall::SUCCESS;

#ifdef _WINDOWS
  char      *file = nsnull;
  FARPROC   DllReg;
  HINSTANCE hLib;

  mTarget->GetPath(&file);
  if(file != nsnull)
  {
    if((hLib = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
    {
      if((DllReg = GetProcAddress(hLib, "DllRegisterServer")) != NULL)
        DllReg();
      else
        rv = nsInstall::UNABLE_TO_LOCATE_LIB_FUNCTION;

      FreeLibrary(hLib);
    }
    else
      rv = nsInstall::UNABLE_TO_LOAD_LIBRARY;
  }
  else
    rv = nsInstall::UNEXPECTED_ERROR;
#endif

  return(rv);
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsRegisterServerAbort()
{
  PRInt32 rv = nsInstall::SUCCESS;

#ifdef _WINDOWS
  char      *file = nsnull;
  FARPROC   DllUnReg;
  HINSTANCE hLib;

  mTarget->GetPath(&file);
  if(file != nsnull)
  {
    if((hLib = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
    {
      if((DllUnReg = GetProcAddress(hLib, "DllUnregisterServer")) != NULL)
        DllUnReg();
      else
        rv = nsInstall::UNABLE_TO_LOCATE_LIB_FUNCTION;

      FreeLibrary(hLib);
    }
    else
      rv = nsInstall::UNABLE_TO_LOAD_LIBRARY;
  }
  else
    rv = nsInstall::UNEXPECTED_ERROR;
#endif

  return(rv);
}

