/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Douglas Turner <dougt@netscape.com>
 */
#ifndef __nsInstallProgressDialog_h__
#define __nsInstallProgressDialog_h__

#include "nsIPrefMigrationProgress.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"

#include "nsCOMPtr.h"

#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIXULWindowCallbacks.h"    

#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"


class nsPrefMigrationProgressDialog : public nsIPrefMigrationProgress, public nsIXULWindowCallbacks
{
    public: 

        nsPrefMigrationProgressDialog();
        virtual ~nsPrefMigrationProgressDialog();
        
        NS_DECL_ISUPPORTS
        NS_DECL_NSIPREFMIGRATIONPROGRESS

        NS_IMETHOD CreateProfileProgressDialog();
        NS_IMETHOD MigrationStarted(const char *UIPackageName);
        NS_IMETHOD IncrementProgressBar();

        NS_IMETHOD KillProfileProgressDialog();

        //NS_IMETHOD InstallFinalization(const char *message, PRInt32 itemNum, PRInt32 totNum);
        //NS_IMETHOD InstallAborted();

        // Declare implementations of nsIXULWindowCallbacks interface functions.
        NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
        NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    private:

        nsCOMPtr<nsIDOMXULDocument>  mDocument;		// is this really owned?
        nsCOMPtr<nsIWebShellWindow>  mWindow;			// is this really owned?
};
#endif
