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

#ifndef nsInstallFileOpItem_h__
#define nsInstallFileOpItem_h__

#include "prtypes.h"

#include "nsFileSpec.h"
#include "nsSoftwareUpdate.h"
#include "nsInstallObject.h"
#include "nsInstall.h"

class nsInstallFileOpItem : public nsInstallObject
{
  public:
    /* Public Fields */

    /* Public Methods */
    // used by:
    //   FileOpFileDelete()
    nsInstallFileOpItem(nsInstall*    installObj,
                        PRInt32       aCommand,
                        nsFileSpec&   aTarget,
                        PRInt32       aFlags,
                        PRInt32*      aReturn);

    // used by:
    //   FileOpDirRemove()
    //   FileOpFileCopy()
    //   FileOpFileMove()
    nsInstallFileOpItem(nsInstall*    installObj,
                        PRInt32       aCommand,
                        nsFileSpec&   aSrc,
                        nsFileSpec&   aTarget,
                        PRInt32*      aReturn);

    // used by:
    //   FileOpDirCreate()
    nsInstallFileOpItem(nsInstall*    aInstallObj,
                        PRInt32       aCommand,
                        nsFileSpec&   aTarget,
                        PRInt32*      aReturn);

    // used by:
    //   FileOpDirRename()
    //   FileOpFileExecute()
    //   FileOpFileRename()
    nsInstallFileOpItem(nsInstall*    aInstallObj,
                        PRInt32       aCommand,
                        nsFileSpec&   a1,
                        nsString&     a2,
                        PRInt32*      aReturn);

    virtual ~nsInstallFileOpItem();

    PRInt32       Prepare(void);
    PRInt32       Complete();
    PRUnichar*         toString();
    void          Abort();
    
  /* should these be protected? */
    PRBool        CanUninstall();
    PRBool        RegisterPackageNode();
      
  private:
    
    /* Private Fields */
    
    nsInstall*    mIObj;        // initiating Install object
    nsFileSpec*   mSrc;
    nsFileSpec*   mTarget;
    nsString*     mStrTarget;
    nsString*     mParams;
    long          mFStat;
    PRInt32       mFlags;
    PRInt32       mCommand;
    
    /* Private Methods */

    PRInt32       NativeFileOpDirCreate(nsFileSpec* aTarget);
    PRInt32       NativeFileOpDirRemove(nsFileSpec* aTarget, PRInt32 aFlags);
    PRInt32       NativeFileOpDirRename(nsFileSpec* aSrc, nsString* aTarget);
    PRInt32       NativeFileOpFileCopy(nsFileSpec* aSrc, nsFileSpec* aTarget);
    PRInt32       NativeFileOpFileDelete(nsFileSpec* aTarget, PRInt32 aFlags);
    PRInt32       NativeFileOpFileExecute(nsFileSpec* aTarget, nsString* aParams);
    PRInt32       NativeFileOpFileMove(nsFileSpec* aSrc, nsFileSpec* aTarget);
    PRInt32       NativeFileOpFileRename(nsFileSpec* aSrc, nsString* aTarget);
    PRInt32       NativeFileOpWinShortcutCreate();
    PRInt32       NativeFileOpMacAliasCreate();
    PRInt32       NativeFileOpUnixLinkCreate();

};

#endif /* nsInstallFileOpItem_h__ */

