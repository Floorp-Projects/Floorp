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
#include <string.h>
#include <time.h>
#include <assert.h>

#include "nsCacheObject.h"
#include "nsCacheTrace.h"

/* 
 * nsCacheObject
 *
 * Gagan Saksena 02/02/98
 * 
 */

const int DEFAULT_EXPIRES = 86400;

nsCacheObject::nsCacheObject():
	m_Flags(INIT), 
	m_Url(new char[1]), 
	m_Etag(new char[1])
{
	Init();
	*m_Url = '\0';
	*m_Etag = '\0';
}

nsCacheObject::~nsCacheObject() 
{
	delete[] m_Url;
	delete[] m_Etag;
}

nsCacheObject::nsCacheObject(const nsCacheObject& another):
	m_Flags(another.m_Flags),
	m_Url(new char[strlen(another.m_Url)+1]), 
	m_Etag(new char[strlen(another.m_Etag)+1])
{

	strcpy(m_Url, another.m_Url);
	strcpy(m_Etag, another.m_Etag);

	m_Hits = another.m_Hits;
	m_LastAccessed = another.m_LastAccessed;
	m_LastModified = another.m_LastModified;
	m_Size = another.m_Size;
}

nsCacheObject::nsCacheObject(const char* i_url):
	m_Flags(INIT), 
		m_Url(new char[strlen(i_url)+1]), 
		m_Etag(new char[1])
{
	Init();
	assert(i_url);
	strcpy(m_Url, i_url);
}

void nsCacheObject::Address(const char* i_url) 
{
	assert(i_url);
	if (m_Url)
		delete[] m_Url;
	m_Url = new char[strlen(i_url) + 1];
	strcpy(m_Url, i_url);
}

#ifdef _DEBUG
int nsCacheObject::IsExpired(void) const
{
	time_t now = time(0);
	return (m_Expires > now) ? 0 : 1; 
}
#endif

void nsCacheObject::Etag(const char* i_etag) 
{
	assert(i_etag && *i_etag);
	if (m_Etag)
		delete[] m_Etag;
	m_Etag = new char[strlen(i_etag) + 1];
	strcpy(m_Etag, i_etag);
}

void nsCacheObject::Init() 
{
	time_t now = time(0);
	m_Expires = now + DEFAULT_EXPIRES;
}

const char*	nsCacheObject::Trace() const
{
	char linebuffer[256];
	char* total = 0;

	sprintf(linebuffer, "nsCacheObject:URL=%s,SIZE=%d,ET=%s,\n\tLM=%d,LA=%d,EXP=%d,HITS=%d\n", 
			m_Url, 
			m_Size,
			m_Etag,
			m_LastModified,
			m_LastAccessed,
			m_Expires,
			m_Hits);
	APPEND(linebuffer);

	return total;
}