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
#ifndef __nsIDownloadProgressDialog_h
#define __nsIDownloadProgressDialog_h

#include "nsIXULWindowCallbacks.h"
#include "nsIStreamListener.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocumentObserver.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIURL.h"
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"

/*------------------------- nsDownloadProgressDialog ---------------------------
| This class implements a "download progress" dialog that transfers a URL      |
| to an output file and indicates the progress to the user as it does so.      |
------------------------------------------------------------------------------*/
class nsDownloadProgressDialog : public nsIXULWindowCallbacks,
                                        nsIStreamListener,
                                        nsIDocumentObserver {
public:
    // Declare implementation of ISupports stuff.
    NS_DECL_ISUPPORTS

    // Declare implementations of nsIXULWindowCallbacks interface functions.
    NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
    NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    // Declare implementations of nsIStreamListener/nsIStreamObserver functions.
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

    // Declare implementations of nsIDocumentObserver functions.
    NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
    NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
    NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                              nsIContent* aContent,
                              nsISupports* aSubContent) { return NS_OK; }
    NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                    nsIContent* aContent1,
                                    nsIContent* aContent2) { return NS_OK; }
    // This one we care about; see implementation below.
    NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                                nsIContent*  aContent,
                                nsIAtom*     aAttribute,
                                PRInt32      aHint);
    NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               PRInt32     aNewIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
    NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet) { return NS_OK; }
    NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                              nsIStyleSheet* aStyleSheet,
                                              PRBool aDisabled) { return NS_OK; }
    NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule,
                                PRInt32 aHint) { return NS_OK; }
    NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
    NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) { return NS_OK; }
    NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; }

    // nsDownloadProgressDialog stuff
    nsDownloadProgressDialog( nsIURL *anInputURL, const nsFileSpec &anOutputFileName );
    ~nsDownloadProgressDialog();
    NS_IMETHOD Show();

protected:
    void OnClose();
    void OnStart();
    void OnStop();

private:
    nsCOMPtr<nsIURL>             mUrl;
    nsCOMPtr<nsIDOMXULDocument>  mDocument;
    nsCOMPtr<nsIWebShellWindow>  mWindow;
    nsOutputFileStream          *mOutput;
    nsFileSpec                   mFileName;
    PRUint32                     mBufLen;
    char *                       mBuffer;
    PRBool                       mStopped;
}; // nsDownloadProgressDialog
    
#endif
