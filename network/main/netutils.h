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
#ifndef NETUTILS_H
#define NETUTILS_H

/* Keep TRACEMSG consistant throughout libnet */
#include "mktrace.h"

/* Used throughout netlib. */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef CONST 
#define CONST const
#endif

PR_BEGIN_EXTERN_C

/*
 * This function takes an error code and associated error data
 * and creates a string containing a textual description of
 * what the error is and why it happened.
 *
 * The returned string is allocated and thus should be freed
 * once it has been used.
 *
 * This function is defined in mkmessag.c.
 */
extern char * NET_ExplainErrorDetails (int code, ...);

extern void NET_Progress(MWContext *context, char *msg);

/* returns true if the functions thinks the string contains
 * HTML
 */
extern PRBool NET_ContainsHTML(char * string, int32 length);

extern void NET_ParseContentTypeHeader(MWContext *context, char *value,
				       URL_Struct *URL_s, PRBool is_http);

typedef struct WritePostDataData WritePostDataData;
extern void NET_free_write_post_data_object(struct WritePostDataData *obj);

/* this function is called repeatably to write
 * the post data body onto the network.
 *
 * Returns negative on fatal error
 * Returns zero when done
 * Returns positive if not yet completed
 */
extern int
NET_WritePostData(MWContext  *context,
                                  URL_Struct *URL_s,
                  PRFileDesc     *sock,
                  void      **write_post_data_data,
                  PRBool     add_crlf_to_line_endings);

extern int NET_UUEncode(unsigned char *src, unsigned char *dst, int srclen);

/*
 * Various character macros
 */

#define NET_TO_UPPER(x) ((((unsigned int) (x)) > 0x7f) ? x : toupper(x))
#define NET_TO_LOWER(x) ((((unsigned int) (x)) > 0x7f) ? x : tolower(x))

#define NET_IS_ALPHA(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isalpha(x))
#define NET_IS_DIGIT(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isdigit(x))
#define NET_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isspace(x))

PR_END_EXTERN_C

#endif /* NETUTILS_H */


