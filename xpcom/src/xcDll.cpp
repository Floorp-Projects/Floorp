/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/* Dll
 *
 * Abstraction of a Dll. Stores modifiedTime and size for easy detection of
 * change in dll.
 *
 * dp Suresh <dp@netscape.com>
 */

#include "xcDll.h"
#include "plstr.h"	// strdup and strfree

Dll::Dll(const char *libFullPath) : m_instance(NULL), m_status(DLL_OK),
	m_fullpath(NULL), m_lastModTime(0), m_size(0)
{
	if (libFullPath == NULL)
	{
		m_status == DLL_INVALID_PARAM;
		return;
	}
	m_fullpath = PL_strdup(libFullPath);
	if (m_fullpath == NULL)
	{
		// No more memory
		m_status = DLL_NO_MEM;
		return;
	}
	PRFileInfo64 statinfo;
	if (PR_GetFileInfo64(m_fullpath, &statinfo) != PR_SUCCESS)
	{
		m_status = DLL_STAT_ERROR;
		return;
	}
	if (statinfo.type != PR_FILE_FILE)
	{
		// Not a file. Cant work with it.
		m_status = DLL_NOT_FILE;
		return;
	}
	m_size = statinfo.size;
	m_lastModTime = statinfo.modifyTime;
	m_status = DLL_OK;			
}

Dll::~Dll(void)
{
	if (m_instance != NULL)
		Unload();
	if (m_fullpath != NULL) PL_strfree(m_fullpath);
	m_fullpath = NULL;
}

PRBool Dll::Load(void)
{
	if (m_status != DLL_OK)
	{
		return (PR_FALSE);
	}
	if (m_instance != NULL)
	{
		// Already loaded
		return (PR_TRUE);
	}
	m_instance = PR_LoadLibrary(m_fullpath);
	return ((m_instance == NULL) ? PR_FALSE : PR_TRUE);
	
}

PRBool Dll::Unload(void)
{
	if (m_status != DLL_OK || m_instance == NULL)
		return (PR_FALSE);
	PRStatus ret = PR_UnloadLibrary(m_instance);
	if (ret == PR_SUCCESS)
	{
		m_instance = NULL;
		return (PR_TRUE);
	}
	else
		return (PR_FALSE);
}

void * Dll::FindSymbol(const char *symbol)
{
	if (symbol == NULL)
		return (NULL);
	
	// If not already loaded, load it now.
	if (Load() != PR_TRUE)
		return (NULL);

	return (PR_FindSymbol(m_instance, symbol));
}
