/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "rl.h"
#include "plstr.h"

PR_IMPLEMENT(void)
RLInit(void)
{
}

PR_IMPLEMENT(void)
RLTrace(const char* fmtstr, ...)
{
    char* str;
    va_list ap;
    va_start(ap, fmtstr);

    str = PR_vsmprintf(fmtstr, ap);
    if (str) {
        PR_Write(PR_STDERR, str, PL_strlen(str));
        PR_smprintf_free(str);
    }
}
