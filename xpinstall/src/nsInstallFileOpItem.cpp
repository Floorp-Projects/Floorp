/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nspr.h"
#include "nsInstall.h"
#include "nsInstallFileOpEnums.h"
#include "nsInstallFileOpItem.h"

#ifdef _WINDOWS
#include "nsWinShortcut.h"
#endif

/* Public Methods */

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         PRInt32        aFlags,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	*aReturn = NS_OK;
    
    mIObj       = aInstallObj;
	mCommand    = aCommand;
	mFlags      = aFlags;
	mSrc        = nsnull;
    mParams     = nsnull;
	mStrTarget  = nsnull;

    mTarget     = new nsFileSpec(aTarget);
    
    if (mTarget == nsnull)
        *aReturn = nsInstall::OUT_OF_MEMORY;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aSrc,
                                         nsFileSpec&    aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	*aReturn = NS_OK;
    
    mIObj       = aInstallObj;
	mCommand    = aCommand;
	mFlags      = 0;
	mParams     = nsnull;
	mStrTarget  = nsnull;
    
    mSrc        = new nsFileSpec(aSrc);
    mTarget     = new nsFileSpec(aTarget);
    
    if (mTarget == nsnull  || mSrc == nsnull)
        *aReturn = nsInstall::OUT_OF_MEMORY;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	*aReturn = NS_OK;
    
    mIObj       = aInstallObj;
	mCommand    = aCommand;
	mFlags      = 0;
	mSrc        = nsnull;
    mParams     = nsnull;
	mStrTarget  = nsnull;
    
    mTarget     = new nsFileSpec(aTarget);

    if (mTarget == nsnull)
        *aReturn = nsInstall::OUT_OF_MEMORY;

}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    a1,
                                         nsString&      a2,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
    *aReturn = NS_OK;
    mIObj       = aInstallObj;
	mCommand    = aCommand;
	mFlags      = 0;

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
	*aReturn    = NS_OK;
  mIObj       = aInstallObj;
  mCommand    = aCommand;
  mIconId     = aIconId;
  mFlags      = 0;
  mSrc        = nsnull;
  mStrTarget  = nsnull;

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
}

PRInt32 nsInstallFileOpItem::Complete()
{
  PRInt32 aReturn = NS_OK;

  switch(mCommand)
  {
    case NS_FOP_DIR_CREATE:
      NativeFileOpDirCreate(mTarget);
      break;
    case NS_FOP_DIR_REMOVE:
      NativeFileOpDirRemove(mTarget, mFlags);
      break;
    case NS_FOP_DIR_RENAME:
      NativeFileOpDirRename(mSrc, mStrTarget);
      break;
    case NS_FOP_FILE_COPY:
      NativeFileOpFileCopy(mSrc, mTarget);
      break;
    case NS_FOP_FILE_DELETE:
      NativeFileOpFileDelete(mTarget, mFlags);
      break;
    case NS_FOP_FILE_EXECUTE:
      NativeFileOpFileExecute(mTarget, mParams);
      break;
    case NS_FOP_FILE_MOVE:
      NativeFileOpFileMove(mSrc, mTarget);
      break;
    case NS_FOP_FILE_RENAME:
      NativeFileOpFileRename(mSrc, mStrTarget);
      break;
    case NS_FOP_WIN_SHORTCUT:
      NativeFileOpWindowsShortcut(mTarget, mShortcutPath, mDescription, mWorkingPath, mParams, mIcon, mIconId);
      break;
    case NS_FOP_MAC_ALIAS:
      NativeFileOpMacAlias();
      break;
    case NS_FOP_UNIX_LINK:
      NativeFileOpUnixLink();
      break;
  }
	return aReturn;
}
  
char* nsInstallFileOpItem::toString()
{
  nsString result;
  char*    resultCString;

  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      result = "Copy File: ";
      result.Append(mSrc->GetNativePathCString());
      result.Append(" to ");
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_DELETE:
      result = "Delete File: ";
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_MOVE:
      result = "Move File: ";
      result.Append(mSrc->GetNativePathCString());
      result.Append(" to ");
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_RENAME:
      result = "Rename File: ";
      result.Append(*mStrTarget);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_DIR_CREATE:
      result = "Create Folder: ";
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_DIR_REMOVE:
      result = "Remove Folder: ";
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_DIR_RENAME:
      result = "Rename Dir: ";
      result.Append(*mStrTarget);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_WIN_SHORTCUT:
      result = "Windows Shortcut: ";
      result.Append(*mShortcutPath);
      result.Append("\\");
      result.Append(*mDescription);
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_MAC_ALIAS:
      break;
    case NS_FOP_UNIX_LINK:
      break;
    default:
      result = "Unkown file operation command!";
      resultCString = result.ToNewCString();
      break;
  }
  return resultCString;
}

PRInt32 nsInstallFileOpItem::Prepare()
{
    // no set-up necessary
    return nsInstall::SUCCESS;
}

void nsInstallFileOpItem::Abort()
{
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

//
// File operation functions begin here
//
PRInt32
nsInstallFileOpItem::NativeFileOpDirCreate(nsFileSpec* aTarget)
{
  aTarget->CreateDirectory();
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRemove(nsFileSpec* aTarget, PRInt32 aFlags)
{
  aTarget->Delete(aFlags);
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpDirRename(nsFileSpec* aSrc, nsString* aTarget)
{
  char* szTarget = aTarget->ToNewCString();

  aSrc->Rename(szTarget);
  
  if (szTarget)
    delete [] szTarget;

  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileCopy(nsFileSpec* aSrc, nsFileSpec* aTarget)
{
  aSrc->Copy(*aTarget);
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileDelete(nsFileSpec* aTarget, PRInt32 aFlags)
{
  aTarget->Delete(aFlags);
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileExecute(nsFileSpec* aTarget, nsString* aParams)
{
  aTarget->Execute(*aParams);
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileMove(nsFileSpec* aSrc, nsFileSpec* aTarget)
{
  aSrc->Move(*aTarget);
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpFileRename(nsFileSpec* aSrc, nsString* aTarget)
{
  char* szTarget = aTarget->ToNewCString();

  aSrc->Rename(szTarget);
  
  if (szTarget)
    delete [] szTarget;

  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWindowsShortcut(nsFileSpec* mTarget, nsFileSpec* mShortcutPath, nsString* mDescription, nsFileSpec * mWorkingPath, nsString* mParams, nsFileSpec* mIcon, PRInt32 mIconId)
{
#ifdef _WINDOWS
  char *cDescription  = mDescription->ToNewCString();
  char *cParams       = mParams->ToNewCString();

  if((cDescription == nsnull) || (cParams == nsnull))
    return nsInstall::OUT_OF_MEMORY;

  CreateALink(mTarget->GetNativePathCString(),
              mShortcutPath->GetNativePathCString(),
              cDescription,
              mWorkingPath->GetNativePathCString(),
              cParams,
              mIcon->GetNativePathCString(),
              mIconId);

  if(cDescription)
    delete(cDescription);
  if(cParams)
    delete(cParams);
#endif

  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpMacAlias()
{
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpUnixLink()
{
  return NS_OK;
}

