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

/*
	EmbeddedFramePluginInstance.cpp
 */

#include "EmbeddedFramePluginInstance.h"
#include "EmbeddedFrame.h"
#include "MRJPlugin.h"

#include "nsIPluginInstancePeer.h"
#include "nsIPluginTagInfo.h"

#include <stdio.h>

EmbeddedFramePluginInstance::EmbeddedFramePluginInstance()
	:	mPeer(NULL), mParentInstance(NULL), mFrame(NULL)
{
	NS_INIT_REFCNT();
}

EmbeddedFramePluginInstance::~EmbeddedFramePluginInstance()
{
	if (mFrame != NULL)
		delete mFrame;
}

NS_IMPL_ISUPPORTS(EmbeddedFramePluginInstance, nsIPluginInstance::GetIID())

NS_METHOD EmbeddedFramePluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
	mPeer = peer;
	NS_ADDREF(mPeer);

	nsIPluginTagInfo* tagInfo = NULL;
	if (mPeer->QueryInterface(nsIPluginTagInfo::GetIID(), &tagInfo) == NS_OK) {
		const char* frameValue = NULL;
		if (tagInfo->GetAttribute("FRAME", &frameValue) == NS_OK) {
			sscanf(frameValue, "%X", &mFrame);
		}
		NS_RELEASE(tagInfo);
	}

	return NS_OK;
}

NS_METHOD EmbeddedFramePluginInstance::GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
	if (mPeer != NULL) {
		*resultingPeer = mPeer;
		mPeer->AddRef();
	}
	return NS_OK;
}

NS_METHOD EmbeddedFramePluginInstance::Destroy()
{
	NS_IF_RELEASE(mPeer);
	NS_IF_RELEASE(mParentInstance);
	if (mFrame != NULL) {
		delete mFrame;
		mFrame = NULL;
	}
	return NS_OK;
}

NS_METHOD EmbeddedFramePluginInstance::SetWindow(nsPluginWindow* pluginWindow)
{
	if (pluginWindow != NULL)
		mFrame->setWindow(WindowRef(pluginWindow->window->port));
	else
		mFrame->setWindow(NULL);
	return NS_OK;
}

NS_METHOD EmbeddedFramePluginInstance::HandleEvent(nsPluginEvent* pluginEvent, PRBool* eventHandled)
{
	if (mFrame != NULL)
		*eventHandled = mFrame->handleEvent(pluginEvent->event);
	return NS_OK;
}
