#ifndef WEBSHELLCONTAINER_H
#define WEBSHELLCONTAINER_H

class CMozillaBrowser;

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

	// nsIStreamObserver
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const nsString &aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg);
};

#endif