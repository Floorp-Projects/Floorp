/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Douglas Turner <dougt@netscape.com>
 */
#ifndef __nsInstallProgressDialog_h__
#define __nsInstallProgressDialog_h__

#include "nscore.h"
#include "nsIXPINotifier.h"
#include "nsIXPIProgressDlg.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"

#include "nsCOMPtr.h"

#include "nsPIXPIManagerCallbacks.h"    

#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"


class nsInstallProgressDialog : public nsIXPIListener,
                                public nsIXPIProgressDlg
{
    public: 

        nsInstallProgressDialog(nsPIXPIManagerCallbacks *aManager);
        virtual ~nsInstallProgressDialog();
        
        NS_DECL_ISUPPORTS

        // implement nsIXPIListener
        NS_DECL_NSIXPILISTENER

        // implement nsIXPIProgressDlg
        NS_DECL_NSIXPIPROGRESSDLG

//        void SetWindow(nsISupports* aWindow);

    protected:
        nsresult setDlgAttribute(const char *id, const char *name, const nsString &value);
        nsresult getDlgAttribute(const char *id, const char *name, nsString &value);

    private:
        nsPIXPIManagerCallbacks*       mManager;
        nsCOMPtr<nsIDOMXULDocument>    mDocument;   // Should this be a weak reference?
        nsCOMPtr<nsIDOMWindowInternal>         mWindow;     // Should this be a weak reference?
};
#endif
