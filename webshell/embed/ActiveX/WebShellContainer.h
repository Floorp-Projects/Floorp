/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef WEBSHELLCONTAINER_H
#define WEBSHELLCONTAINER_H

// This is the class that handles the XPCOM side of things, callback
// interfaces into the web shell and so forth.

class CWebShellContainer :
		public nsIWebShellContainer,
		public nsIStreamObserver,
		public nsIDocumentLoaderObserver
{
public:
	CWebShellContainer(CMozillaBrowser *pOwner);

protected:
	virtual ~CWebShellContainer();

// Protected members
protected:
	nsString m_sTitle;
	
	CMozillaBrowser *m_pOwner;
	CDWebBrowserEvents1 *m_pEvents1;
	CDWebBrowserEvents2 *m_pEvents2;

public:
	// nsISupports
	NS_DECL_ISUPPORTS

	// nsIWebShellContainer
	NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
	NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
	NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
	NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus);
	NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
	NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
	NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);
	NS_IMETHOD CanCreateNewWebShell(PRBool& aResult);
    NS_IMETHOD SetNewWebShellInfo(const nsString& aName, const nsString& anURL, 
							nsIWebShell* aOpenerShell, PRUint32 aChromeMask,
							nsIWebShell** aNewShell, nsIWebShell** anInnerShell);
    NS_IMETHOD ChildShellAdded(nsIWebShell** aChildShell, nsIContent* frameNode);

	// nsIStreamObserver
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

	// nsIDocumentLoaderObserver 
	NS_IMETHOD OnStartDocumentLoad(nsIURL* aURL, const char* aCommand);
	NS_IMETHOD OnEndDocumentLoad(nsIURL *aUrl, PRInt32 aStatus); 
	NS_IMETHOD OnStartURLLoad(nsIURL* aURL, const char* aContentType, nsIContentViewer* aViewer);   
	NS_IMETHOD OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax); 
	NS_IMETHOD OnStatusURLLoad(nsIURL* aURL, nsString& aMsg); 
	NS_IMETHOD OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus); 
	NS_IMETHOD HandleUnknownContentType( nsIURL *aURL,const char *aContentType,const char *aCommand );		
};

#endif
