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

#include "nsIXPInstallProgress.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"

#include "nsCOMPtr.h"

#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIXULWindowCallbacks.h"    

#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"


class nsInstallProgressDialog : public nsIXPInstallProgress, public nsIXULWindowCallbacks
{
    public: 

        nsInstallProgressDialog();
        virtual ~nsInstallProgressDialog();
        
        NS_DECL_ISUPPORTS

        
        NS_IMETHOD BeforeJavascriptEvaluation();
        NS_IMETHOD AfterJavascriptEvaluation();
        NS_IMETHOD InstallStarted(const char *UIPackageName);
        NS_IMETHOD ItemScheduled(const char *message);
        NS_IMETHOD InstallFinalization(const char *message, PRInt32 itemNum, PRInt32 totNum);
        NS_IMETHOD InstallAborted();

        // Declare implementations of nsIXULWindowCallbacks interface functions.
        NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
        NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    private:

        nsCOMPtr<nsIDOMXULDocument>  mDocument;
        nsCOMPtr<nsIWebShellWindow>  mWindow;
};
#endif