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

#ifndef COOKIES_H
#define COOKIES_H

#include "mkgeturl.h"

PR_BEGIN_EXTERN_C

/* removes all cookies structs from the cookie list */
extern void
NET_RemoveAllCookies(void);

extern char *
NET_GetCookie(MWContext * context, char * address);

extern void
NET_SetCookieString(MWContext * context,
                     char * cur_url,
                     char * set_cookie_header);

/* saves out the HTTP cookies to disk
 *
 * on entry pass in the name of the file to save
 *
 * returns 0 on success -1 on failure.
 *
 */
extern int NET_SaveCookies(char * filename);

/* reads HTTP cookies from disk
 *
 * on entry pass in the name of the file to read
 *
 * returns 0 on success -1 on failure.
 *
 */
extern int NET_ReadCookies(char * filename);

/* wrapper of NET_SetCookieString for netlib use. We need outformat and url_struct to determine
 * whether we're dealing with inline cookies 
 */
extern void
NET_SetCookieStringFromHttp(FO_Present_Types outputFormat,
                                URL_Struct * URL_s,
                                MWContext * context,
                                char * cur_url,
                                char * set_cookie_header);

PR_END_EXTERN_C

#endif /* COOKIES_H */
