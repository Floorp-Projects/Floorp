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

#ifndef COOKIES_H
#define COOKIES_H

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

/* create an HTML stream and push a bunch of HTML about cookies */
extern void
NET_DisplayCookieInfoAsHTML(ActiveEntry * cur_entry);

/* wrapper of NET_SetCookieString for netlib use. We need outformat and url_struct to determine
 * whether we're dealing with inline cookies 
 */
extern void
NET_SetCookieStringFromHttp(FO_Present_Types outputFormat,
                                URL_Struct * URL_s,
                                MWContext * context,
                                char * cur_url,
                                char * set_cookie_header);

#endif /* COOKIES_H */
