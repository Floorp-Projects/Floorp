#ifndef ACTIVEXPLUGININSTANCE_H
#define ACTIVEXPLUGININSTANCE_H

class CActiveXPluginInstance : public nsIPluginInstance
{
protected:
	virtual ~CActiveXPluginInstance();

	CControlSite	*mControlSite;
	nsPluginWindow	mPluginWindow;

public:
	CActiveXPluginInstance();

	// nsISupports overrides
	NS_DECL_ISUPPORTS

	// nsIEventHandler overrides
    NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);

	// nsIPluginInstance overrides
    NS_IMETHOD Initialize(nsIPluginInstancePeer* peer);
    NS_IMETHOD GetPeer(nsIPluginInstancePeer* *resultingPeer);
    NS_IMETHOD Start(void);
    NS_IMETHOD Stop(void);
    NS_IMETHOD Destroy(void);
    NS_IMETHOD SetWindow(nsPluginWindow* window);
#ifndef NEW_PLUGIN_STREAM_API
    NS_IMETHOD NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result);
#endif
    NS_IMETHOD Print(nsPluginPrint* platformPrint);
#ifndef NEW_PLUGIN_STREAM_API
    NS_IMETHOD URLNotify(const char* url, const char* target, nsPluginReason reason, void* notifyData);
#endif
    NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);
};


#endif