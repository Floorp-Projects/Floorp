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
 *   Jerry Kirk    <Jerry.Kirk@NexwareCorp.com>
 */

#ifndef nsCEventFilter_h__
#define nsCEventFilter_h__

#include "PhT.h"
#include "nsIEventFilter.h"
#include "nsCPhFilter.h"

class nsCEventFilter : public nsIEventFilter
{
public:
	nsCEventFilter(void* platformFilterData=nsnull);
	
	NS_DECL_ISUPPORTS

	NS_DECL_NSIEVENTFILTER

protected:
	virtual ~nsCEventFilter();

protected:
	nsCPhFilter	m_filter;
};

#endif /* nsCEventFilter_h__ */
