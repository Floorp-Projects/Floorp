/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
