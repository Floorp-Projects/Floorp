/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "stdafx.h"

///////////////////////////////////////////////////////////////////////////////

CActiveXPluginInstance::CActiveXPluginInstance()
{
	NS_INIT_REFCNT();
	mControlSite = NULL;
}


CActiveXPluginInstance::~CActiveXPluginInstance()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CActiveXPluginInstance)
NS_IMPL_RELEASE(CActiveXPluginInstance)

nsresult CActiveXPluginInstance::QueryInterface(const nsIID& aIID, void** aInstancePtrResult)
{
	NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
	if (nsnull == aInstancePtrResult)
	{
		return NS_ERROR_NULL_POINTER;
	}

	*aInstancePtrResult = NULL;

	if (aIID.Equals(kISupportsIID))
	{
		*aInstancePtrResult = (void*) ((nsIPluginInstance*)this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kIPluginInstanceIID))
	{
		*aInstancePtrResult = (void*) ((nsIPluginInstance*)this);
		AddRef();
		return NS_OK;
	}

	return NS_NOINTERFACE;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPluginInstance overrides

NS_IMETHODIMP CActiveXPluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::Start(void)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::Stop(void)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::Destroy(void)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::SetWindow(nsPluginWindow* window)
{
	if (window)
	{
		mPluginWindow = *window;
	}
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::NewStream(nsIPluginStreamListener** listener)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::Print(nsPluginPrint* platformPrint)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::GetValue(nsPluginInstanceVariable variable, void *value)
{
	return NS_OK;
}

NS_IMETHODIMP CActiveXPluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
	return NS_OK;
}
