/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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