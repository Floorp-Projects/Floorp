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

#include "plstr.h"

#define RDF_STRLEN(s)            PL_strlen((s))
#define RDF_STRCMP(s1,s2)        PL_strcmp((s1),(s2))

/* #define RDF_STRDUP(s)            PL_strdup((s)) evil? */

#define RDF_STRCHR(s,c)          PL_strchr((s),(c))
#define RDF_STRRCHR(s,c)         PL_strrchr((s),(c))
#define RDF_STRCAT(dest,src)     PL_strcat((dest),(src))
#define RDF_STRCASECMP(s1,s2)    PL_strcasecmp((s1),(s2))
#define RDF_STRNCASECMP(s1,s2,n) PL_strncasecmp((s1),(s2),(n))
#define RDF_STRCASESTR(s1,s2)    PL_strcasestr((s1),(s2))

