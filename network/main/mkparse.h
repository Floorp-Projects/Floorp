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
#ifndef MKPARSE_H
#define MKPARSE_H

/*
 *  Returns the number of the month given or -1 on error.
 */
extern int NET_MonthNo (char * month);

/* accepts an RFC 850 or RFC 822 date string and returns a
 * time_t
 */
extern time_t NET_ParseDate(char *date_string);

/* remove front and back white space
 * modifies the original string
 */
extern char * NET_Strip (char * s);

/* find a character in a fixed length buffer */
extern char * strchr_in_buf(char *string, int32 string_len, char find_char);

#endif  /* MKPARSE_H */
