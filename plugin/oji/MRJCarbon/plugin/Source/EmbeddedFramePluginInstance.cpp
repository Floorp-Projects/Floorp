/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
	:	mPeer(NULL), mFrame(NULL)
{
	NS_INIT_REFCNT();
}

EmbeddedFramePluginInstance::~EmbeddedFramePluginInstance()
{
	if (mFrame != NULL)
		delete mFrame;
}

NS_METHOD EmbeddedFramePluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
	mPeer = peer;
	NS_ADDREF(mPeer);

	nsIPluginTagInfo* tagInfo = NULL;
	if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), (void**)&tagInfo) == NS_OK) {
		const char* frameValue = NULL;
		if (tagInfo->GetAttribute("JAVAFRAME", &frameValue) == NS_OK) {
			sscanf(frameValue, "%X", &mFrame);
		}
		if (mFrame != NULL)
			mFrame->setPluginInstance(this);
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

	// assume that Java will release this frame.
	if (mFrame != NULL) {
		mFrame->showHide(false);
		// delete mFrame;
		mFrame = NULL;
	}

	return NS_OK;
}

NS_METHOD EmbeddedFramePluginInstance::SetWindow(nsPluginWindow* pluginWindow)
{
	if (mFrame != NULL) {
		if (pluginWindow != NULL)
			mFrame->setWindow(WindowRef(pluginWindow->window->port));
		else
			mFrame->setWindow(NULL);
	}
	return NS_OK;
}

NS_METHOD EmbeddedFramePluginInstance::HandleEvent(nsPluginEvent* pluginEvent, PRBool* eventHandled)
{
	if (mFrame != NULL)
		*eventHandled = mFrame->handleEvent(pluginEvent->event);
	return NS_OK;
}

void EmbeddedFramePluginInstance::setFrame(EmbeddedFrame* frame)
{
	mFrame = frame;
}

NS_IMPL_ISUPPORTS1(EmbeddedFramePluginInstance, nsIPluginInstance)
