/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* 
*/

#ifndef nsDocLoader_h__
#define nsDocLoader_h__

#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIStreamObserver.h"
#include "nsWeakReference.h"
#include "nsILoadGroup.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"


/****************************************************************************
 * nsDocLoaderImpl implementation...
 ****************************************************************************/

class nsDocLoaderImpl : public nsIDocumentLoader, 
                        public nsIStreamObserver,
                        public nsSupportsWeakReference,
                        public nsIWebProgress
{
public:

    nsDocLoaderImpl();

    nsresult Init();

    // for nsIGenericFactory:
    static NS_IMETHODIMP Create(nsISupports *aOuter, const nsIID &aIID, void **aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOCUMENTLOADER
    // nsIStreamObserver methods: (for observing the load group)
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSIWEBPROGRESS

    // Implementation specific methods...
protected:
    virtual ~nsDocLoaderImpl();

    nsresult SetDocLoaderParent(nsDocLoaderImpl * aLoader);
    nsresult RemoveChildGroup(nsDocLoaderImpl *aLoader);
    void DocLoaderIsEmpty(nsresult aStatus);

    void FireOnStartDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                 nsIURI* aURL);

    void FireOnEndDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                               nsIChannel *aDocChannel,
                               nsresult aStatus);
							   
    void FireOnStartURLLoad(nsDocLoaderImpl* aLoadInitiator,
                            nsIChannel* channel);

    void FireOnEndURLLoad(nsDocLoaderImpl* aLoadInitiator,
                          nsIChannel* channel, nsresult aStatus);

protected:

    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
  
    nsCOMPtr<nsIChannel>       mDocumentChannel;       // [OWNER] ???compare with document
    nsVoidArray                mDocObservers;
    nsISupports*               mContainer;             // [WEAK] it owns me!

    nsDocLoaderImpl*           mParent;                // [WEAK]

    nsCString                  mCommand;

    /*
     * This flag indicates that the loader is loading a document.  It is set
     * from the call to LoadDocument(...) until the OnConnectionsComplete(...)
     * notification is fired...
     */
    PRBool mIsLoadingDocument;

    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsISupportsArray>  mChildList;

    // The following member variables are related to the new nsIWebProgress 
    // feedback interfaces that travis cooked up.
    nsCOMPtr<nsIWebProgressListener> mProgressListener;
    PRInt32 mProgressStatusFlags;
};

#endif /* nsDocLoader_h__ */
