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

#ifndef ABOUTURL_H
#define ABOUTURL_H

PUBLIC void NET_InitAboutProtocol(void);

/*
 * About protocol registration stuff
 */

/* Callback for registered about protocols.
 * Should return PR_TRUE if the token was handled */
typedef PRBool (*NET_AboutCB)(const char *token,
			      FO_Present_Types format_out,
			      URL_Struct *URL_s,
			      MWContext *window_id);

/* Wildcard token for FE about protocol handling
 * Otherwise unhandled tokens will match on this */
#define FE_ABOUT_TOKEN ("FEABOUT")

/* About protocol registration function.
 * Returns PR_TRUE if registration succeeds.
 * Registration will fail if token or callback is null,
 * or token has already been registered */
PUBLIC PRBool NET_RegisterAboutProtocol(const char *token,
					NET_AboutCB callback);
				      

#endif /* ABOUTURL_H */
