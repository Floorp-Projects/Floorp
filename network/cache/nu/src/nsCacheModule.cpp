/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <xp_core.h>
#include <xp_str.h>

#include "nsCacheModule.h"
#include "nsCacheTrace.h"


/* 
 * nsCacheModule
 *
 * Gagan Saksena 02/02/98
 * 
 */


#define DEFAULT_SIZE 10*0x100000L

nsCacheModule::nsCacheModule():
	m_Size(DEFAULT_SIZE),
	m_pNext(0), 
	m_Entries(0)
{
}

nsCacheModule::nsCacheModule(const PRUint32 i_size):
	m_Size(i_size),
	m_pNext(0),
	m_Entries(0)
{
}

nsCacheModule::~nsCacheModule()
{
	if (m_pNext)
	{
		delete m_pNext;
		m_pNext = 0;
	}
}

const char* nsCacheModule::Trace() const
{
	char linebuffer[128];
	char* total;

	sprintf(linebuffer, "nsCacheModule: Objects = %d\n", Entries());
	
    total = new char[strlen(linebuffer) + 1];
    strcpy(total, linebuffer);

	return total;
}