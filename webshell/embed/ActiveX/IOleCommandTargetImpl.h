/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef IOLECOMMANDIMPL_H
#define IOLECOMMANDIMPL_H

typedef HRESULT (_stdcall *OleCommandProc)(IOleCommandTarget *pCmdTarget, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);

struct OleCommandInfo
{
	ULONG			nCmdID;
	const GUID		*pCmdGUID;
	ULONG			nWindowsCmdID;
	OleCommandProc	pfnCommandProc;
	wchar_t			*szVerbText;
	wchar_t			*szStatusText;
};

// Macros to be placed in any class derived from the IOleCommandTargetImpl
// class. These define what commands are exposed from the object.

#define BEGIN_OLECOMMAND_TABLE() \
	OleCommandInfo *GetCommandTable() \
	{ \
		static OleCommandInfo s_aSupportedCommands[] = \
		{

#define OLECOMMAND_MESSAGE(id, group, cmd, verb, desc) \
			{ id, group, cmd, NULL, verb, desc },

#define OLECOMMAND_HANDLER(id, group, handler, verb, desc) \
			{ id, group, 0, handler, verb, desc },

#define END_OLECOMMAND_TABLE() \
			{ 0, &GUID_NULL, 0, NULL, NULL, NULL } \
		}; \
		return s_aSupportedCommands; \
	};

// Implementation of the IOleCommandTarget interface. The template is
// reasonably generic which is a good thing given how needlessly complicated
// this interface is. Blame Microsoft not me.

template< class T >
class IOleCommandTargetImpl : public IOleCommandTarget
{
	struct OleExecData
	{
		const GUID *pguidCmdGroup;
		DWORD nCmdID;
		DWORD nCmdexecopt;
		VARIANT *pvaIn;
		VARIANT *pvaOut;
	};

public:

	// Query the status of the specified commands (test if is it supported etc.)
	virtual HRESULT STDMETHODCALLTYPE QueryStatus(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[], OLECMDTEXT __RPC_FAR *pCmdText)
	{
		T* pT = static_cast<T*>(this);
		
		if (prgCmds == NULL)
		{
			return E_INVALIDARG;
		}

		OleCommandInfo *pCommands = pT->GetCommandTable();
		NG_ASSERT(pCommands);

		BOOL bCmdGroupFound = FALSE;
		BOOL bTextSet = FALSE;

		// Iterate through list of commands and flag them as supported/unsupported
		for (ULONG nCmd = 0; nCmd < cCmds; nCmd++)
		{
			// Unsupported by default
			prgCmds[nCmd].cmdf = 0;

			// Search the support command list
			for (int nSupported = 0; pCommands[nSupported].pCmdGUID != &GUID_NULL; nSupported++)
			{
				OleCommandInfo *pCI = &pCommands[nSupported];

				if (pguidCmdGroup && pCI->pCmdGUID && memcmp(pguidCmdGroup, pCI->pCmdGUID, sizeof(GUID)) == 0)
				{
					continue;
				}
				bCmdGroupFound = TRUE;

				if (pCI->nCmdID != prgCmds[nCmd].cmdID)
				{
					continue;
				}

				// Command is supported so flag it and possibly enable it
				prgCmds[nCmd].cmdf = OLECMDF_SUPPORTED;
				if (pCI->nWindowsCmdID != 0)
				{
					prgCmds[nCmd].cmdf |= OLECMDF_ENABLED;
				}

				// Copy the status/verb text for the first supported command only
				if (!bTextSet && pCmdText)
				{
					// See what text the caller wants
					wchar_t *pszTextToCopy = NULL;
					if (pCmdText->cmdtextf & OLECMDTEXTF_NAME)
					{
						pszTextToCopy = pCI->szVerbText;
					}
					else if (pCmdText->cmdtextf & OLECMDTEXTF_STATUS)
					{
						pszTextToCopy = pCI->szStatusText;
					}
					
					// Copy the text
					pCmdText->cwActual = 0;
					memset(pCmdText->rgwz, 0, pCmdText->cwBuf * sizeof(wchar_t));
					if (pszTextToCopy)
					{
						// Don't exceed the provided buffer size
						int nTextLen = wcslen(pszTextToCopy);
						if (nTextLen > pCmdText->cwBuf)
						{
							nTextLen = pCmdText->cwBuf;
						}

						wcsncpy(pCmdText->rgwz, pszTextToCopy, nTextLen);
						pCmdText->cwActual = nTextLen;
					}
					
					bTextSet = TRUE;
				}
				break;
			}
		}
		
		// Was the command group found?
		if (!bCmdGroupFound)
		{
			OLECMDERR_E_UNKNOWNGROUP;
		}

		return S_OK;
	}


	// Execute the specified command
	virtual HRESULT STDMETHODCALLTYPE Exec(const GUID __RPC_FAR *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
	{
		T* pT = static_cast<T*>(this);
		BOOL bCmdGroupFound = FALSE;

		OleCommandInfo *pCommands = pT->GetCommandTable();
		NG_ASSERT(pCommands);

		// Search the support command list
		for (int nSupported = 0; pCommands[nSupported].pCmdGUID != &GUID_NULL; nSupported++)
		{
			OleCommandInfo *pCI = &pCommands[nSupported];

			if (pguidCmdGroup && pCI->pCmdGUID && memcmp(pguidCmdGroup, pCI->pCmdGUID, sizeof(GUID)) == 0)
			{
				continue;
			}
			bCmdGroupFound = TRUE;

			if (pCI->nCmdID != nCmdID)
			{
				continue;
			}

			// Command is supported but not implemented
			if (pCI->nWindowsCmdID == 0)
			{
				continue;
			}

			// Send ourselves a WM_COMMAND windows message with the associated
			// identifier and exec data
			OleExecData cData;
			cData.pguidCmdGroup = pguidCmdGroup;
			cData.nCmdID = nCmdID;
			cData.nCmdexecopt = nCmdexecopt;
			cData.pvaIn = pvaIn;
			cData.pvaOut = pvaOut;


			if (pCI->pfnCommandProc)
			{
				pCI->pfnCommandProc(this, pCI->pCmdGUID, pCI->nCmdID, nCmdexecopt, pvaIn, pvaOut);
			}
			else
			{
				HWND hwndTarget = pT->GetCommandTargetWindow();
				if (hwndTarget)
				{
					::SendMessage(hwndTarget, WM_COMMAND, LOWORD(pCI->nWindowsCmdID), (LPARAM) &cData);
				}
			}

			return S_OK;
		}

		// Was the command group found?
		if (!bCmdGroupFound)
		{
			OLECMDERR_E_UNKNOWNGROUP;
		}

		return OLECMDERR_E_NOTSUPPORTED;
	}
};


#endif