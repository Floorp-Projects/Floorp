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

#ifndef nsInstallFileOpItem_h__
#define nsInstallFileOpItem_h__

#include "prtypes.h"

#include "nsIFile.h"
#include "nsSoftwareUpdate.h"
#include "nsInstallObject.h"
#include "nsInstall.h"

class nsInstallFileOpItem : public nsInstallObject
{
  public:
    /* Public Fields */
    enum 
    {
      ACTION_NONE                 = -401,
      ACTION_SUCCESS              = -402,
      ACTION_FAILED               = -403
    };


    /* Public Methods */
    // used by:
    //   FileOpFileDelete()
    nsInstallFileOpItem(nsInstall*    installObj,
                        PRInt32       aCommand,
                        nsIFile*      aTarget,
                        PRInt32       aFlags,
                        PRInt32*      aReturn);

    // used by:
    //   FileOpDirRemove()
    //   FileOpFileCopy()
    //   FileOpFileMove()
    //   FileMacAlias()
    nsInstallFileOpItem(nsInstall*    installObj,
                        PRInt32       aCommand,
                        nsIFile*      aSrc,
                        nsIFile*      aTarget,
                        PRInt32*      aReturn);

    // used by:
    //   FileOpDirCreate()
    nsInstallFileOpItem(nsInstall*    aInstallObj,
                        PRInt32       aCommand,
                        nsIFile*      aTarget,
                        PRInt32*      aReturn);

    // used by:
    //   FileOpDirRename()
    //   FileOpFileExecute()
    //   FileOpFileRename()
    nsInstallFileOpItem(nsInstall*    aInstallObj,
                        PRInt32       aCommand,
                        nsIFile*      a1,
                        nsString&     a2,
                        PRBool        aBlocking,
                        PRInt32*      aReturn);

    // used by:
    //   WindowsShortcut()
    nsInstallFileOpItem(nsInstall*    aInstallObj,
                        PRInt32       aCommand,
                        nsIFile*      aTarget,
                        nsIFile*      aShortcutPath,
                        nsString&     aDescription,
                        nsIFile*      aWorkingPath,
                        nsString&     aParams,
                        nsIFile*      aIcon,
                        PRInt32       aIconId,
                        PRInt32*      aReturn);

    virtual ~nsInstallFileOpItem();

    PRInt32       Prepare(void);
    PRInt32       Complete();
    char*         toString();
    void          Abort();
    
  /* should these be protected? */
    PRBool        CanUninstall();
    PRBool        RegisterPackageNode();
      
  private:
    
    /* Private Fields */
    
    nsInstall*          mIObj;        // initiating Install object
    nsCOMPtr<nsIFile>   mSrc;
    nsCOMPtr<nsIFile>   mTarget;
    nsCOMPtr<nsIFile>   mShortcutPath;
    nsCOMPtr<nsIFile>   mWorkingPath;
    nsCOMPtr<nsIFile>   mIcon;
    nsString*           mDescription;
    nsString*           mStrTarget;
    nsString*           mParams;
    long                mFStat;
    PRInt32             mFlags;
    PRInt32             mIconId;
    PRInt32             mCommand;
    PRInt32             mAction;
    PRBool              mBlocking;
    
    /* Private Methods */

    PRInt32       NativeFileOpDirCreatePrepare();
    PRInt32       NativeFileOpDirCreateAbort();
    PRInt32       NativeFileOpDirRemovePrepare();
    PRInt32       NativeFileOpDirRemoveComplete();
    PRInt32       NativeFileOpDirRenamePrepare();
    PRInt32       NativeFileOpDirRenameComplete();
    PRInt32       NativeFileOpDirRenameAbort();
    PRInt32       NativeFileOpFileCopyPrepare();
    PRInt32       NativeFileOpFileCopyComplete();
    PRInt32       NativeFileOpFileCopyAbort();
    PRInt32       NativeFileOpFileDeletePrepare();
    PRInt32       NativeFileOpFileDeleteComplete(nsIFile *aTarget);
    PRInt32       NativeFileOpFileExecutePrepare();
    PRInt32       NativeFileOpFileExecuteComplete();
    PRInt32       NativeFileOpFileMovePrepare();
    PRInt32       NativeFileOpFileMoveComplete();
    PRInt32       NativeFileOpFileMoveAbort();
    PRInt32       NativeFileOpFileRenamePrepare();
    PRInt32       NativeFileOpFileRenameComplete();
    PRInt32       NativeFileOpFileRenameAbort();
    PRInt32       NativeFileOpWindowsShortcutPrepare();
    PRInt32       NativeFileOpWindowsShortcutComplete();
    PRInt32       NativeFileOpWindowsShortcutAbort();
    PRInt32       NativeFileOpMacAliasPrepare();
    PRInt32       NativeFileOpMacAliasComplete();
    PRInt32       NativeFileOpMacAliasAbort();
    PRInt32       NativeFileOpUnixLink();
    PRInt32       NativeFileOpWindowsRegisterServerPrepare();
    PRInt32       NativeFileOpWindowsRegisterServerComplete();
    PRInt32       NativeFileOpWindowsRegisterServerAbort();

};

#endif /* nsInstallFileOpItem_h__ */

