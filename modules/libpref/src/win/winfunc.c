/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "mochaapi.h"
#include "mochalib.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "prefapi.h"

extern MochaContext *		m_mochaContext;
extern MochaObject *		m_mochaPrefObject;

int pref_InitInitialObjects() {
    MochaBoolean ok;
    MochaDatum result;
	HRSRC hFound;
	HGLOBAL hRes;
	char * lpBuff = NULL;
	XP_File fp;
	XP_StatStruct stats;
	long fileLength;


#ifdef FROM_RES
	hFound = FindResource(m_hInstance, "init_prefs", RT_RCDATA);
	hRes = LoadResource(m_hInstance, hFound);
	lpBuff = (char *)LockResource(hRes);
    ok = MOCHA_EvaluateBuffer(m_mochaContext,m_mochaPrefObject,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
#else
	_stat("c:\\dog\\initprefs", &stats);

	fileLength = stats.st_size;
	fp = fopen("c:\\dog\\initprefs", "r");

	if (fp) {
		char* readBuf = (char *) malloc(fileLength * sizeof(char));
		if (readBuf) {
			fileLength = XP_FileRead(readBuf, fileLength, fp);
			
			ok = MOCHA_EvaluateBuffer(m_mochaContext,m_mochaPrefObject,
				  readBuf, fileLength, NULL, 0, &result);
			free(readBuf);
			XP_FileClose(fp);
		}
	}
#endif
	return TRUE;
}

char *EncodeBase64Buffer(char *subject, long size) {
	return NULL;
}
char *DecodeBase64Buffer(char *subject) {
	return NULL;
}
