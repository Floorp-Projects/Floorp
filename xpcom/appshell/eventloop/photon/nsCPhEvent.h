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
 *   Jerry Kirk    <Jerry.Kirk@NexwareCorp.com>
 */

#ifndef nsCPhEvent_h__
#define nsCPhEvent_h__

#include "PhT.h"

class nsCPhEvent
{
public:
    nsCPhEvent(PhEvent_t *platformEventData=nsnull);
	virtual ~nsCPhEvent();

    unsigned long GetEventBufferSize();
    unsigned long GetEventSize();
    nsresult ResizeEvent(unsigned long aEventSize);
	 
public:
	PhEvent_t      *m_msg;
    unsigned long   mEventBufferSz;
};

#endif /* nsCPhEvent_h__ */
