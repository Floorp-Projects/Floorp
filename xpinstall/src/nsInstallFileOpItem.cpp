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

/* Public Methods */

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         PRInt32        aFlags,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	mIObj    = aInstallObj;
	mCommand = aCommand;
	mFlags   = aFlags;
	mSrc     = nsnull;
  mParams  = nsnull;
	mTarget  = new nsFileSpec(aTarget);

  *aReturn = NS_OK;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aSrc,
                                         nsFileSpec&    aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	mIObj    = aInstallObj;
	mCommand = aCommand;
	mFlags   = 0;
	mSrc     = new nsFileSpec(aSrc);
  mParams  = nsnull;
	mTarget  = new nsFileSpec(aTarget);

  *aReturn = NS_OK;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	mIObj    = aInstallObj;
	mCommand = aCommand;
	mFlags   = 0;
	mSrc     = nsnull;
  mParams  = nsnull;
	mTarget  = new nsFileSpec(aTarget);

  *aReturn = NS_OK;
}

nsInstallFileOpItem::nsInstallFileOpItem(nsInstall*     aInstallObj,
                                         PRInt32        aCommand,
                                         nsFileSpec&    aTarget,
                                         nsString&      aParams,
                                         PRInt32*       aReturn)
:nsInstallObject(aInstallObj)
{
	mIObj    = aInstallObj;
	mCommand = aCommand;
	mFlags   = 0;
	mSrc     = nsnull;
  mParams  = new nsString(aParams);
	mTarget  = new nsFileSpec(aTarget);

  *aReturn = NS_OK;
}

nsInstallFileOpItem::~nsInstallFileOpItem()
{
  if(mSrc)
    delete mSrc;
  if(mTarget)
    delete mTarget;
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
      NativeFileOpDirRename(mSrc, mTarget);
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
      NativeFileOpFileRename(mSrc, mTarget);
      break;
    case NS_FOP_WIN_SHORTCUT_CREATE:
      NativeFileOpWinShortcutCreate();
      break;
    case NS_FOP_MAC_ALIAS_CREATE:
      NativeFileOpMacAliasCreate();
      break;
    case NS_FOP_UNIX_LINK_CREATE:
      NativeFileOpUnixLinkCreate();
      break;
  }
	return aReturn;
}
  
float nsInstallFileOpItem::GetInstallOrder()
{
	return 3;
}

char* nsInstallFileOpItem::toString()
{
	nsString result;
  char*    resultCString;

  switch(mCommand)
  {
    case NS_FOP_FILE_COPY:
      result = "Copy file: ";
      result.Append(mSrc->GetNativePathCString());
      result.Append(" to ");
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_DELETE:
      result = "Delete file: ";
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_MOVE:
      result = "Move file: ";
      result.Append(mSrc->GetNativePathCString());
      result.Append(" to ");
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
      break;
    case NS_FOP_FILE_RENAME:
      result = "Rename file: ";
      result.Append(mTarget->GetNativePathCString());
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
    case NS_FOP_WIN_SHORTCUT_CREATE:
      break;
    case NS_FOP_MAC_ALIAS_CREATE:
      break;
    case NS_FOP_UNIX_LINK_CREATE:
      break;
    case NS_FOP_FILE_SET_STAT:
      result = "Set file stat: ";
      result.Append(mTarget->GetNativePathCString());
      resultCString = result.ToNewCString();
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
	return NULL;
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
    return FALSE;
}

/* RegisterPackageNode
* InstallFileOpItem() does notinstall files which need to be registered,
* hence this function returns false.
*/
PRBool
nsInstallFileOpItem::RegisterPackageNode()
{
    return FALSE;
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
nsInstallFileOpItem::NativeFileOpDirRename(nsFileSpec* aSrc, nsFileSpec* aTarget)
{
  aSrc->Rename(*aTarget);
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
nsInstallFileOpItem::NativeFileOpFileRename(nsFileSpec* aSrc, nsFileSpec* aTarget)
{
  aSrc->Rename(*aTarget);
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpWinShortcutCreate()
{
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpMacAliasCreate()
{
  return NS_OK;
}

PRInt32
nsInstallFileOpItem::NativeFileOpUnixLinkCreate()
{
  return NS_OK;
}

