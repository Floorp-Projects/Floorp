#ifndef ACTIVEXPLUGININSTANCE_H
#define ACTIVEXPLUGININSTANCE_H

class CActiveXPluginInstance : public nsIPluginInstance
{
protected:
	virtual ~CActiveXPluginInstance();

	CControlSite	*mControlSite;
	nsPluginWindow	 mPluginWindow;

public:
	CActiveXPluginInstance();

	// nsISupports overrides
	NS_DECL_ISUPPORTS

	// nsIPluginInstance overrides
	NS_IMETHOD Initialize(nsIPluginInstancePeer* peer);
    NS_IMETHOD GetPeer(nsIPluginInstancePeer* *resultingPeer);
    NS_IMETHOD Start(void);
    NS_IMETHOD Stop(void);
    NS_IMETHOD Destroy(void);
    NS_IMETHOD SetWindow(nsPluginWindow* window);
    NS_IMETHOD NewStream(nsIPluginStreamListener** listener);
    NS_IMETHOD Print(nsPluginPrint* platformPrint);
    NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);
    NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);
};


#endif