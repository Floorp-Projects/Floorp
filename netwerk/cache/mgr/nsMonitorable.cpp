/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* 
 * This is the implementation of the nsMonitorable::MonitorLocker class.
 * Although the inlines work ok elsewhere, IRIX chokes on these. Hence
 * moving these to a cpp.
 * -Gagan Saksena 09/15/98 
 */

#include "nsMonitorable.h"

nsMonitorable::MonitorLocker::MonitorLocker(nsMonitorable* i_pThis):
	m_pMonitorable(i_pThis)
{
	if (m_pMonitorable) m_pMonitorable->Lock();
}

nsMonitorable::MonitorLocker::~MonitorLocker()
{
	if (m_pMonitorable) m_pMonitorable->Unlock();
}
