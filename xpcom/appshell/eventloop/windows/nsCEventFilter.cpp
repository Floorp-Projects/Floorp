/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsCRT.h"

#include "nsCEventFilter.h"

//*****************************************************************************
//***    nsCEventFilter: Object Management
//*****************************************************************************

nsCEventFilter::nsCEventFilter(void* platformFilterData)
{
	NS_INIT_REFCNT();

	if(platformFilterData)
		nsCRT::memcpy(&m_filter, platformFilterData, sizeof(m_filter));
	else
		nsCRT::memset(&m_filter, 0, sizeof(m_filter));
}

nsCEventFilter::~nsCEventFilter()
{
}

//*****************************************************************************
// nsCEventFilter::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsCEventFilter, nsIEventFilter, nsIWinEventFilter)

//*****************************************************************************
// nsCEventFilter::nsIEventFilter
//*****************************************************************************   

NS_IMETHODIMP nsCEventFilter::GetNativeData(nsNativeFilterDataType dataType, 
	void** data)
{
	NS_ENSURE_ARG(nsNativeFilterDataTypes::WinFilter == dataType);
	NS_ENSURE_ARG_POINTER(data);

	*data = &m_filter;

	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::SetNativeData(nsNativeFilterDataType dataType,
	void* data)
{
	NS_ENSURE_ARG(nsNativeFilterDataTypes::WinFilter == dataType);

	if(!data)
		nsCRT::memset(&m_filter, 0, sizeof(m_filter));
	else
		nsCRT::memcpy(&m_filter, data, sizeof(m_filter));

	return NS_OK;
}

//*****************************************************************************
// nsCEventFilter::nsIEventFilter
//*****************************************************************************   

NS_IMETHODIMP nsCEventFilter::GetHwnd(void** aHwnd)
{
	NS_ENSURE_ARG_POINTER(aHwnd);
	*aHwnd = m_filter.hWnd;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::SetHwnd(void* aHwnd)
{
	m_filter.hWnd = (HWND)aHwnd;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::GetMsgFilterMin(PRUint32* aMsgFilterMin)
{
	NS_ENSURE_ARG_POINTER(aMsgFilterMin);
	*aMsgFilterMin = m_filter.wMsgFilterMin;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::SetMsgFilterMin(PRUint32 aMsgFilterMin)
{
	m_filter.wMsgFilterMin = aMsgFilterMin;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::GetMsgFilterMax(PRUint32* aMsgFilterMax)
{
	NS_ENSURE_ARG_POINTER(aMsgFilterMax);
	*aMsgFilterMax = m_filter.wMsgFilterMax;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::SetMsgFilterMax(PRUint32 aMsgFilterMax)
{
	m_filter.wMsgFilterMax = aMsgFilterMax;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::GetRemoveFlags(PRUint32* aRemoveFlags)
{
	NS_ENSURE_ARG_POINTER(aRemoveFlags);
	*aRemoveFlags = m_filter.wRemoveFlags;
	return NS_OK;
}

NS_IMETHODIMP nsCEventFilter::SetRemoveFlags(PRUint32 aRemoveFlags)
{
	m_filter.wRemoveFlags = aRemoveFlags;
	return NS_OK;
}