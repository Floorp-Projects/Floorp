/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 *
 * Implements Dynamically loadable module manipulation
 *
 * dp Suresh <dp@netscape.com>
 *
 */


#include "wfDlm.h"
#include "libfont.h"

#ifdef NSPR20
#ifdef XP_MAC
#include "probslet.h"
#else
#include "obsolete/probslet.h"
#endif
#endif

wfDlm::wfDlm(const char *fname, const char *str)
: m_state(0), m_filename(NULL), m_lib(NULL), m_dlmInterface(NULL)
{
	if (str && *str)
	{
		reconstruct(str);
	}
	else
	{
		// New file.
		m_filename = CopyString(fname);
		if (sync() < 0)
		{
			// Some error with this file.
		}
	}
}


int wfDlm::status(void)
{
	return(m_state);
}

char *
wfDlm::describe()
{
	char *buf;

	if (!m_filename || !*m_filename)
	{
		return (NULL);
	}
	
	// NO I18N REQUIRED FOR THESE STRINGS
	buf = PR_smprintf("%s;%lld;%lu;", m_filename, m_modifyTime, m_fileSize);
	return (buf);
}

int
wfDlm::reconstruct(const char *str)
{
	char buf[1024];	// XXX this should be dynamic

	if (!str || !*str)
	{
		// Error. No string to reconstruct from.
		m_state = -1;
	}
	else
	{
		str = wf_scanToken(str, buf, sizeof(buf), ";", 1);
		m_filename = CopyString(buf);
		if (isEmpty(m_filename))
		{
			m_state = -1;
		}
		if (*str) str++;

		str = wf_scanToken(str, buf, sizeof(buf), ";", 1);
		if (*buf)
		{
			// XXX PRTime is 64 bit. This will result in data loss.
			// XXX we will fix this once NSPR provides 64bit scanf
			PRInt32 lo = atol(buf);
			LL_I2L(m_modifyTime, lo);
		}
		WF_TRACEMSG(("NF: Modified time converted from string '%s' to %lld\n",
			buf, m_modifyTime));
		if (*str) str++;

		str = wf_scanToken(str, buf, sizeof(buf), ";", 1);
		if (*buf)
		{
			// XXX fileSize is uint32. This should be ok.
			m_fileSize = atol(buf);
		}
		m_state = 1;
	}
	return (m_state);
}

wfDlm::~wfDlm()
{
	finalize();
}


int wfDlm::finalize(void)
{
	unload(1);		// this will reset m_lib and m_dlmInterface
	m_state = 0;
	if (m_filename)
	{
		delete[] m_filename;
		m_filename = NULL;
	}
	return (0);
}

//
// Implementation of public methods
//

const char *
wfDlm::filename()
{
	return (m_filename);
}


int
wfDlm::isChanged()
{
	PRFileInfo finfo;

	if (m_state == 0)
		sync();

	if (m_state < 0)
		return (m_state);
	if (PR_GetFileInfo(m_filename, &finfo) == PR_FAILURE)
		return (-1);
	return (LL_NE(finfo.modifyTime, m_modifyTime) || finfo.size != m_fileSize);
}


int
wfDlm::sync(void)
{
	PRFileInfo finfo;
	if (!m_filename || !*m_filename ||
		(PR_GetFileInfo(m_filename, &finfo) == PR_FAILURE))
	{
		m_state = -1;
		return (m_state);
	}

	m_state = 1;
	m_modifyTime = finfo.modifyTime;
	m_fileSize = finfo.size;
	return (0);
}

int wfDlm::load(void)
{
	if (!m_lib)
	{
#ifdef XP_MAC
		const char *libPath = PR_GetLibraryPath();
		char *libDir = CopyString(m_filename);
		char *libName = strrchr(libDir, '/');
		
		if (libName != NULL)
		{
			libName[1] = '\0';
			PR_SetLibraryPath(libDir);
		}
		
		m_lib = PR_LoadLibrary(m_filename);
		
		if (libName != NULL)
			PR_SetLibraryPath(libPath);
		
		delete[] libDir;

#else
		m_lib = PR_LoadLibrary(m_filename);
#endif
		if (!m_lib)
		{
			m_state = -2;
			return (-1);
		}
		nfdlm_OBJECT_CREATE_PROC *proc =
		  (nfdlm_OBJECT_CREATE_PROC *)findSymbol("dlmFactory_Create");
		if (!proc)
		{
			WF_TRACEMSG(("NF: dlm.load(%s) couldn't find symbol dlmFactory_Create. Skipping dlm.", filename()));
			unload(1);
			m_state = -2;
			return (-1);
		}
		m_dlmInterface = (*proc)(NULL);
		
		if (!m_dlmInterface)
		{
			WF_TRACEMSG(("NF: dlm.load(%s) Couldn't create m_dlmInterface object. Skipping dlm.", filename()));
			unload();
			m_state = -2;
			return (-1);
		}
		else
		{
			WF_TRACEMSG(("NF: dlm.load(%s) successful.", filename()));
		}
	}
	return (0);
}

FARPROC wfDlm::findSymbol(const char *symbol)
{
	// Precondition
	if (status() < 0)
	{
		return (NULL);
	}

	if (!m_lib && load() < 0)
	{
		return (NULL);
	}

#ifndef NSPR20
	return PR_FindSymbol(symbol, m_lib);
#else
	return (FARPROC)PR_FindSymbol(m_lib, symbol);
#endif /* NSPR20 */
}

int wfDlm::unload(int force)
{
	int ret = 0;

	if (status() < 0)
	{
		return (-1);
	}

	if (m_lib)
	{
		// Check if the dlm wants to be unloaded
		if (m_dlmInterface != NULL)
		{
			if (force || nfdlm_OnUnload(m_dlmInterface, NULL) >= 0)
			{
				nfdlm_release(m_dlmInterface, NULL);
				m_dlmInterface = NULL;
			}
		}
		
		if (m_dlmInterface == NULL)
		{
			ret = PR_UnloadLibrary(m_lib);
			m_lib = NULL;
			WF_TRACEMSG(("NF: dlm.unload(%s) returned %d.", filename(), ret));
		}
	}
	return (ret);
}

struct nffp *
wfDlm::createDisplayerObject(struct nffbp *brokerDisplayerInterface)
{
	if (!m_lib)
	{
		load();
	}

	if (status() < 0)
	{
		return (NULL);
	}
	
	struct nffp *fp =
	  (struct nffp *)nfdlm_CreateObject(m_dlmInterface, &nffp_ID,
										(void **)&brokerDisplayerInterface, 1,
										NULL);
	return (fp);
}


//
// Static functions
//
int wfDlm::isEmpty(const char *filename)
{
	PRFileInfo finfo;
	int ret = 0;
	if (!filename || !*filename ||
		(PR_GetFileInfo(filename, &finfo) == PR_FAILURE) ||
		finfo.size == 0)
	{
		ret = 1;
	}
	return (ret);
}

