#ifndef ACTIVEXPLUGIN_H
#define ACTIVEXPLUGIN_H

class CActiveXPlugin : public nsIPlugin
{
protected:
	virtual ~CActiveXPlugin();

public:
	CActiveXPlugin();

	// nsISupports overrides
	NS_DECL_ISUPPORTS

	// nsIFactory overrides
	NS_IMETHOD CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);
	NS_IMETHOD LockFactory(PRBool aLock);

	// nsIPlugin overrides
	NS_IMETHOD Initialize();
    NS_IMETHOD Shutdown(void);
    NS_IMETHOD GetMIMEDescription(const char* *resultingDesc);
    NS_IMETHOD GetValue(nsPluginVariable variable, void *value);
};

#endif