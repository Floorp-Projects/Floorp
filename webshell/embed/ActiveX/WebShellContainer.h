#ifndef WEBSHELLCONTAINER_H
#define WEBSHELLCONTAINER_H

// This is the class that handles the XPCOM side of things, callback
// interfaces into the web shell and so forth.

class CWebShellContainer :
		public nsIWebShellContainer,
		public nsIStreamObserver
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
	NS_IMETHOD NewWebShell(nsIWebShell *&aNewWebShell);
	NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell);

	// nsIStreamObserver
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const nsString &aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg);
};

#endif