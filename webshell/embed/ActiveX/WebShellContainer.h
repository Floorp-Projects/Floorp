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
		public nsIBrowserWindow,
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

	// nsIBrowserWindow
	NS_IMETHOD Init(nsIAppShell* aAppShell, nsIPref* aPrefs, const nsRect& aBounds, PRUint32 aChromeMask, PRBool aAllowPlugins = PR_TRUE);
	NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
	NS_IMETHOD SizeTo(PRInt32 aWidth, PRInt32 aHeight);
	NS_IMETHOD GetContentBounds(nsRect& aResult);
	NS_IMETHOD GetBounds(nsRect& aResult);
	NS_IMETHOD GetWindowBounds(nsRect& aResult);
	NS_IMETHOD IsIntrinsicallySized(PRBool& aResult);
	NS_IMETHOD SizeWindowTo(PRInt32 aWidth, PRInt32 aHeight);
	NS_IMETHOD SizeContentTo(PRInt32 aWidth, PRInt32 aHeight);
	NS_IMETHOD ShowAfterCreation();
	NS_IMETHOD Show();
	NS_IMETHOD Hide();
	NS_IMETHOD Close();
	NS_IMETHOD ShowModally(PRBool aPrepare);
	NS_IMETHOD SetChrome(PRUint32 aNewChromeMask);
	NS_IMETHOD GetChrome(PRUint32& aChromeMaskResult);
	NS_IMETHOD SetTitle(const PRUnichar* aTitle);
	NS_IMETHOD GetTitle(const PRUnichar** aResult);
	NS_IMETHOD SetStatus(const PRUnichar* aStatus);
	NS_IMETHOD GetStatus(const PRUnichar** aResult);
	NS_IMETHOD SetDefaultStatus(const PRUnichar* aStatus);
	NS_IMETHOD GetDefaultStatus(const PRUnichar** aResult);
	NS_IMETHOD SetProgress(PRInt32 aProgress, PRInt32 aProgressMax);
	NS_IMETHOD ShowMenuBar(PRBool aShow);
	NS_IMETHOD GetWebShell(nsIWebShell*& aResult);
	NS_IMETHOD GetContentWebShell(nsIWebShell **aResult);

	// nsIWebShellContainer
	NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
	NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
	NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
	NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
	NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
						PRBool aVisible,
						nsIWebShell *&aNewWebShell);
	NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
	NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);
	NS_IMETHOD ContentShellAdded(nsIWebShell* aWebShell, nsIContent* frameNode);
	NS_IMETHOD CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
						 PRInt32 aXPos, PRInt32 aYPos, 
						 const nsString& aPopupType, const nsString& anAnchorAlignment,
						 const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup);

#ifdef NECKO
	// nsIStreamObserver
    NS_IMETHOD OnStartRequest(nsIChannel* aChannel, nsISupports* aContext);
    NS_IMETHOD OnStopRequest(nsIChannel* aChannel, nsISupports* aContext, nsresult aStatus, const PRUnichar* aMsg);

	// nsIDocumentLoaderObserver 
	NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
	NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus, nsIDocumentLoaderObserver* aObserver);
	NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer);
	NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax);
	NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);
	NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
	NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType,const char *aCommand );		
#else
	// nsIStreamObserver
    NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg);

	NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
	NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIURI *aUrl, PRInt32 aStatus, nsIDocumentLoaderObserver* aObserver);
	NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType, nsIContentViewer* aViewer);
	NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
	NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg);
	NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus);
	NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader, nsIURI *aURL,const char *aContentType,const char *aCommand );		
#endif
};

#endif
