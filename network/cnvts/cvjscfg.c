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

#include "cvjscfg.h"
#include "cvsimple.h"
#include "prefapi.h"

/*
 * jsc_complete
 */
PRIVATE void
jsc_complete(void* bytes, int32 num_bytes)
{
#ifdef DEBUG_malmer
	extern FILE* real_stderr;

	fwrite(bytes, sizeof(char), num_bytes, real_stderr);
#endif

	if ( bytes ) {
		PREF_EvaluateConfigScript(bytes, num_bytes, NULL, TRUE, TRUE);
	} else {
		/* If failover is ok, read the local cached config */
	}

	/* Need to hash and save to disk here */
}


/*
 * NET_JavascriptConfig
 */
MODULE_PRIVATE NET_StreamClass*
NET_JavascriptConfig(int fmt, void* data_obj, URL_Struct* URL_s, MWContext* w)
{
	return NET_SimpleStream(fmt, (void*) jsc_complete, URL_s, w);
}


